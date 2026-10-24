#include <gtest/gtest.h>

#include "../../Rendering/d3d8_probe.h"

// Requires a D3D8 device. CI runs this suite under Wine/DXVK/lavapipe on Linux.
TEST(Rendering, D3D8Triangle)
{
    ASSERT_TRUE(RunD3D8Probe("triangle.bmp"));
}
