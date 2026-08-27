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
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main/Torrent%20Clients/qBittorrent/' "$qb_installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$qb_installer"
