//
// Created by christoph on 09.09.18.
//

#include "OIT_MBOIT.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "TilingMode.hpp"
#include "OIT_MBOIT_Utils.hpp"
#include "OIT_MBOIT.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
static bool useStencilBuffer = true;

enum MBOITPixelFormat {
    MBOIT_PIXEL_FORMAT_FLOAT_32, MBOIT_PIXEL_FORMAT_UNORM_16
};

// Internal mode
static bool usePowerMoments = true;
static int numMoments = 4;
static MBOITPixelFormat pixelFormat = MBOIT_PIXEL_FORMAT_FLOAT_32;
static bool USE_R_RG_RGBA_FOR_MBOIT6 = true;
static float overestimationBeta = 0.1;

OIT_MBOIT::OIT_MBOIT()
{
    create();
}

void OIT_MBOIT::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_MBOIT::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    // Create moment OIT uniform data buffer
    momentUniformData.moment_bias = 5*1e-7;
    momentUniformData.overestimation = overestimationBeta;
    computeWrappingZoneParameters(momentUniformData.wrapping_zone_parameters);
    momentOITUniformBuffer = Renderer->createGeometryBuffer(sizeof(MomentOITUniformData), &momentUniformData, UNIFORM_BUFFER);

    updateMomentMode();

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(blendShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
                                                                  (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_MBOIT::setGatherShaderList(const std::list<std::string> &shaderIDs)
{
    gatherShaderIDs = shaderIDs;
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass1.glsl\"");
    mboitPass1Shader = ShaderManager->getShaderProgram(gatherShaderIDs);
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass2.glsl\"");
    mboitPass2Shader = ShaderManager->getShaderProgram(gatherShaderIDs);
}

void OIT_MBOIT::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
        sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    this->sceneFramebuffer = sceneFramebuffer;
    this->sceneDepthRBO = sceneDepthRBO;

    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    // Create accumulator framebuffer object & texture
    blendFBO = Renderer->createFBO();
    TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGBA32F;
    blendRenderTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    blendFBO->bindTexture(blendRenderTexture);
    blendFBO->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);

    updateMomentMode();
}

void OIT_MBOIT::reloadShaders()
{
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass1.glsl\"");
    mboitPass1Shader = ShaderManager->getShaderProgram(gatherShaderIDs);
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass2.glsl\"");
    mboitPass2Shader = ShaderManager->getShaderProgram(gatherShaderIDs);
    blendShader = ShaderManager->getShaderProgram({"MBOITBlend.Vertex", "MBOITBlend.Fragment"});
    if (blitRenderData) {
        // Copy data to new shader if this function is not called by the constructor
        blitRenderData = blitRenderData->copy(blendShader);
    }
}

