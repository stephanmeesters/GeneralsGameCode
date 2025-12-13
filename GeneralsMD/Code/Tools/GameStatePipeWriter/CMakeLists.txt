#add_executable(z_gamestate_pipe_writer)
#
#set_target_properties(z_gamestate_pipe_writer PROPERTIES OUTPUT_NAME z_gamestate_pipe_writer)
#
#target_sources(z_gamestate_pipe_writer PRIVATE
#    PipeWriterMain.cpp
#)
#
#target_link_libraries(z_gamestate_pipe_writer PRIVATE
#    zi_always
#)




add_executable(z_gamestate_pipe_writer)

add_compile_definitions(z_gamestate_pipe_writer PRIVATE DEBUG_LOGGING=1)

set_target_properties(z_gamestate_pipe_writer PROPERTIES OUTPUT_NAME z_gamestate_pipe_writer)

target_link_libraries(z_gamestate_pipe_writer PRIVATE
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
    target_compile_definitions(z_gamestate_pipe_writer PRIVATE _AFXDLL)
endif()

target_include_directories(z_gamestate_pipe_writer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(z_gamestate_pipe_writer PRIVATE
    PipeWriterMain.cpp
        GeneratedSnapshotSchema.h
)

target_include_directories(z_gamestate_pipe_writer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
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
