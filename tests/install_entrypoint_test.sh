#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
installer="$repo_root/Install.sh"
qb_installer="$repo_root/Torrent Clients/qBittorrent/qBittorrent_install.sh"
qb_522_binary="$repo_root/Torrent Clients/qBittorrent/x86_64/qBittorrent-5.2.2 - libtorrent-v1.2.20/qbittorrent-nox"

test -f "$installer"
bash -n "$installer"
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main' "$installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$installer"

test -f "$qb_installer"
grep -Fq '"5.2.2"' "$qb_installer"
grep -Fq '"5.2."' "$qb_installer"
test -s "$qb_522_binary"
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main/Torrent%20Clients/qBittorrent/' "$qb_installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$qb_installer"