void OIT_MBOIT::updateMomentMode()
{
    // 1. Set shader state dependent on the selected mode
    ShaderManager->addPreprocessorDefine("ROV", "1"); // Always use fragment shader interlock
    ShaderManager->addPreprocessorDefine("NUM_MOMENTS", toString(numMoments));
    ShaderManager->addPreprocessorDefine("SINGLE_PRECISION", toString((int)(pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32)));
    ShaderManager->addPreprocessorDefine("TRIGONOMETRIC", toString((int)(!usePowerMoments)));
    ShaderManager->addPreprocessorDefine("USE_R_RG_RGBA_FOR_MBOIT6", toString((int)USE_R_RG_RGBA_FOR_MBOIT6));

    // 2. Re-load the shaders
    reloadShaders();

    // 3. Load textures
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    //const GLint internalFormat1 = pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32 ? GL_R32F : GL_R16;
    const GLint internalFormat1 = GL_R32F;
    const GLint internalFormat2 = pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32 ? GL_RG32F : GL_RG16;
    const GLint internalFormat4 = pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32 ? GL_RGBA32F : GL_RGBA16;
    const GLint pixelFormat1 = GL_RED;
    const GLint pixelFormat2 = GL_RG;
    const GLint pixelFormat4 = GL_RGBA;

    int depthB0 = 1;
    int depthB = 1;
    int depthBExtra = 0;
    GLint internalFormatB0 = internalFormat1;
    GLint internalFormatB = internalFormat4;
    GLint internalFormatBExtra = 0;
    GLint pixelFormatB0 = pixelFormat1;
    GLint pixelFormatB = pixelFormat4;
    GLint pixelFormatBExtra = 0;

    if (numMoments == 6) {
        if (USE_R_RG_RGBA_FOR_MBOIT6) {
            depthBExtra = 1;
            internalFormatB = internalFormat2;
            pixelFormatB = pixelFormat2;
            internalFormatBExtra = internalFormat4;
            pixelFormatBExtra = pixelFormat4;
        } else {
            depthB = 3;
            internalFormatB = internalFormat2;
            pixelFormatB = pixelFormat2;
        }
    } else if (numMoments == 8) {
        depthB = 2;
    }

    // Highest memory requirement: width * height * sizeof(DATATYPE) * #moments
    void *emptyData = calloc(width * height, sizeof(float) * 8);

    textureSettingsB0 = TextureSettings();
    textureSettingsB0.type = TEXTURE_2D_ARRAY;
    textureSettingsB0.pixelType = GL_FLOAT;
    textureSettingsB0.pixelFormat = pixelFormatB0;
    textureSettingsB0.internalFormat = internalFormatB0;
    b0 = TextureManager->createTexture(emptyData, width, height, depthB0, textureSettingsB0);

    textureSettingsB = textureSettingsB0;
    textureSettingsB.pixelFormat = pixelFormatB;
    textureSettingsB.internalFormat = internalFormatB;
    b = TextureManager->createTexture(emptyData, width, height, depthB, textureSettingsB);

    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        textureSettingsBExtra = textureSettingsB0;
        textureSettingsBExtra.pixelFormat = pixelFormatBExtra;
        textureSettingsBExtra.internalFormat = internalFormatBExtra;
        bExtra = TextureManager->createTexture(emptyData, width, height, depthBExtra, textureSettingsBExtra);
    }

    free(emptyData);


    // Set algorithm-dependent bias
    if (usePowerMoments) {
        if (numMoments == 4 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 6*1e-4; // 6*1e-5
        } else if (numMoments == 4 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 5*1e-7; // 5*1e-7
        } else if (numMoments == 6 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 6*1e-3; // 6*1e-4
        } else if (numMoments == 6 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 5*1e-6; // 5*1e-6
        } else if (numMoments == 8 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 2.5*1e-2; // 2.5*1e-3
        } else if (numMoments == 8 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 5*1e-5; // 5*1e-5
        }
    } else {
        if (numMoments == 4 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 4*1e-3; // 4*1e-4
        } else if (numMoments == 4 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 4*1e-7; // 4*1e-7
        } else if (numMoments == 6 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 6.5*1e-3; // 6.5*1e-4
        } else if (numMoments == 6 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 8*1e-6; // 8*1e-7
        } else if (numMoments == 8 && pixelFormat == MBOIT_PIXEL_FORMAT_UNORM_16) {
            momentUniformData.moment_bias = 8.5*1e-3; // 8.5*1e-4
        } else if (numMoments == 8 && pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32) {
            momentUniformData.moment_bias = 1.5*1e-5; // 1.5*1e-6;
        }
    }

    momentOITUniformBuffer->subData(0, sizeof(MomentOITUniformData), &momentUniformData);
}


void OIT_MBOIT::setNewState(const InternalState &newState)
{
    numMoments = newState.oitAlgorithmSettings.getIntValue("numMoments");
    pixelFormat = newState.oitAlgorithmSettings.getValue("pixelFormat") == "Float"
            ? MBOIT_PIXEL_FORMAT_FLOAT_32 : MBOIT_PIXEL_FORMAT_UNORM_16;
    if (numMoments == 6) {
        USE_R_RG_RGBA_FOR_MBOIT6 = newState.oitAlgorithmSettings.getBoolValue("USE_R_RG_RGBA_FOR_MBOIT6");
    }
    usePowerMoments = newState.oitAlgorithmSettings.getBoolValue("usePowerMoments");

    if (newState.oitAlgorithmSettings.getValueOpt("overestimationBeta", overestimationBeta)) {
        momentUniformData.overestimation = overestimationBeta;
        // subData already called in updateMomentMode
        //momentOITUniformBuffer->subData(0, sizeof(MomentOITUniformData), &momentUniformData);
    } else {
        overestimationBeta = 0.1f;
        momentUniformData.overestimation = overestimationBeta;
    }

    useStencilBuffer = newState.useStencilBuffer;

    updateMomentMode();
}



