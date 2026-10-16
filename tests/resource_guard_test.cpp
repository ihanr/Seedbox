// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "../patches/qbittorrent/5.2.4/resourceguard.h"

namespace
{
using Seedbox::Pressure;
using Seedbox::ResourceGuard;

void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

const Pressure bad {true, 10, 1, 0};
const Pressure good {true, 20, 0.09, 4.99};

void feed(ResourceGuard &guard, int begin, int end, const Pressure &pressure, bool uploading = true)
{
    for (int now = begin; now <= end; now += 10)
        guard.update(now, pressure, uploading);
}

// Catches accidental activation, unlimited-value normalization and excessive polling.
void defaultOffAndSampling()
{
    ResourceGuard off;
    check(!off.enabled() && !off.shouldSample(0), "default guard must not sample");
    feed(off, 0, 300, bad);
    check(off.level() == 0 && off.ceilingKiB(8192, 2048) == 8192, "default guard changed settings");
    check(off.ceilingKiB(0, 0) == 0, "zero baseline was raised");
    ResourceGuard on(true);
    check(on.shouldSample(0) && !on.shouldSample(0) && !on.shouldSample(9)
        && on.shouldSample(10) && !on.shouldSample(19) && on.shouldSample(20), "sampling is not bounded to ten seconds");
    feed(on, 0, 300, good);
    check(on.level() == 0, "healthy host was tightened");
}

// Catches single-spike activation, missing cooldown, missing level cap and stale baselines.
void pressureLevelsAndCeilings()
{
    ResourceGuard guard(true);
    feed(guard, 0, 20, bad);
    check(guard.level() == 0, "short pressure spike tightened buffers");
    check(guard.update(30, bad, true) && guard.level() == 1, "continuous pressure did not tighten after thirty seconds");
    check(guard.ceilingKiB(8192, 2048) == 6144, "first level is not seventy-five percent");
    check(guard.ceilingKiB(4096, 2048) == 3072, "new user baseline was ignored");
    check(guard.ceilingKiB(8192, 7000) == 7000, "user low watermark was crossed");
    check(guard.ceilingKiB(1200, 0) == 1024, "one MiB floor was crossed");
    check(guard.ceilingKiB(512, 0) == 512, "small baseline was increased");
    check(guard.ceilingKiB(8192, 8192) == 8192 && guard.ceilingKiB(8192, 9000) == 8192, "inverted watermarks were changed");
    check(guard.ceilingKiB(0, 0) == 0 && guard.ceilingKiB(-1, 0) == -1, "nonpositive baseline was increased");
    check(guard.ceilingKiB(std::numeric_limits<int>::max(), 0) == 1610612735, "ceiling calculation overflowed");
    feed(guard, 40, 80, bad);
    check(guard.level() == 1, "second tightening ignored sixty-second cooldown");
    check(guard.update(90, bad, true) && guard.level() == 2, "second tightening was not applied");
    check(guard.ceilingKiB(8192, 2048) == 4096, "second level is not fifty percent");
    feed(guard, 100, 400, bad);
    check(guard.level() == 2 && !guard.update(410, bad, true), "guard exceeded two levels");
}

// Catches counting interrupted, invalid or unobserved intervals as continuous pressure.
void interruptionsAndInvalidSamples()
{
    const Pressure middle {true, 15, 0.5, 0};
    ResourceGuard spike(true);
    feed(spike, 0, 20, bad);
    spike.update(30, middle, true);
    feed(spike, 40, 60, bad);
    check(spike.level() == 0, "intermediate sample did not reset pressure window");
    spike.update(70, bad, true);
    check(spike.level() == 1, "fresh continuous window did not tighten");

    ResourceGuard gap(true);
    feed(gap, 0, 20, bad);
    gap.update(60, bad, true);
    feed(gap, 70, 80, bad);
    check(gap.level() == 0, "unobserved sampling gap counted as pressure");
    gap.update(90, bad, true);
    check(gap.level() == 1, "pressure did not resume after sampling gap");
    gap.update(100, {}, true);
    feed(gap, 110, 130, bad);
    check(gap.level() == 1, "invalid sample did not reset continuous window");
    gap.update(140, {true, 10, std::numeric_limits<double>::quiet_NaN(), 0}, true);
    gap.update(150, {true, 10, 101, 0}, true);
    gap.update(160, {true, -1, 1, 0}, true);
    feed(gap, 170, 190, bad);
    check(gap.level() == 1, "nonfinite or out-of-range sample changed level");
    feed(gap, 200, 300, {}, false);
    check(gap.level() == 1, "invalid metrics recovered existing protection");
}

// Catches treating no upload or I/O pressure as a reason to tighten memory buffers.
void uploadAndIoGates()
{
    ResourceGuard idle(true);
    feed(idle, 0, 300, bad, false);
    check(idle.level() == 0, "idle session was tightened");
    ResourceGuard io(true);
    feed(io, 0, 300, {true, 10, 1, 10});
    check(io.level() == 0, "I/O pressure was treated as memory-only pressure");
    feed(io, 310, 340, {true, 10, 1, 9.99});
    check(io.level() == 1, "strict I/O boundary was not honored");
    ResourceGuard insufficient(true);
    feed(insufficient, 0, 100, {true, 10.01, 1, 0});
    feed(insufficient, 110, 210, {true, 10, 0.99, 0});
    check(insufficient.level() == 0, "one memory condition alone caused tightening");
}

// Catches premature recovery, interrupted recovery and inability to recover while idle.
void healthyRecovery()
{
    ResourceGuard guard(true);
    feed(guard, 0, 90, bad);
    feed(guard, 100, 210, good, false);
    check(guard.level() == 2, "protection recovered before two healthy minutes");
    check(guard.update(220, good, false) && guard.level() == 1, "idle healthy recovery failed");
    feed(guard, 230, 330, good, false);
    check(guard.level() == 1, "recovery skipped a level");
    guard.update(340, {true, 20, 0.1, 0}, false);
    feed(guard, 350, 460, good, false);
    check(guard.level() == 1, "memory PSI recovery boundary failed to reset window");
    guard.update(470, {true, 20, 0, 5}, false);
    feed(guard, 480, 590, good, false);
    check(guard.level() == 1, "I/O recovery boundary failed to reset window");
    check(guard.update(600, good, false) && guard.level() == 0, "second healthy recovery failed");
    check(guard.ceilingKiB(8192, 2048) == 8192, "recovery did not restore current user baseline");

    ResourceGuard gap(true);
    feed(gap, 0, 30, bad);
    feed(gap, 40, 140, good);
    feed(gap, 180, 290, good);
    check(gap.level() == 1, "unobserved gap counted as healthy recovery");
    gap.update(300, good, true);
    check(gap.level() == 0, "recovery failed after fresh healthy window");
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("seedbox-resource-guard-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture()
    {
        write("proc/meminfo", "MemTotal: 100000 kB\nMemAvailable: 10000 kB\n");
        write("proc/pressure/memory", "some avg10=1.25 avg60=1.00 avg300=0.00 total=10\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
        write("proc/pressure/io", "some avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
        write("proc/self/cgroup", "0::/system.slice/qb.service\n");
        write("proc/1/cgroup", "0::/init.scope\n");
        write("proc/self/mountinfo", "1 0 8:1 / / rw,relatime - ext4 /dev/sda1 rw\n");
        write("cgroup/cgroup.controllers", "cpu io memory pids\n");
        for (const std::string path : {"", "/system.slice", "/system.slice/qb.service"})
        {
            write("cgroup" + path + "/memory.max", "max\n");
            write("cgroup" + path + "/memory.high", "max\n");
        }
    }
    ~Fixture() { std::filesystem::remove_all(root); }
    void write(const std::string &relative, const std::string &content)
    {
        const auto path = root / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << content;
        check(file.good(), "could not write pressure fixture");
    }
    void remove(const std::string &relative) { std::filesystem::remove(root / relative); }
    Pressure read() const { return Seedbox::readPressure((root / "proc").string(), (root / "cgroup").string()); }
};

// Catches unit/PSI parsing errors and accidental use of host RAM under a visible quota.
void linuxFileParsing()
{
    Fixture fixture;
    auto pressure = fixture.read();
    check(pressure.valid && pressure.availablePercent == 10 && pressure.memorySome == 1.25 && pressure.ioSome == 0, "valid host metrics were not parsed");
    fixture.remove("cgroup/memory.max");
    fixture.remove("cgroup/memory.high");
    check(fixture.read().valid, "unlimited v2 mount root without limit files was rejected");
    fixture.write("cgroup/system.slice/memory.high", "1048576\n");
    check(!fixture.read().valid, "finite ancestor memory.high was ignored");
    fixture.write("cgroup/system.slice/memory.high", "max\n");
    fixture.write("cgroup/system.slice/qb.service/memory.max", "1048576\n");
    check(!fixture.read().valid, "finite own memory.max was ignored");
    fixture.write("cgroup/system.slice/qb.service/memory.max", "max\n");
    fixture.remove("cgroup/system.slice/memory.high");
    check(!fixture.read().valid, "missing ancestor limit file was accepted");
    fixture.write("cgroup/system.slice/memory.high", "max\n");
    fixture.write("proc/self/cgroup", "8:memory:/test\n");
    check(!fixture.read().valid, "cgroup v1 was accepted");
    fixture.write("proc/self/cgroup", "0::/../../outside\n");
    check(!fixture.read().valid, "cgroup path traversal was accepted");
    fixture.write("proc/self/cgroup", "0::/system.slice/qb.service\n");
    fixture.write("proc/1/cgroup", "0::/docker/abc\n");
    check(!fixture.read().valid, "container cgroup marker was ignored");
    fixture.write("proc/1/cgroup", "0::/init.scope\n");
    fixture.write("proc/self/root/.dockerenv", "");
    check(!fixture.read().valid, "namespaced container marker was ignored");
    fixture.remove("proc/self/root/.dockerenv");
    fixture.write("proc/self/mountinfo", "1 0 0:1 / / rw - overlay overlay rw\n");
    check(!fixture.read().valid, "container overlay root was accepted");
    fixture.write("proc/self/mountinfo", "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n");
    fixture.remove("cgroup/cgroup.controllers");
    check(!fixture.read().valid, "missing cgroup v2 identity was accepted");
}

void malformedFiles()
{
    Fixture fixture;
    for (const std::string value : {"nan", "inf", "-0.1", "100.1", "1oops", ""})
    {
        fixture.write("proc/pressure/memory", "some avg10=" + value + " avg60=0\n");
        check(!fixture.read().valid, "invalid memory PSI was accepted");
    }
    fixture.write("proc/pressure/memory", "some avg10=0 avg60=0\n");
    fixture.write("proc/pressure/io", "full avg10=0 avg60=0\n");
    check(!fixture.read().valid, "missing I/O some line was accepted");
    fixture.write("proc/pressure/io", "some avg10=100 avg60=0\n");
    check(fixture.read().valid, "legal zero and one hundred PSI boundaries were rejected");
    for (const std::string contents : {"MemTotal: 0 kB\nMemAvailable: 0 kB\n", "MemTotal: 100 kB\nMemAvailable: 101 kB\n", "MemTotal: -100 kB\nMemAvailable: 10 kB\n", "MemTotal: 100 MB\nMemAvailable: 10 kB\n", "MemTotal: 100 kB\n"})
    {
        fixture.write("proc/meminfo", contents);
        check(!fixture.read().valid, "invalid meminfo was accepted");
    }
    fixture.write("proc/meminfo", "MemTotal: 100000 kB\nMemAvailable: 10000 kB\n");
    fixture.remove("proc/pressure/io");
    check(!fixture.read().valid, "missing PSI file was accepted");
}

// Kernel PSI uses dot decimals even when the process C locale uses commas.
void numericLocaleParsing()
{
    Fixture fixture;
    const std::string original = std::setlocale(LC_NUMERIC, nullptr);
    bool tested = false;
    bool valid = false;
    for (const char *name : {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8", "de_DE", "fr_FR", "German_Germany.1252", "French_France.1252"})
    {
        if (std::setlocale(LC_NUMERIC, name) && std::string(std::localeconv()->decimal_point) == ",")
        {
            tested = true;
            valid = fixture.read().valid;
            break;
        }
    }
    std::setlocale(LC_NUMERIC, original.c_str());
    if (tested)
        check(valid, "kernel PSI dot decimals were rejected under comma numeric locale");
    else
        std::cout << "SKIP comma numeric locale regression: no supported comma locale installed\n";
}
}

int main()
{
    try
    {
        defaultOffAndSampling();
        pressureLevelsAndCeilings();
        interruptionsAndInvalidSamples();
        uploadAndIoGates();
        healthyRecovery();
        linuxFileParsing();
        malformedFiles();
        numericLocaleParsing();
        std::cout << "PASS resource guard policy and Linux fixtures\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
