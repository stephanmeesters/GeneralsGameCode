add_executable(z_headless_tool)

target_compile_definitions(z_headless_tool PRIVATE _CONSOLE)

if("${CMAKE_SYSTEM}" MATCHES "Windows")
    set_target_properties(z_headless_tool PROPERTIES OUTPUT_NAME z_gamestate_tool)
else()
    set_target_properties(z_headless_tool PROPERTIES OUTPUT_NAME z_gamestate_tool)
endif()

target_link_libraries(z_headless_tool PRIVATE
    binkstub
    comctl32
    core_debug
    core_profile
    d3d8
    d3dx8
    dinput8
    dxguid
    imm32
    milesstub
    vfw32
    winmm
    z_gameengine
    z_gameenginedevice
    zi_always
)

target_include_directories(z_headless_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(z_headless_tool PRIVATE
    ToolMain.cpp
)

# Version headers for this tool (mirrors the main game target).
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)
