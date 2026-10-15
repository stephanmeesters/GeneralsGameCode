add_executable(z_particlerendertest WIN32)
set_target_properties(z_particlerendertest PROPERTIES OUTPUT_NAME "ParticleRenderTestZH${RTS_BUILD_OUTPUT_SUFFIX}")

target_link_libraries(z_particlerendertest PRIVATE
    core_wwstub
    corei_particlerendertest
    z_wwaudio
    z_wwvegas
    zi_always
    d3d8lib
    imm32
    Version
    vfw32
    winmm
)