void OIT_MBOIT::setUniformData()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    mboitPass1Shader->setUniformImageTexture(0, b0, textureSettingsB0.internalFormat, GL_READ_WRITE, 0, true, 0);
    mboitPass1Shader->setUniformImageTexture(1, b, textureSettingsB.internalFormat, GL_READ_WRITE, 0, true, 0);
    //mboitPass1Shader->setUniform("viewportW", width);

    mboitPass2Shader->setUniformImageTexture(0, b0, textureSettingsB0.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
    mboitPass2Shader->setUniformImageTexture(1, b, textureSettingsB.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
    //mboitPass2Shader->setUniform("viewportW", width);

    blendShader->setUniformImageTexture(0, b0, textureSettingsB0.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
    blendShader->setUniformImageTexture(1, b, textureSettingsB.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
    blendShader->setUniform("transparentSurfaceAccumulator", blendRenderTexture, 0);
    //blendShader->setUniform("viewportW", width);

    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        mboitPass1Shader->setUniformImageTexture(2, bExtra, textureSettingsBExtra.internalFormat, GL_READ_WRITE, 0, true, 0);
        mboitPass2Shader->setUniformImageTexture(2, bExtra, textureSettingsBExtra.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
        blendShader->setUniformImageTexture(2, bExtra, textureSettingsBExtra.internalFormat, GL_READ_WRITE, 0, true, 0); // GL_READ_ONLY? -> Shader
    }

    mboitPass1Shader->setUniformBuffer(1, "MomentOITUniformData", momentOITUniformBuffer);
    mboitPass2Shader->setUniformBuffer(1, "MomentOITUniformData", momentOITUniformBuffer);
}

void OIT_MBOIT::renderGUI()
{
    ImGui::Separator();

    // USE_R_RG_RGBA_FOR_MBOIT6
    const char *momentModes[] = {"Power Moments: 4", "Power Moments: 6 (Layered)", "Power Moments: 6 (R_RG_RGBA)",
                                 "Power Moments: 8", "Trigonometric Moments: 2", "Trigonometric Moments: 3 (Layered)",
                                 "Trigonometric Moments: 3 (R_RG_RGBA)", "Trigonometric Moments: 4"};
    const int momentModesNumMoments[] = {4, 6, 6, 8, 4, 6, 6, 8};
    static int momentModeIndex = -1;
    if (true) { // momentModeIndex == -1
        // Initialize
        momentModeIndex = usePowerMoments ? 0 : 4;
        momentModeIndex += numMoments/2 - 2;
        momentModeIndex += (USE_R_RG_RGBA_FOR_MBOIT6 && numMoments == 6) ? 1 : 0;
        momentModeIndex += (numMoments == 8) ? 1 : 0;
    }

    if (ImGui::Combo("Moment Mode", &momentModeIndex, momentModes, IM_ARRAYSIZE(momentModes))) {
        usePowerMoments = (momentModeIndex / 4) == 0;
        numMoments = momentModesNumMoments[momentModeIndex]; // Count complex moments * 2
        USE_R_RG_RGBA_FOR_MBOIT6 = (momentModeIndex == 2) || (momentModeIndex == 6);
        updateMomentMode();
        reRender = true;
    }

    const char *pixelFormatModes[] = {"Float 32-bit", "UNORM Integer 16-bit"};
    if (ImGui::Combo("Pixel Format", (int*)&pixelFormat, pixelFormatModes, IM_ARRAYSIZE(pixelFormatModes))) {
        updateMomentMode();
        reRender = true;
    }

    if (ImGui::SliderFloat("Overestimation", &overestimationBeta, 0.0f, 1.0f, "%.2f")) {
        momentUniformData.overestimation = overestimationBeta;
        momentOITUniformBuffer->subData(0, sizeof(MomentOITUniformData), &momentUniformData);
        reRender = true;
    }

    //if (selectTilingModeUI()) {
    //    reloadShaders();
    //    reRender = true;
    //}
}



void OIT_MBOIT::setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB, sgl::CameraPtr &camera)
{
    float minViewZ = screenSpaceBB.getMaximum().z;
    float maxViewZ = screenSpaceBB.getMinimum().z;
    minViewZ = std::max(-minViewZ, camera->getNearClipDistance());
    maxViewZ = std::min(-maxViewZ, camera->getFarClipDistance());
    minViewZ = std::min(minViewZ, camera->getFarClipDistance());
    maxViewZ = std::max(maxViewZ, camera->getNearClipDistance());
    float logmin = log(minViewZ);
    float logmax = log(maxViewZ);
    mboitPass1Shader->setUniform("logDepthMin", logmin);
    mboitPass1Shader->setUniform("logDepthMax", logmax);
    mboitPass2Shader->setUniform("logDepthMin", logmin);
    mboitPass2Shader->setUniform("logDepthMax", logmax);
}

void OIT_MBOIT::gatherBegin()
{
    setUniformData();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void OIT_MBOIT::renderScene()
{
    glEnable(GL_DEPTH_TEST);

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    pass = 1;
    renderSceneFunction();
}

void OIT_MBOIT::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    Renderer->bindFBO(blendFBO);
    Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT, Color(0,0,0,0));

    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

    pass = 2;
    renderSceneFunction();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void OIT_MBOIT::renderToScreen()
{
    glDisable(GL_DEPTH_TEST);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());


    if (useStencilBuffer) {
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
    }

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

    Renderer->bindFBO(sceneFramebuffer);
    Renderer->render(blitRenderData);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glDepthMask(GL_TRUE);
}
