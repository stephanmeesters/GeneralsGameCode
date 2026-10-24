#include <cstdio>
#include <exception>
#include "d3d8_probe.h"

int main(int argc, char **argv)
{
    try {
        return RunD3D8Probe(argc > 1 ? argv[1] : "triangle.bmp") ? 0 : 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
