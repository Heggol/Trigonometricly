#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec2 position;
};

namespace sine {
    std::vector<Vertex> generateSineWave(float t_amplitude, float t_frequency, float t_phase, int t_pointCount);
}