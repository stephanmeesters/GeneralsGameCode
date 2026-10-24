#pragma once

// Throws std::runtime_error on device/readback failures. Returns false if pixel
// validation or writing the output image fails. Requires no game assets.
bool RunD3D8Probe(const char *output);
