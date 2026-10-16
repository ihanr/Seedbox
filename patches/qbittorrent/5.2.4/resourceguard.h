// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Seedbox
{
struct Pressure
{
    bool valid = false;
    double availablePercent = 0;
    double memorySome = 0;
    double ioSome = 0;
};

namespace ResourceGuardDetail
{
inline bool percentage(double value) { return std::isfinite(value) && value >= 0 && value <= 100; }

inline bool unsignedNumber(const std::string &text, std::uint64_t &value)
{
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc() && result.ptr == text.data() + text.size();
}

inline bool psi(const std::string &path, double &value)
{
    std::ifstream file(path);
    std::string line, key, token;
    bool found = false;
    while (std::getline(file, line))
    {
        std::istringstream fields(line);
        if (!(fields >> key) || key != "some") continue;
        while (fields >> token)
        {
            if (token.compare(0, 6, "avg10=") != 0) continue;
            if (found) return false;
            const char *begin = token.data() + 6;
            const char *end = token.data() + token.size();
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc() || result.ptr != end || !percentage(value)) return false;
            found = true;
        }
    }
    return found && !file.bad();
}

inline bool groupPath(const std::string &path, std::string &group)
{
    std::ifstream file(path);
    std::string line, extra;
    if (!std::getline(file, line) || line.compare(0, 4, "0::/") != 0) return false;
    while (std::getline(file, extra)) if (!extra.empty()) return false;
    group = line.substr(3);
    for (const char *marker : {"docker", "kubepods", "containerd", "libpod", "lxc"})
        if (group.find(marker) != std::string::npos) return false;
    std::istringstream parts(group);
    while (std::getline(parts, extra, '/')) if (extra == ".." || extra == ".") return false;
    return !file.bad();
}

inline bool unlimited(const std::string &path)
{
    std::ifstream file(path);
    std::string value, extra;
    return (file >> value) && value == "max" && !(file >> extra) && !file.bad();
}

inline bool ordinaryHost(const std::string &procRoot, const std::string &cgroupRoot)
{
    std::string group, initGroup;
    if (!groupPath(procRoot + "/self/cgroup", group) || !groupPath(procRoot + "/1/cgroup", initGroup)
        || !std::ifstream(cgroupRoot + "/cgroup.controllers")) return false;
    // ponytail: unsupported containers stay observation-only; hidden ancestors cannot be proven unlimited.
    for (const char *marker : {".dockerenv", "run/.containerenv", "run/systemd/container"})
    {
        std::error_code error;
        if (std::filesystem::exists(procRoot + "/self/root/" + marker, error) || error) return false;
    }
    std::ifstream mounts(procRoot + "/self/mountinfo");
    std::string line, id, parent, device, root, mount, options, type;
    bool rootSeen = false;
    while (std::getline(mounts, line))
    {
        std::istringstream fields(line);
        if (!(fields >> id >> parent >> device >> root >> mount >> options)) return false;
        if (mount != "/") continue;
        while (fields >> options && options != "-") {}
        if (!(fields >> type) || type == "overlay" || type == "aufs") return false;
        rootSeen = true;
    }
    if (!rootSeen || mounts.bad()) return false;
    while (true)
    {
        const std::string current = cgroupRoot + (group == "/" ? "" : group);
        std::error_code maxError, highError;
        const bool hasMax = std::filesystem::exists(current + "/memory.max", maxError);
        const bool hasHigh = std::filesystem::exists(current + "/memory.high", highError);
        if (maxError || highError) return false;
        // The cgroup v2 mount root normally has neither limit file.
        if (!(group == "/" && !hasMax && !hasHigh)
            && (!hasMax || !hasHigh || !unlimited(current + "/memory.max") || !unlimited(current + "/memory.high"))) return false;
        if (group == "/") break;
        const auto slash = group.find_last_of('/');
        group = slash == 0 ? "/" : group.substr(0, slash);
    }
    return true;
}
}

