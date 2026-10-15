add_executable(g_particlerendertest WIN32)
set_target_properties(g_particlerendertest PROPERTIES OUTPUT_NAME "ParticleRenderTestV${RTS_BUILD_OUTPUT_SUFFIX}")

target_link_libraries(g_particlerendertest PRIVATE
    core_wwstub
    corei_particlerendertest
    g_wwaudio
    g_wwvegas
    gi_always
    d3d8lib
    imm32
    Version
    vfw32
    winmm
)
