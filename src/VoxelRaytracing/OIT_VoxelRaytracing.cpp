//
// Created by christoph on 02.10.18.
//

#include <cmath>
#include <GL/glew.h>
#include <Graphics/Texture/TextureManager.hpp>

#include "OIT_VoxelRaytracing.hpp"

OIT_VoxelRaytracing::OIT_VoxelRaytracing()
{
    ;
}

void OIT_VoxelRaytracing::create()
{
    renderShader = sgl::ShaderManager->getShaderProgram({ "VoxelRaytracingMain.Compute" });
}

void OIT_VoxelRaytracing::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    sgl::TextureSettings settings;
    renderImage = sgl::TextureManager->createEmptyTexture(width, height, settings);
}

void OIT_VoxelRaytracing::setUniformData()
{
    //renderShader->setShaderStorageBuffer(0, "LineSegmentBuffer", NULL);
    renderShader->setUniformImageTexture(0, renderImage, GL_RGBA8, GL_WRITE_ONLY);
}

inline int nextPowerOfTwo(int x) {
    int powerOfTwo = 1;
    while (powerOfTwo < x) {
        powerOfTwo *= 2;
    }
    return powerOfTwo;
}

// Utility functions for dispatching compute shaders

inline int iceil(int x, int y) {
    return (x - 1) / y + 1;
}

glm::ivec2 rangePadding2D(int w, int h, glm::ivec2 localSize) {
    return glm::ivec2(iceil(w, localSize[0])*localSize[0], iceil(h, localSize[1])*localSize[1]);
}

glm::ivec2 rangePadding1D(int w, int localSize) {
    return glm::ivec2(iceil(w, localSize)*localSize);
}

void OIT_VoxelRaytracing::renderToScreen()
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    sgl::ShaderManager->getMaxComputeWorkGroupCount();
    glm::ivec2 globalWorkSize = rangePadding2D(width, height, glm::ivec2(4, 8)); // last vector: local work group size
    renderShader->dispatchCompute(globalWorkSize.x, globalWorkSize.y);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}