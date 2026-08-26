#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
installer="$repo_root/Install.sh"

test -f "$installer"
bash -n "$installer"
grep -Fq 'https://raw.githubusercontent.com/ihanr/Seedbox/main' "$installer"
! grep -Fq 'https://raw.githubusercontent.com/guowanghushifu/Seedbox-Components/main' "$installer"
