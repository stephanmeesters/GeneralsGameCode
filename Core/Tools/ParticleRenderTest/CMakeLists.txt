add_library(corei_particlerendertest INTERFACE)

target_sources(corei_particlerendertest INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BigTextureFileFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BigTextureFileFactory.h"
)

target_precompile_headers(corei_particlerendertest INTERFACE
    [["Precompiled/BasePrecomp.h"]]
    [["WWLib/always.h"]]
)
