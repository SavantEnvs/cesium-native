#!/usr/bin/env bash
# Builds cesium-native twice against the vcpkg prefix baked into the image (no network):
#   build-fuzz/  sanitized + DWARF 3, with one libFuzzer and one -standalone binary per harness
#   build-test/  normal flags, the upstream doctest suite that mayhem/test.sh runs
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"

DEPS=/opt/toolchains/deps/vcpkg-56bb241
COMMON=(
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_TOOLCHAIN_FILE="$DEPS/src/scripts/buildsystems/vcpkg.cmake"
  -DVCPKG_MANIFEST_MODE=OFF
  -DVCPKG_INSTALLED_DIR="$DEPS/installed"
  -DVCPKG_TARGET_TRIPLET=x64-linux-release
  -DVCPKG_HOST_TRIPLET=x64-linux-release
  -DCESIUM_USE_EZVCPKG=OFF
  -DCESIUM_ENABLE_CLANG_TIDY=OFF
  -DCESIUM_INSTALL_STATIC_LIBS=OFF
  -DCESIUM_INSTALL_HEADERS=OFF
  -DMAYHEM_LSAN_OFF_SRC="$SRC/mayhem/lsan_off.cc"   # __lsan_is_turned_off() hook, linked into every harness
)

# 1) sanitized project + harnesses. -fsanitize=fuzzer-no-link is unconditional so the fuzzed code
#    carries SanCov edges even when SANITIZER_FLAGS is empty.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
cmake -S mayhem -B build-fuzz "${COMMON[@]}" \
  -DCESIUM_TESTS_ENABLED=OFF \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" \
  -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS"
cmake --build build-fuzz -j"$MAYHEM_JOBS"
for f in build-fuzz/bin/*; do
  cp -f "$f" "$SRC/$(basename "$f")"
done

# 2) the upstream test suite plus the known-answer probe, with normal flags
cmake -S mayhem -B build-test "${COMMON[@]}" \
  -DMAYHEM_BUILD_KAT=ON \
  -DCESIUM_TESTS_ENABLED=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build build-test -j"$MAYHEM_JOBS" --target cesium-native-tests kat
