#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
installer="$repo_root/Install.sh"
readme="$repo_root/README.md"

bash -n "$installer"
grep -Fq '"13"' "$installer"
grep -Fq '"24"' "$installer"
grep -Fq 'Custom BBR modules are supported on Debian only' "$installer"
grep -Fq 'Debian 10、11、12、13' "$readme"
grep -Fq 'Ubuntu 20.04、22.04、23.04、24.04' "$readme"
