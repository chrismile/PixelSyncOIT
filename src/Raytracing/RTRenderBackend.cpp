/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "RTRenderBackend.hpp"

void RTRenderBackend::setViewportSize(int width, int height) {
    this->width = width;
    this->height = height;
    image.resize(width * height);
}

void RTRenderBackend::loadTrajectories(const std::string &filename, const Trajectories &trajectories) {
    /**
     * TODO: Convert the trajectories to an internal representation.
     * NOTE: Trajectories can save multiple attributes (i.e., importance criteria).
     * As a simplification, we only use the first attribute for rendering and ignore everything else.
     * The data type 'Trajectories' is defined in "stc/Utils/TrajectoryFile.hpp".
     */
}

void RTRenderBackend::loadTriangleMesh(
        const std::string &filename,
        const std::vector<uint32_t> &indices,
        const std::vector<glm::vec3> &vertices,
        const std::vector<glm::vec3> &vertexNormals,
        const std::vector<float> &vertexAttributes) {
    // TODO: Convert the triangle mesh to an internal representation.
}

void RTRenderBackend::setTransferFunction(
        const std::vector<OpacityPoint> &opacityPoints, const std::vector<ColorPoint_sRGB> &colorPoints_sRGB) {
    // Interpolate the color points in sRGB space. Then, convert the result to linear RGB for rendering:
    //TransferFunctionWindow::sRGBToLinearRGB(glm::vec3(...));
}

void RTRenderBackend::setLineRadius(float lineRadius) {
    // TODO: Adapt the line radius (and ignore this function call for triangle meshes).
}

uint32_t *RTRenderBackend::renderToImage(
        const glm::vec3 &pos, const glm::vec3 &dir, const glm::vec3 &up, const float fovy) {
    /**
     * TODO: Write to the image (RGBA32, pre-multiplied alpha, sRGB).
     * For internal computations, linear RGB is used. The values need to be converted to sRGB before writing to the
     * image. For this, the following function can be used:
     * TransferFunctionWindow::linearRGBTosRGB(glm::vec3(...))
     */

    return &image.front();
}
