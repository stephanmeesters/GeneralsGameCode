include("$ENV{CI_TOOLS}/vcpkg/scripts/toolchains/windows.cmake")
# FFmpeg recognizes lib.exe as MSVC's librarian and uses its ar-lib adapter.
# A wrapper named only "lib" is incorrectly treated as a Unix ar command.
set(CMAKE_AR "$ENV{CI_TOOLS}/msvc/bin/${VCPKG_TARGET_ARCHITECTURE}/lib.exe" CACHE FILEPATH "MSVC librarian" FORCE)
# Native vcpkg keeps slash-style MSVC flags. FFmpeg's debug configuration
# removes these three flags using dash-style patterns before enabling its
# required optimizations; normalize their spelling so /RTC1 is removed too.
foreach(language C CXX)
    string(REGEX REPLACE "/(RTC1|Od|Ob0)( |$)" "-\\1\\2" debug_flags "${CMAKE_${language}_FLAGS_DEBUG}")
    set(CMAKE_${language}_FLAGS_DEBUG "${debug_flags}" CACHE STRING "MSVC debug flags" FORCE)
endforeach()
