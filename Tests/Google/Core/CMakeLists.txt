# Core GameEngine code needs a game to compile, so z_googletest compiles these sources.
set(COREI_GOOGLETEST_SRC
    GoogleTestMain.cpp
    Rendering/EXAMPLE_TEMPORARY_ClearTest.cpp
    Rendering/EXAMPLE_TEMPORARY_LaserTest.cpp
    Rendering/EXAMPLE_TEMPORARY_ParticlesTest.cpp
    Rendering/EXAMPLE_TEMPORARY_Render2DBlendingTest.cpp
    Rendering/EXAMPLE_TEMPORARY_Render2DShapesTest.cpp
    Rendering/EXAMPLE_TEMPORARY_Render2DTextureTest.cpp
    Rendering/EXAMPLE_TEMPORARY_TextTest.cpp
    Rendering/EXAMPLE_TEMPORARY_TracerTest.cpp
    Rendering/EXAMPLE_TEMPORARY_TriangleTest.cpp
    Rendering/RenderingTestFixture.cpp
    Rendering/RenderingTestFixture.h
)

add_library(corei_googletest INTERFACE)

target_sources(corei_googletest INTERFACE ${COREI_GOOGLETEST_SRC})

target_link_libraries(corei_googletest INTERFACE
    comctl32
    GTest::gtest
    imm32
    stb
    vfw32
    winmm
)

# The rendering tests ask bgfx which renderer it runs.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(corei_googletest INTERFACE bgfx)
endif()
