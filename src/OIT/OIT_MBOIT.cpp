//
// Created by christoph on 09.09.18.
//

#include "OIT_MBOIT.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_MBOIT_Utils.hpp"
#include "OIT_MBOIT.hpp"

using namespace sgl;

struct MomentOITUniformData
{
    glm::vec4 wrapping_zone_parameters;
    float overestimation;
    float moment_bias;
} MomentOIT;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_MBOIT::OIT_MBOIT()
{
    clearBitSet = true;
    create();
}

void OIT_MBOIT::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_PixelSync::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITGather.glsl\"");
    ShaderManager->addPreprocessorDefine("SINGLE_PRECISION", "1");
    ShaderManager->addPreprocessorDefine("NUM_MOMENTS", "4");
    ShaderManager->addPreprocessorDefine("SINGLE_PRECISION", "1");
    ShaderManager->addPreprocessorDefine("ROV", "1");

    gatherShader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    blitShader = ShaderManager->getShaderProgram({"MBOITResolve.Vertex", "MBOITResolve.Fragment"}, true);
    //clearShader = ShaderManager->getShaderProgram({"MBOITClear.Vertex", "MBOITClear.Fragment"});

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(blitShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
            (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

    MomentOITUniformData uniformData;
    uniformData.moment_bias = 5*1e-7;
    uniformData.overestimation = 0.25f;
    computeWrappingZoneParameters(uniformData.wrapping_zone_parameters);
    GeometryBufferPtr momentOITUniformBuffer = Renderer->createGeometryBuffer(sizeof(MomentOITUniformData),
            &uniformData, UNIFORM_BUFFER);
    blitShader->setUniformBuffer(0, "MomentOITUniformData", momentOITUniformBuffer);

    //clearRenderData = ShaderManager->createShaderAttributes(clearShader);
    //geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
    // (void*)&fullscreenQuad.front(), SHADER_STORAGE_BUFFER);
    //clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_MBOIT::resolutionChanged()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    int depth = 1;

    // Highest memory requirement:
    size_t bufferSizeBytes = sizeof(float) * 4 * 8 * width * height;
    void *emptyData = (void*)calloc(width * height, sizeof(float) * 4 * 8);
    memset(emptyData, 0, bufferSizeBytes);

    TextureSettings textureSettings;
    textureSettings.pixelType = GL_FLOAT;

    textureSettings.pixelFormat = GL_RED;
    textureSettings.internalFormat = GL_R32F; // GL_R16
    TexturePtr b0 = TextureManager->createTexture3D(emptyData, width, height, depth, textureSettings);

    textureSettings.pixelFormat = GL_RGBA;
    textureSettings.internalFormat = GL_RGBA32F; // GL_RGBA16
    TexturePtr b = TextureManager->createTexture3D(emptyData, width, height, depth, textureSettings);

    free(emptyData);

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    blitShader->setUniform("viewportW", width);
    blitShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    //clearShader->setUniform("viewportW", width);
    //clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    // Buffer has to be cleared again
    clearBitSet = true;
}

void OIT_MBOIT::gatherBegin()
{
    /*glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    //if (clearBitSet) {
    //    Renderer->render(clearRenderData);
    //    clearBitSet = false;
    //}
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LESS);
    //glDepthMask(GL_FALSE);
    GL_R8_SNORM;

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
    }*/
}

void OIT_MBOIT::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_MBOIT::renderToScreen()
{
    /*Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    if (useStencilBuffer) {
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
    }

    Renderer->render(blitRenderData);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glDepthMask(GL_TRUE);*/
}
