#!/usr/bin/env bash
# Run inside Ubuntu 24.04 after the owner completes WSL first-run setup.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
version=v0.45
digest=5fac64093d0e3e69f2584ce7935f24f703f622d9681f1495c85b9f00d1291a2c
archive="$root/platform/ps5/downloads/ps5-payload-sdk-$version.zip"
destination="$root/platform/ps5/tools/$version"
if [[ ${1:-} == --install-dependencies ]]; then
    sudo apt-get update
    sudo apt-get install --no-install-recommends bash clang-18 lld-18 llvm-18 wget unzip git make cmake meson pkg-config python3 python3-pyelftools socat ninja-build
elif [[ $# != 0 ]]; then
    echo 'usage: bash platform/ps5/setup-public-sdk.sh [--install-dependencies]' >&2
    exit 1
fi
for tool in clang-18 ld.lld-18 llvm-config-18 wget unzip cmake ninja python3 sha256sum git; do
    command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 1; }
done
[[ $(llvm-config-18 --version) == 18.* ]] || { echo 'LLVM 18 required' >&2; exit 1; }
source_tree="$root/platform/ps5/sdk-review"
[[ $(git -C "$source_tree" rev-parse HEAD) == a7685ce8e97ce1f8a4f38253a41d54241c9124f1 ]]
[[ -f $source_tree/LICENSE ]]
mkdir -p "$(dirname -- "$archive")" "$(dirname -- "$destination")"
if [[ ! -e $archive ]]; then
    wget -O "$archive.part" "https://github.com/ps5-payload-dev/sdk/releases/download/$version/ps5-payload-sdk.zip"
    printf '%s  %s\n' "$digest" "$archive.part" | sha256sum --check
    mv -- "$archive.part" "$archive"
fi
printf '%s  %s\n' "$digest" "$archive" | sha256sum --check
# Never merge into or overwrite an installed SDK, even after a partial attempt.
if [[ -e $destination ]]; then
    echo "Preserving existing $destination; inspect it or use it explicitly with build-public-payloads.sh." >&2
    exit 1
fi
stage=$(mktemp -d "${destination}.stage.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT
unzip -q "$archive" -d "$stage"
[[ -f $stage/ps5-payload-sdk/target/lib/crt1.o && -f $stage/ps5-payload-sdk/samples/hello_cmake/main.c ]]
# Actual pinned source uses LICENSE, despite COPYING references in headers.
cp -- "$source_tree/LICENSE" "$stage/ps5-payload-sdk/LICENSE.SDK"
cp -- "$source_tree/README.md" "$stage/ps5-payload-sdk/README.SOURCE.md"
mv -T -- "$stage" "$destination"
printf 'SDK=%s\nRelease=%s\nSHA256=%s\n' "$destination/ps5-payload-sdk" "$version" "$digest"
echo 'Next: bash platform/ps5/build-public-payloads.sh'
