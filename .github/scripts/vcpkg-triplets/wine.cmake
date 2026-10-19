set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../vcpkg-wine-toolchain.cmake")
# Follow msvc-wine's native vcpkg setup: Unix build tools invoke the Windows
# compiler through the supplied wrappers, without Windows PowerShell or vcvars.
set(ENV{CC} cl.exe)
set(ENV{CXX} cl.exe)
set(ENV{PATH} "$ENV{CI_TOOLS}/msvc/bin/${VCPKG_TARGET_ARCHITECTURE}:$ENV{PATH}")
