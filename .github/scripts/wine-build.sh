#!/usr/bin/env bash
set -euo pipefail

: "${CI_TOOLS:?}" "${PRESET:?}" "${GAME:?}"
case "$GAME" in
    Generals) game_prefix=GENERALS; generals=ON; zerohour=OFF ;;
    GeneralsMD) game_prefix=ZEROHOUR; generals=OFF; zerohour=ON ;;
    *) echo "Unsupported game: $GAME" >&2; exit 1 ;;
esac
WINEDLLOVERRIDES=mscoree,mshtml=d wineboot --init
wineserver -w
wineserver -p

winpath() { winepath -w "$1"; }
tools_win=$(winpath "$CI_TOOLS")
export WINEPATH="$tools_win\\cmake\\bin;$tools_win\\ninja;$tools_win\\git\\cmd"
mkdir -p "$WINEPREFIX/drive_c/build-temp"
export TMP='C:\build-temp' TEMP='C:\build-temp'
compiler_flags=()

if [[ "$PRESET" == vc6* ]]; then
    # MSVC600 uses mixed-case names; Wine resolves these as on Windows.
    vs="$tools_win\\vc6"
    export MSVCDir="$vs\\VC98"
    export WINEPATH="$vs\\VC98\\Bin;$vs\\Common\\MSDev98\\Bin;$vs\\Common\\Tools;$WINEPATH"
    export INCLUDE="$vs\\VC98\\ATL\\Include;$vs\\VC98\\Include;$vs\\VC98\\MFC\\Include"
    export LIB="$vs\\VC98\\Lib;$vs\\VC98\\MFC\\Lib"
elif [[ "$PRESET" == win32* ]]; then
    msvc_dir=$(find "$CI_TOOLS/msvc/VC/Tools/MSVC" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)
    sdk_dir=$(find "$CI_TOOLS/msvc/Windows Kits/10/Include" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)
    msvc=$(winpath "$msvc_dir")
    sdk="$tools_win\\msvc\\Windows Kits\\10"
    sdk_version=$(basename "$sdk_dir")
    export WINEPATH="$msvc\\bin\\Hostx64\\x86;$msvc\\bin\\Hostx64\\x64;$sdk\\bin\\$sdk_version\\x64;$WINEPATH"
    export INCLUDE="$msvc\\atlmfc\\include;$msvc\\include;$sdk\\Include\\$sdk_version\\ucrt;$sdk\\Include\\$sdk_version\\shared;$sdk\\Include\\$sdk_version\\um;$sdk\\Include\\$sdk_version\\winrt"
    export LIB="$msvc\\atlmfc\\lib\\x86;$msvc\\lib\\x86;$sdk\\Lib\\$sdk_version\\ucrt\\x86;$sdk\\Lib\\$sdk_version\\um\\x86"
    export VCPKG_VISUAL_STUDIO_PATH="$tools_win\\msvc"
    export VS170COMNTOOLS="$tools_win\\msvc\\Common7\\Tools\\"
    # CMake try_run checks (including benchmark probes) execute Debug binaries
    # during configure. Install both CRT architectures before configuring, not
    # just before CTest. Keep debug DLLs in the prefix, out of release artifacts.
    redist_dir=$(find "$CI_TOOLS/msvc/VC/Redist/MSVC" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)
    for arch in x86 x64; do
        system_dir="$WINEPREFIX/drive_c/windows/syswow64"
        [[ "$arch" != x64 ]] || system_dir="$WINEPREFIX/drive_c/windows/system32"
        find "$redist_dir/$arch" "$redist_dir/debug_nonredist/$arch" \
            -type f -iname '*.dll' -exec cp -t "$system_dir" {} +
        cp "$CI_TOOLS/msvc/Windows Kits/10/bin/$sdk_version/$arch/ucrt/ucrtbased.dll" "$system_dir/"
        cp "$CI_TOOLS/msvc/Windows Kits/10/Redist/$sdk_version/ucrt/DLLs/$arch/"*.dll "$system_dir/"
    done
    export WINEDLLOVERRIDES='msvcp140,msvcp140_1,msvcp140_2,msvcp140_atomic_wait,msvcp140_codecvt_ids,vcruntime140,vcruntime140_1,concrt140,msvcp140d,vcruntime140d,vcruntime140_1d,concrt140d,ucrtbase,ucrtbased=n,b'
else
    echo "Unsupported preset: $PRESET" >&2
    exit 1
fi

