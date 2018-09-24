//
// Created by christoph on 24.09.18.
//

#ifndef PIXELSYNCOIT_SSAOUTILS_HPP
#define PIXELSYNCOIT_SSAOUTILS_HPP

#include <vector>
#include <glm/glm.hpp>


/**
 * Generate kernel sample positions inside a unit hemisphere.
 * Basic idea for kernel generation from https://learnopengl.com/Advanced-Lighting/SSAO.
 * However, this code makes sure to discard samples outside of the unit hemisphere (in order to get a real uniform
 * distribution in the unit sphere instead of normalizing the values).
 */
std::vector<glm::vec3> generateSSAOKernel(int numSamples);

/**
 * Generates a kernel of random rotation vectors in tangent space.
 */
std::vector<glm::vec3> generateRotationVectors(int numVectors);

#endif //PIXELSYNCOIT_SSAOUTILS_HPP
