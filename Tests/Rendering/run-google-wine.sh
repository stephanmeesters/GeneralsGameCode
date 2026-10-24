#!/usr/bin/env bash
# Run an MSVC-built test executable; this script does not compile anything.
set -euo pipefail
executable=$(realpath "${1:?Usage: run-google-wine.sh TEST_EXECUTABLE DXVK_DIRECTORY}")
dxvk_dir=$(realpath "${2:?Pass an extracted DXVK release directory}")
work_dir=$(dirname "$executable")
name=$(basename "$executable" .exe)
cp "$dxvk_dir/x32/d3d8.dll" "$dxvk_dir/x32/d3d9.dll" "$work_dir/"
cd "$work_dir"
export WINEPREFIX="$work_dir/wine-prefix"
export WINEDEBUG=-all
export DXVK_LOG_PATH="$work_dir"
export DXVK_LOG_LEVEL=info
export WINEDLLOVERRIDES='d3d8,d3d9=n;mscoree,mshtml='
# Older Wine uses 32-bit Vulkan for a 32-bit exe; newer WoW64 uses 64-bit
# Vulkan. Expose only lavapipe manifests and let the loader pick its architecture.
export VK_DRIVER_FILES=${LAVAPIPE_ICD:-$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print | sort | paste -sd:)}
: "${VK_DRIVER_FILES:?No lavapipe ICD found}"
export DXVK_FILTER_DEVICE_NAME=llvmpipe
rm -f results.xml triangle.bmp "${name}_d3d9.log"
# Shut down this prefix's Wine processes before Xvfb exits (including on failure).
timeout -k 5s 180s xvfb-run -a bash -c '
    trap '\''wineserver -k'\'' EXIT
    wine "$1" --gtest_filter=Rendering.* --gtest_output=xml:results.xml
' bash "$executable" 2>&1 | tee rendering.log
# An empty filter is a successful Google Test exit, but must fail this job.
python3 - <<'PY'
import xml.etree.ElementTree as ET
root = ET.parse('results.xml').getroot()
tests = root.findall('./testsuite/testcase')
assert tests, 'No rendering tests ran'
for test in tests:
    assert test.get('classname') == 'Rendering', ET.tostring(test)
    assert test.get('status') == 'run' and test.get('result') == 'completed', ET.tostring(test)
    assert test.find('skipped') is None and test.find('failure') is None, ET.tostring(test)
PY
test -s triangle.bmp
grep -i 'Device : llvmpipe' "${name}_d3d9.log"
