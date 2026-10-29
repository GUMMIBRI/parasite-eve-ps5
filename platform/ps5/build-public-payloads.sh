#!/usr/bin/env bash
# Offline build only. Never invokes SDK deploy/test targets.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
export PS5_PAYLOAD_SDK=${PS5_PAYLOAD_SDK:-"$root/platform/ps5/tools/v0.45/ps5-payload-sdk"}
export LLVM_CONFIG=${LLVM_CONFIG:-$(command -v llvm-config-18)}
[[ $("$LLVM_CONFIG" --version) == 18.* ]] || { echo 'LLVM 18 required' >&2; exit 1; }
sdk="$PS5_PAYLOAD_SDK"
[[ -x $sdk/bin/prospero-clang && -f $sdk/target/lib/crt1.o ]]
python3 -c 'import elftools'
# Upstream wrappers expand CRT/linker paths without quoting.
[[ $sdk != *[[:space:]]* && $root != *[[:space:]]* ]] || {
    echo 'SDK wrappers require SDK/output paths without whitespace' >&2; exit 1;
}
mkdir -p "$root/outputs/ps5"
out=$(mktemp -d "$root/outputs/ps5/$(date -u +%Y%m%dT%H%M%SZ).XXXXXX")
exec > >(tee "$out/build.log") 2>&1
printf 'SDK=%s\nLLVM_CONFIG=%s\n' "$sdk" "$LLVM_CONFIG"
"$LLVM_CONFIG" --version
clang-18 --version
ld.lld-18 --version
cmake --version
if command -v dpkg-query >/dev/null; then
    dpkg-query -W clang-18 lld-18 llvm-18 cmake ninja-build python3-pyelftools
fi
# Unmodified SDK sample, built with the release CRT/sysroot/linker wrapper.
cp -- "$sdk/samples/hello_cmake/main.c" "$out/hello-sdk-main.c"
sha256sum "$out/hello-sdk-main.c"
"$sdk/bin/prospero-clang" -Wall -Werror -o "$out/hello-sdk.elf" "$out/hello-sdk-main.c"
cmake -S "$root" -B "$out/build" -G Ninja \
    -DPE_PS5_PAYLOAD=ON \
    -DCMAKE_TOOLCHAIN_FILE="$sdk/toolchain/prospero.cmake" \
    -DPS5_PAYLOAD_SDK="$sdk" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$out"
cmake --build "$out/build" --verbose
for elf in "$out/hello-sdk.elf" "$out/pe-ps5-io.elf"; do
    python3 - "$elf" <<'PY'
import sys
from elftools.elf.elffile import ELFFile
with open(sys.argv[1], 'rb') as stream:
    elf = ELFFile(stream)
    assert elf.elfclass == 64 and elf.little_endian
    assert elf.header['e_machine'] == 'EM_X86_64'
    assert elf.header['e_type'] == 'ET_DYN'
    assert elf.header['e_entry'] != 0
    assert any(s['p_type'] == 'PT_LOAD' for s in elf.iter_segments())
    print(sys.argv[1], 'ELF64 x86-64 PIE, nonzero entry, load segments; UNTESTED ON CONSOLE')
PY
    sha256sum "$elf"
done
printf '%s\n' 'UNTESTED ON CONSOLE. Link success does not establish loader/stdio/filesystem compatibility.' > "$out/CONSOLE-UNTESTED.txt"
