#!/usr/bin/env bash
# Run inside a Linux environment with MinGW, Wine, Xvfb and Mesa lavapipe.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "$0")" && pwd)
work_dir=${1:?Usage: run-wine.sh BUILD_DIRECTORY DXVK_DIRECTORY}
dxvk_dir=${2:?Pass an extracted DXVK release directory}
mkdir -p "$work_dir"
work_dir=$(cd -- "$work_dir" && pwd)
dxvk_dir=$(cd -- "$dxvk_dir" && pwd)
cmake -S "$source_dir" -B "$work_dir/build" -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build "$work_dir/build"
cp "$work_dir/build/d3d8_probe.exe" "$work_dir/"
cp "$dxvk_dir/x32/d3d8.dll" "$dxvk_dir/x32/d3d9.dll" "$work_dir/"
cd "$work_dir"
export WINEPREFIX="$work_dir/prefix"
export WINEDEBUG=-all
export DXVK_LOG_PATH="$work_dir"
export DXVK_LOG_LEVEL=info
export WINEDLLOVERRIDES='d3d8,d3d9=n;mscoree,mshtml='
export VK_DRIVER_FILES=${LAVAPIPE_ICD:-/usr/share/vulkan/icd.d/lvp_icd.x86_64.json}
export DXVK_FILTER_DEVICE_NAME=llvmpipe
# A virtual X server is required even though rendering is offscreen.
timeout -k 5s 120s xvfb-run -a wine ./d3d8_probe.exe triangle.bmp
# Also verify that this run actually used the CPU Vulkan device.
grep -i llvmpipe d3d8_probe_d3d9.log
