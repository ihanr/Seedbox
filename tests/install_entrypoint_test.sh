#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
installer="$repo_root/Install.sh"
qb_installer="$repo_root/Torrent Clients/qBittorrent/qBittorrent_install.sh"

test -f "$installer"
bash -n "$installer"
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main' "$installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$installer"

test -f "$qb_installer"
source "$qb_installer"
need_input() { :; }
warn() { printf '%s\n' "$*" >&2; }
tput() { :; }
clear() { :; }

# Both published versions must be selectable through the actual menu.
qb_ver_choose <<< 8
[[ "$qb_ver" == qBittorrent-5.2.2 ]]
qb_ver_choose <<< 9
[[ "$qb_ver" == qBittorrent-5.2.4 ]]

# 5.2.4 ships only with LT 1.2.20; invalid choices return to the menu.
qb_ver=qBittorrent-5.2.4
lib_ver=libtorrent-v2.0.11
lib_ver_check <<< 4
[[ "$lib_ver" == libtorrent-v1.2.20 ]]
qb_install_check </dev/null
[[ "$qb_ver" == qBittorrent-5.2.4 && "$lib_ver" == libtorrent-v1.2.20 ]]
for qb_ver in qBittorrent-5.0.4 qBittorrent-5.2.2; do
    lib_ver=libtorrent-v2.0.11
    lib_ver_check </dev/null
    [[ "$lib_ver" == libtorrent-v2.0.11 ]]
done

# Fail safely if a rejected 5.2.4 package reaches any process/file changes.
check_install_guard() {
    local version=$1 library=$2 machine=$3 expected_status=$4 status
    status=0
    (
        uname() { printf '%s\n' "$machine"; }
        pgrep() { exit 97; }
        rm() { exit 98; }
        pkill() { exit 98; }
        wget() { exit 98; }
        install_qBittorrent_ test-user test-password "$version" "$library" 4096 9090 36666
    ) >/dev/null 2>&1 || status=$?
    if [[ "$status" != "$expected_status" ]]; then
        printf 'Unexpected install status for %s / %s / %s: %s (wanted %s)\n' \
            "$version" "$library" "$machine" "$status" "$expected_status" >&2
        return 1
    fi
}
check_install_guard qBittorrent-5.2.4 libtorrent-v2.0.11 x86_64 1
check_install_guard qBittorrent-5.2.4 libtorrent-v1.2.19 x86_64 1
check_install_guard qBittorrent-5.2.4 libtorrent-v1.2.20 aarch64 1
check_install_guard qBittorrent-5.2.4 libtorrent-v1.2.20 armv7l 1
check_install_guard qBittorrent-5.2.4 libtorrent-v1.2.20 x86_64 97
check_install_guard qBittorrent-5.2.2 libtorrent-v2.0.11 aarch64 97

for qb_version in 5.2.2 5.2.4; do
    qb_binary="$repo_root/Torrent Clients/qBittorrent/x86_64/qBittorrent-$qb_version - libtorrent-v1.2.20/qbittorrent-nox"
    if [[ ! -s "$qb_binary" ]]; then
        printf 'Missing qBittorrent %s binary: %s\n' "$qb_version" "$qb_binary" >&2
        exit 1
    fi
done
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main/Torrent%20Clients/qBittorrent/' "$qb_installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$qb_installer"
