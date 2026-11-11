#!/usr/bin/env bash
# GitHub's Ubuntu mirror list can start with an unavailable Azure mirror.
# Use explicit official sources for this installation without editing host files.
set -euo pipefail

source /etc/os-release
if [[ "$ID" != ubuntu ]]; then
    printf 'This CI installer requires an Ubuntu runner.\n' >&2
    exit 1
fi
sources="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/discordmessenger-apt.list"
cat > "$sources" <<SOURCES
deb https://archive.ubuntu.com/ubuntu ${VERSION_CODENAME} main universe
deb https://archive.ubuntu.com/ubuntu ${VERSION_CODENAME}-updates main universe
deb https://security.ubuntu.com/ubuntu ${VERSION_CODENAME}-security main universe
SOURCES

apt_options=(
    -o "Dir::Etc::sourcelist=$sources"
    -o 'Dir::Etc::sourceparts=-'
    -o 'Acquire::Retries=3'
    -o 'Acquire::http::Timeout=20'
    -o 'Acquire::https::Timeout=20'
)
# Treat incomplete index refreshes as failures instead of using stale indexes.
sudo apt-get "${apt_options[@]}" -o 'APT::Update::Error-Mode=any' update
sudo apt-get "${apt_options[@]}" install -y --no-install-recommends \
    g++-mingw-w64-i686-posix binutils-mingw-w64-i686 cmake

i686-w64-mingw32-g++-posix --version
i686-w64-mingw32-windres --version
cmake --version
