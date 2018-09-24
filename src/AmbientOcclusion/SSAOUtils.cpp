//
// Created by christoph on 24.09.18.
//

#include <random>

#include "SSAOUtils.hpp"

/**
 * Generate kernel sample positions inside a unit hemisphere.
 * Basic idea for kernel generation from https://learnopengl.com/Advanced-Lighting/SSAO.
 * However, this code makes sure to discard samples outside of the unit hemisphere (in order to get a real uniform
 * distribution in the unit sphere instead of normalizing the values).
 */
std::vector<glm::vec3> generateSSAOKernel(int numSamples)
{
    std::vector<glm::vec3> kernel;
    kernel.reserve(numSamples);

    std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
    std::default_random_engine rng;

    for (int i = 0; i < numSamples; i++) {
        // Generate uniformly distributed points in the hemisphere
        glm::vec3 samplePosition(uniformDist(rng) * 2.0f - 1.0f, uniformDist(rng) * 2.0f - 1.0f, uniformDist(rng));
        if (glm::length(samplePosition) > 1.0f) {
            // Sample position is outside of unit hemisphere. Take a new sample.
            i--;
            continue;
        }

        // Scale the samples so that a higher number of samples is closer to the origin
        float scale = 0.1f + static_cast<float>(i) / numSamples * 0.9;

        // TODO: Scale
        kernel.push_back(scale * samplePosition);
    }

    return kernel;
}


/**
 * Generates a kernel of random rotation vectors in tangent space.
 */
std::vector<glm::vec3> generateRotationVectors(int numVectors)
{
    std::vector<glm::vec3> rotationVectors;
    rotationVectors.reserve(numVectors);

    std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
    std::default_random_engine rng;

    for (int i = 0; i < numVectors; i++) {
        // Represents rotation around z-axis in tangent space
        glm::vec3 rotationVector(uniformDist(rng) * 2.0f - 1.0f,  uniformDist(rng) * 2.0f - 1.0f, 0.0f);
        rotationVectors.push_back(rotationVector);
    }

    return rotationVectors;
}
