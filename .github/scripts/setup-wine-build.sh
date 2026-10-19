#!/usr/bin/env bash
set -euo pipefail

# The VC6 path follows resources/dockerbuild: run Windows CMake and Ninja
# under Wine, so Windows include casing and the existing presets still work.
: "${CI_TOOLS:?}" "${PRESET:?}"
mkdir -p "$CI_TOOLS"
cd "$CI_TOOLS"

download() { curl --fail --location --retry 3 "$1" --output "$2"; }

if [[ ! -f cmake/bin/cmake.exe ]]; then
    version=3.31.6
    archive="cmake-$version-windows-x86_64.zip"
    base="https://github.com/Kitware/CMake/releases/download/v$version"
    download "$base/$archive" "$archive"
    download "$base/cmake-$version-SHA-256.txt" cmake.sha256
    grep " $archive$" cmake.sha256 | sha256sum --check --strict
    unzip -q "$archive"
    mv "cmake-$version-windows-x86_64" cmake
    rm "$archive" cmake.sha256
fi

if [[ ! -f ninja/ninja.exe ]]; then
    download https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip ninja.zip
    unzip -q ninja.zip -d ninja
    rm ninja.zip
fi

# BusyBox avoids the MSYS shell, which can hang under Wine during submodule updates.
if [[ ! -f git/cmd/git.exe ]]; then
    download https://github.com/git-for-windows/git/releases/download/v2.49.0.windows.1/MinGit-2.49.0-busybox-64-bit.zip git.zip
    unzip -q git.zip -d git
    rm git.zip
fi

if [[ "$PRESET" == vc6* ]]; then
    if [[ ! -d vc6 ]]; then
        revision=001c4bafdcf2ef4b474d693acccd35a91e848f40
        download "https://github.com/itsmattkc/MSVC600/archive/$revision.zip" vc6.zip
        echo 'd0ee1f6dcef7db3ad703120d9fb4fad49ebca28f44372e40550348b1c00ca583  vc6.zip' | sha256sum --check --strict
        unzip -q vc6.zip
        mv "MSVC600-$revision" vc6
        rm vc6.zip
    fi
elif [[ "$PRESET" == win32* ]]; then
    revision=514f8ea34842cd6d831804d0e9658d3a32870ae1
    if [[ ! -f msvc-wine/install.sh ]]; then
        download "https://github.com/mstorsjo/msvc-wine/archive/$revision.zip" msvc-wine.zip
        unzip -q msvc-wine.zip
        mv "msvc-wine-$revision" msvc-wine
        rm msvc-wine.zip
    fi
    if [[ ! -f msvc/ci-install-complete ]]; then
        # MFC is needed by WorldBuilder and the tools.
        python3 msvc-wine/vsdownload.py --accept-license --major 17 --msvc-version 17.14 \
            --with-default --architecture x86 x64 --dest "$CI_TOOLS/msvc" \
            Microsoft.VisualStudio.Component.VC.ATLMFC
        touch msvc/ci-install-complete
    fi
    if [[ ! -f msvc/bin/x86/cl ]]; then
        # cl.exe resolves include casing under Wine. The upstream fixinclude
        # pass is for native clang, and its byte-based newline rewriting can
        # corrupt UTF-16 MFC resource scripts. Keep the original headers intact.
        # shellcheck disable=SC2016
        sed '/"\$ORIG"\/fixinclude /c\    :' msvc-wine/install.sh > msvc-wine/install-wine.sh
        bash -n msvc-wine/install-wine.sh
        WINEDLLOVERRIDES=mscoree,mshtml=d xvfb-run -a bash msvc-wine/install-wine.sh "$CI_TOOLS/msvc"
    fi
    ar_adapter=$(find /usr/share -maxdepth 2 -path '*/automake-*/ar-lib' | sort -V | tail -1)
    test -n "$ar_adapter"
    for arch in x86 x64; do
        cp "$ar_adapter" "msvc/bin/$arch/ar-lib"
    done
else
    echo "Unsupported preset: $PRESET" >&2
    exit 1
fi
