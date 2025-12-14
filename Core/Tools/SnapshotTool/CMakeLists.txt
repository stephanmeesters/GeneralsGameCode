add_executable(core_snapshot_tool WIN32)

add_compile_definitions(core_snapshot_tool PRIVATE DEBUG_LOGGING=1)

set_target_properties(core_snapshot_tool PROPERTIES OUTPUT_NAME z_snapshot_tool)

target_link_libraries(core_snapshot_tool PRIVATE
        comctl32
        z_gameengine
        zi_always
)

if (WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_compile_definitions(core_snapshot_tool PRIVATE _AFXDLL)
endif ()

target_include_directories(core_snapshot_tool PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/Libraries/Include
)

target_sources(core_snapshot_tool PRIVATE
        Logic.cpp
        UI.cpp
        Main.cpp
)
