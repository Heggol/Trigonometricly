#include "sine.hpp"
#include <vector>
#include <cmath>
#include <glm/glm.hpp>

// Generate sine wave animated on the horizontal axis
std::vector<Vertex> sine::generateSineWave(float t_amplitude, float t_frequency, float t_phase, int t_pointCount) {
    std::vector<Vertex> sineVertices;
    for (int i = 0; i < t_pointCount; ++i) {
        float x = static_cast<float>(i) / (t_pointCount - 1) * 2.0f - 1.0f; // [-1, 1]
        float y = t_amplitude * sinf(t_frequency * x * 2.0f * M_PI + t_phase);
        sineVertices.push_back({ glm::vec2(x, y) });
    }
    return sineVertices;
}