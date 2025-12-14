add_executable(z_gamestate_tool WIN32)

add_compile_definitions(z_gamestate_tool PRIVATE DEBUG_LOGGING=1)

set_target_properties(z_gamestate_tool PROPERTIES OUTPUT_NAME z_gamestate_tool)

target_link_libraries(z_gamestate_tool PRIVATE
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

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_compile_definitions(z_gamestate_tool PRIVATE _AFXDLL)
endif()

target_include_directories(z_gamestate_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(z_gamestate_tool PRIVATE
    Logic.cpp
    UI.cpp
    Main.cpp
)

find_package(Python3 COMPONENTS Interpreter )
#if(Python3_Interpreter_FOUND)
    message("JMAMAMAMMAMAMMAMAMAMA")
    set(SNAPSHOT_SCHEMA_OUT ${CMAKE_CURRENT_BINARY_DIR}/GeneratedSnapshotSchema.h)
    add_custom_command(
        OUTPUT ${SNAPSHOT_SCHEMA_OUT}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/cpp/generate_snapshot_schema.py
                --output ${SNAPSHOT_SCHEMA_OUT}
                --source-root ${CMAKE_SOURCE_DIR}/GeneralsMD/Code
                --source-root ${CMAKE_SOURCE_DIR}/Core/GameEngine
                --compile-commands ${CMAKE_BINARY_DIR}/compile_commands.json
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/cpp/generate_snapshot_schema.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating snapshot schema header for GameStateTool"
        VERBATIM
    )
    add_custom_target(generate_snapshot_schema DEPENDS ${SNAPSHOT_SCHEMA_OUT})
    add_dependencies(z_gamestate_tool generate_snapshot_schema)
    target_include_directories(z_gamestate_tool PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
#endif()

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