inline Pressure readPressure(const std::string &procRoot = "/proc", const std::string &cgroupRoot = "/sys/fs/cgroup")
{
    Pressure pressure;
    if (!ResourceGuardDetail::ordinaryHost(procRoot, cgroupRoot)) return pressure;
    std::ifstream memory(procRoot + "/meminfo");
    std::string line, key, value, unit, extra;
    std::uint64_t total = 0, available = 0;
    bool hasTotal = false, hasAvailable = false;
    while (std::getline(memory, line))
    {
        std::istringstream fields(line);
        if (!(fields >> key) || (key != "MemTotal:" && key != "MemAvailable:")) continue;
        const bool isTotal = key == "MemTotal:";
        bool &found = isTotal ? hasTotal : hasAvailable;
        if (found || !(fields >> value >> unit) || unit != "kB" || (fields >> extra)
            || !ResourceGuardDetail::unsignedNumber(value, isTotal ? total : available)) return pressure;
        found = true;
    }
    if (!hasTotal || !hasAvailable || total == 0 || available > total || memory.bad()
        || !ResourceGuardDetail::psi(procRoot + "/pressure/memory", pressure.memorySome)
        || !ResourceGuardDetail::psi(procRoot + "/pressure/io", pressure.ioSome)) return pressure;
    pressure.availablePercent = 100.0 * static_cast<double>(available) / static_cast<double>(total);
    pressure.valid = true;
    return pressure;
}

class ResourceGuard
{
public:
    explicit ResourceGuard(bool enabled = false) : m_enabled(enabled) {}
    bool enabled() const { return m_enabled; }
    bool shouldSample(std::int64_t nowSeconds)
    {
        if (!m_enabled || nowSeconds < 0) return false;
        if (m_lastSample >= 0 && nowSeconds >= m_lastSample && nowSeconds - m_lastSample < 10) return false;
        m_lastSample = nowSeconds;
        return true;
    }
    bool update(std::int64_t nowSeconds, const Pressure &pressure, bool uploading)
    {
        if (!m_enabled) return false;
        if (nowSeconds < 0 || (m_lastUpdate >= 0 && (nowSeconds <= m_lastUpdate || nowSeconds - m_lastUpdate > 30)))
            m_pressureSince = m_healthySince = -1;
        m_lastUpdate = nowSeconds;
        if (nowSeconds < 0 || !pressure.valid || !ResourceGuardDetail::percentage(pressure.availablePercent)
            || !ResourceGuardDetail::percentage(pressure.memorySome) || !ResourceGuardDetail::percentage(pressure.ioSome))
        {
            m_pressureSince = m_healthySince = -1;
            return false;
        }
        const bool stressed = uploading && pressure.availablePercent <= 10 && pressure.memorySome >= 1 && pressure.ioSome < 10;
        const bool healthy = pressure.availablePercent >= 20 && pressure.memorySome < 0.1 && pressure.ioSome < 5;
        if (!stressed) m_pressureSince = -1;
        if (!healthy) m_healthySince = -1;
        if (stressed)
        {
            if (m_pressureSince < 0) m_pressureSince = nowSeconds;
            if (m_level < 2 && nowSeconds - m_pressureSince >= 30
                && (m_lastTighten < 0 || (nowSeconds >= m_lastTighten && nowSeconds - m_lastTighten >= 60)))
            {
                ++m_level;
                m_lastTighten = m_pressureSince = nowSeconds;
                return true;
            }
        }
        if (healthy)
        {
            if (m_healthySince < 0) m_healthySince = nowSeconds;
            if (m_level > 0 && nowSeconds - m_healthySince >= 120)
            {
                --m_level;
                m_healthySince = nowSeconds;
                return true;
            }
        }
        return false;
    }
    int ceilingKiB(int userHighKiB, int userLowKiB) const
    {
        if (!m_enabled || m_level == 0 || userHighKiB <= 0 || userLowKiB >= userHighKiB) return userHighKiB;
        const int reduced = static_cast<int>(static_cast<std::int64_t>(userHighKiB) * (m_level == 1 ? 75 : 50) / 100);
        return std::min(userHighKiB, std::max(reduced, std::max(1024, userLowKiB)));
    }
    int level() const { return m_level; }
private:
    bool m_enabled;
    int m_level = 0;
    std::int64_t m_lastSample = -1;
    std::int64_t m_lastUpdate = -1;
    std::int64_t m_lastTighten = -1;
    std::int64_t m_pressureSince = -1;
    std::int64_t m_healthySince = -1;
};
}
