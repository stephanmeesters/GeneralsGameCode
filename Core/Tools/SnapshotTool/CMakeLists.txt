add_executable(z_snapshot_tool WIN32)

add_compile_definitions(z_snapshot_tool PRIVATE DEBUG_LOGGING=1)

set_target_properties(z_snapshot_tool PROPERTIES OUTPUT_NAME z_snapshot_tool)

target_link_libraries(z_snapshot_tool PRIVATE
        comctl32
        core_config
        core_utility
        corei_gameengine_public
        corei_always
)

if (WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_compile_definitions(z_snapshot_tool PRIVATE _AFXDLL)
endif ()

target_include_directories(z_snapshot_tool PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(z_snapshot_tool PRIVATE
        Logic.cpp
        UI.cpp
        Main.cpp
)