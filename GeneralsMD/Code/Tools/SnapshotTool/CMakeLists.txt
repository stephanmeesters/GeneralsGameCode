add_executable(z_snapshot_tool WIN32)

set_target_properties(z_snapshot_tool PROPERTIES OUTPUT_NAME z_snapshot_tool)

target_link_libraries(z_snapshot_tool PRIVATE
    imm32
    vfw32
    winmm
    z_gameenginedevice
    zi_always
)

target_compile_definitions(z_snapshot_tool PRIVATE _AFXDLL)

target_include_directories(z_snapshot_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/Libraries/Include
)

target_sources(z_snapshot_tool PRIVATE
    Logic.cpp
    Pipe.cpp
    UI.cpp
    Main.cpp
)
