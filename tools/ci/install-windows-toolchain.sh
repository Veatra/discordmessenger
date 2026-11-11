#!/usr/bin/env bash
# GitHub's Ubuntu mirror list can start with an unavailable Azure mirror.
# Use explicit official sources for this installation without editing host files.
set -euo pipefail

source /etc/os-release
if [[ "$ID" != ubuntu ]]; then
    printf 'This CI installer requires an Ubuntu runner.\n' >&2
    exit 1
fi
# Select a reachable mirror before APT enters lengthy DNS/download retries.
# kernel.org hosts an independently resolved mirror of Ubuntu's signed archive.
mirror=''
for candidate in https://archive.ubuntu.com/ubuntu https://mirrors.edge.kernel.org/ubuntu; do
    if curl -fsS --connect-timeout 5 --max-time 15 \
        "$candidate/dists/$VERSION_CODENAME/InRelease" -o /dev/null; then
        mirror="$candidate"
        break
    fi
done
if [[ -z "$mirror" ]]; then
    printf 'No Ubuntu package mirror is reachable; cannot install the Windows toolchain.\n' >&2
    exit 1
fi
printf 'Using Ubuntu package mirror: %s\n' "$mirror"
sources="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/discordmessenger-apt.list"
cat > "$sources" <<SOURCES
deb $mirror ${VERSION_CODENAME} main universe
deb $mirror ${VERSION_CODENAME}-updates main universe
deb $mirror ${VERSION_CODENAME}-security main universe
SOURCES

# A managed runner may require its inherited proxy. sudo normally drops these
# variables, and APT uses lowercase names even when the host exports uppercase.
if [[ -z "${http_proxy:-}" && -n "${HTTP_PROXY:-}" ]]; then export http_proxy="$HTTP_PROXY"; fi
if [[ -z "${https_proxy:-}" && -n "${HTTPS_PROXY:-}" ]]; then export https_proxy="$HTTPS_PROXY"; fi
if [[ -z "${no_proxy:-}" && -n "${NO_PROXY:-}" ]]; then export no_proxy="$NO_PROXY"; fi
for variable in http_proxy https_proxy; do
    if [[ -n "${!variable:-}" ]]; then printf 'Using inherited %s for APT.\n' "$variable"; fi
done

apt_options=(
    -o "Dir::Etc::sourcelist=$sources"
    -o 'Dir::Etc::sourceparts=-'
    -o 'Acquire::Retries=3'
    -o 'Acquire::http::Timeout=20'
    -o 'Acquire::https::Timeout=20'
)
# Treat incomplete index refreshes as failures instead of using stale indexes.
sudo --preserve-env=http_proxy,https_proxy,no_proxy apt-get "${apt_options[@]}" -o 'APT::Update::Error-Mode=any' update
sudo --preserve-env=http_proxy,https_proxy,no_proxy apt-get "${apt_options[@]}" install -y --no-install-recommends \
    g++-mingw-w64-i686-posix binutils-mingw-w64-i686 cmake

i686-w64-mingw32-g++-posix --version
i686-w64-mingw32-windres --version
cmake --version
