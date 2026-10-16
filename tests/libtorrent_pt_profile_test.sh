#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 || ! -x "$1" ]]; then
    printf 'Usage: %s COMPILED_PROFILE_PROBE\n' "$0" >&2
    exit 2
fi
probe=$1

# Reject malformed dependency patches before accepting a rebuilt library.
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
for name in patch openssl-4.patch pt-profile.patch; do
    git apply --numstat "$repo_root/patches/libtorrent/1.2.20/$name" >/dev/null
done

# Each invocation is a fresh process: LT initializes the profile at startup.
printf 'unset: '
(unset SEEDBOX_PT_PROFILE; "$probe" 50 100)
printf 'race: '
SEEDBOX_PT_PROFILE=race "$probe" 50 100
printf 'balanced: '
SEEDBOX_PT_PROFILE=balanced "$probe" 10 100
printf 'race200: '
SEEDBOX_PT_PROFILE=race200 "$probe" 50 200
printf 'invalid: '
SEEDBOX_PT_PROFILE=invalid "$probe" 50 100
printf 'empty: '
SEEDBOX_PT_PROFILE= "$probe" 50 100