if [[ "$PRESET" == *vcpkg* ]]; then
    # Native vcpkg uses msvc-wine's compiler wrappers for x86-windows ports.
    # This avoids requiring Windows PowerShell/.NET inside the Wine prefix.
    baseline=$(python3 -c 'import json; print(json.load(open("vcpkg.json"))["builtin-baseline"])')
    vcpkg_dir="$CI_TOOLS/vcpkg"
    if [[ ! -d "$vcpkg_dir/.git" ]]; then
        git clone https://github.com/microsoft/vcpkg.git "$vcpkg_dir"
    fi
    git -C "$vcpkg_dir" checkout "$baseline"
    bash "$vcpkg_dir/bootstrap-vcpkg.sh" -disableMetrics
    mkdir -p "${VCPKG_CACHE:?}"
    installed_dir="$PWD/build/$PRESET/vcpkg_installed"
    helper_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
    vcpkg_features=()
    [[ "${BUILD_FFMPEG:-OFF}" != ON ]] || vcpkg_features+=(--x-feature=ffmpeg)
    VCPKG_ROOT="$vcpkg_dir" VCPKG_BINARY_SOURCES="clear;files,$VCPKG_CACHE,readwrite" \
        "$vcpkg_dir/vcpkg" install \
        "--overlay-triplets=$helper_dir/vcpkg-triplets" \
        "--x-manifest-root=$PWD" "--x-install-root=$installed_dir" \
        --triplet x86-windows --host-triplet x64-linux \
        "${vcpkg_features[@]}"
    export VCPKG_ROOT
    VCPKG_ROOT=$(winpath "$vcpkg_dir")
    compiler_flags+=(-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_TARGET_TRIPLET=x86-windows -DVCPKG_APPLOCAL_DEPS=OFF)
fi

cmake_exe="$CI_TOOLS/cmake/bin/cmake.exe"
wine "$cmake_exe" --preset "$PRESET" \
    "${compiler_flags[@]}" \
    -S "$(winpath "$PWD")" \
    "-DCMAKE_MAKE_PROGRAM=$tools_win\\ninja\\ninja.exe" \
    "-DGIT_EXECUTABLE=$tools_win\\git\\cmd\\git.exe" \
    -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe \
    "-DRTS_BUILD_GENERALS=$generals" "-DRTS_BUILD_ZEROHOUR=$zerohour" \
    "-DRTS_BUILD_CORE_TOOLS=${BUILD_TOOLS:-ON}" \
    "-DRTS_BUILD_${game_prefix}_TOOLS=${BUILD_TOOLS:-ON}" \
    "-DRTS_BUILD_CORE_EXTRAS=${BUILD_EXTRAS:-OFF}" \
    "-DRTS_BUILD_${game_prefix}_EXTRAS=${BUILD_EXTRAS:-OFF}" \
    "-DRTS_BUILD_OPTION_FFMPEG=${BUILD_FFMPEG:-OFF}" \
    "-DRTS_BUILD_OPTION_TESTS=${BUILD_TESTS:-OFF}" \
    "-DRTS_BUILD_OPTION_BENCHMARKS=${BUILD_BENCHMARKS:-OFF}"
wine "$cmake_exe" --build --preset "$PRESET" --parallel "$(nproc)"

if [[ "$PRESET" == *vcpkg* ]]; then
    config=release
    [[ "$PRESET" != *debug* ]] || config=debug
    dependency_bin="$installed_dir/x86-windows/bin"
    [[ "$config" != debug ]] || dependency_bin="$installed_dir/x86-windows/debug/bin"
    WINEPATH="$(winpath "$dependency_bin");$WINEPATH"
    export WINEPATH
fi

if [[ "${BUILD_TESTS:-OFF}" == ON ]]; then
    wine "$CI_TOOLS/cmake/bin/ctest.exe" --preset "$PRESET" --verbose
fi

build_dir="build/$PRESET"
config=''
if [[ "$PRESET" == win32* ]]; then
    config=/Release
    [[ "$PRESET" != *debug* ]] || config=/Debug
fi
artifacts="$build_dir/$GAME/artifacts"
mkdir -p "$artifacts"
for output_dir in "$build_dir/Core$config" "$build_dir/$GAME$config"; do
    if [[ -d "$output_dir" ]]; then
        find "$output_dir" -maxdepth 1 -type f \
            \( -iname '*.exe' -o -iname '*.dll' -o -iname '*.pdb' \) -exec cp -t "$artifacts" {} +
    fi
done
if [[ "$PRESET" == *vcpkg* && -d "$dependency_bin" ]]; then
    find "$dependency_bin" -maxdepth 1 -type f -iname '*.dll' -exec cp -t "$artifacts" {} +
fi
test -n "$(find "$artifacts" -maxdepth 1 -name '*.exe' -print -quit)"
