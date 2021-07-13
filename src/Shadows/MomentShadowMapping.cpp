//
// Created by christoph on 10.02.19.
//

#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Texture/Bitmap.hpp>
#include <Graphics/OpenGL/Texture.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/ShaderAttributes.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Utils/AppSettings.hpp>
#include <ImGui/ImGuiWrapper.hpp>
#include "MomentShadowMapping.hpp"

// Internal mode
static int SHADOW_MAP_RESOLUTION = 2048;
static bool usePowerMoments = true;
static int numMoments = 4;
static MBOITPixelFormat pixelFormat = MBOIT_PIXEL_FORMAT_FLOAT_32;
static bool USE_R_RG_RGBA_FOR_MBOIT6 = true;
static float overestimationBeta = 0.0;

void setHighResMomentShadowMapping()
{
    usePowerMoments = false;
    numMoments = 8;
}

MomentShadowMapping::MomentShadowMapping()
{
    loadShaders();

    // Create moment OIT uniform data buffer
    momentUniformData.moment_bias = 5*1e-7;
    momentUniformData.overestimation = overestimationBeta;
    computeWrappingZoneParameters(momentUniformData.wrapping_zone_parameters);
    momentOITUniformBuffer = sgl::Renderer->createGeometryBuffer(sizeof(MomentOITUniformData), &momentUniformData,
            sgl::UNIFORM_BUFFER);

    updateMomentMode();

    // Create clear render data (fullscreen rectangle in normalized device coordinates)
    clearRenderData = sgl::ShaderManager->createShaderAttributes(clearShadowMapShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    sgl::GeometryBufferPtr geomBuffer = sgl::Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
            (void*)&fullscreenQuad.front());
    clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", sgl::ATTRIB_FLOAT, 3);

    resolutionChanged();
}

void MomentShadowMapping::loadShaders()
{
    clearShadowMapShader = sgl::ShaderManager->getShaderProgram(
            {"ClearMomentShadowMap.Vertex", "ClearMomentShadowMap.Fragment"});
}

void MomentShadowMapping::setGatherShaderList(const std::vector<std::string> &shaderIDs)
{
    gatherShaderIDs = shaderIDs;
    sgl::ShaderManager->invalidateShaderCache();
    std::string oldGatherHeader = sgl::ShaderManager->getPreprocessorDefine("OIT_GATHER_HEADER");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"GenerateMomentShadowMap.glsl\"");
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAPPING_MOMENTS_GENERATE", "");
    createShadowMapShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_MOMENTS_GENERATE");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", oldGatherHeader);
}

void MomentShadowMapping::createShadowMapPass(std::function<void()> sceneRenderFunction)
{
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glStencilMask(0);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    sgl::Renderer->bindFBO(shadowMapFBO);
    sgl::Renderer->setViewMatrix(lightViewMatrix);
    sgl::Renderer->setProjectionMatrix(lightProjectionMatrix);
    glViewport(0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
    sgl::Renderer->render(clearRenderData);
    preRenderPass = true;
    sceneRenderFunction();
    preRenderPass = false;
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);




    // TODO: Resolution change of map, depends on size of framebuffer, need to adapt b0 etc. size on slider change

    /*sgl::Renderer->unbindFBO();
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    auto shadowMapDebugFBO = sgl::Renderer->createFBO();

    sgl::TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGBA;
    textureSettings.pixelFormat = GL_RGBA;
    textureSettings.pixelType = GL_UNSIGNED_BYTE;
    auto shadowColorMap = sgl::TextureManager->createEmptyTexture(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, textureSettings);
    shadowMapDebugFBO->bindTexture(shadowColorMap);


    auto blitShadowMapShader = sgl::ShaderManager->getShaderProgram(
            {"BlitMomentShadowMap.Vertex", "BlitMomentShadowMap.Fragment"});


    // Generate render data for fullscreen SSAO texture generation pass
    std::vector<sgl::VertexTextured> fullscreenQuad(sgl::Renderer->createTexturedQuad(
            sgl::AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f))));
    const int stride = sizeof(sgl::VertexTextured);
    sgl::GeometryBufferPtr geomBuffer = sgl::Renderer->createGeometryBuffer(
            sizeof(sgl::VertexTextured)*fullscreenQuad.size(), &fullscreenQuad.front());
    auto generateSSAORenderData = sgl::ShaderManager->createShaderAttributes(blitShadowMapShader);
    generateSSAORenderData->addGeometryBuffer(geomBuffer, "vertexPosition", sgl::ATTRIB_FLOAT, 3, 0, stride);
    generateSSAORenderData->addGeometryBuffer(geomBuffer, "vertexTexcoord", sgl::ATTRIB_FLOAT, 2, sizeof(glm::vec3), stride);

    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        blitShadowMapShader->setUniform("shadowMap", b);
        //blitShadowMapShader->setUniform("shadowMap", bExtra);
    } else {
        blitShadowMapShader->setUniform("shadowMap", b);
    }

    sgl::Renderer->bindFBO(shadowMapDebugFBO);
    sgl::Renderer->render(generateSSAORenderData);

    //glFinish();

    sgl::BitmapPtr bitmap(new sgl::Bitmap(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 32));
    glReadPixels(0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
    //glGetTextureImage(static_cast<sgl::TextureGL*>(shadowMap.get())->getTexture(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
    //        SHADOW_MAP_RESOLUTION*SHADOW_MAP_RESOLUTION*32, (void*)bitmap->getPixels());
    bitmap->savePNG("shadowmap.png", true);*/







    //Renderer->blurTexture(ssaoTexture);
    //sgl::Renderer->unbindFBO();
    sgl::Renderer->unbindFBO();
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
}


void MomentShadowMapping::setUniformValuesCreateShadowMap()
{
    // Different binding points than normal MBOIT
    createShadowMapShader->setUniformImageTexture(3, b0, textureSettingsB0.internalFormat, GL_READ_WRITE, 0, true, 0);
    createShadowMapShader->setUniformImageTexture(4, b, textureSettingsB.internalFormat, GL_READ_WRITE, 0, true, 0);
    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        createShadowMapShader->setUniformImageTexture(5, bExtra, textureSettingsBExtra.internalFormat, GL_READ_WRITE, 0, true, 0);
    }
    sgl::ShaderManager->bindUniformBuffer(2, momentOITUniformBuffer);

    createShadowMapShader->setUniform("logDepthMinShadow", logDepthMinShadow);
    createShadowMapShader->setUniform("logDepthMaxShadow", logDepthMaxShadow);
}

void MomentShadowMapping::setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader)
{
    // Moment texture maps
    transparencyShader->setUniform("zeroth_moment_shadow", b0, 9);
    transparencyShader->setUniform("moments_shadow", b, 10);
    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        transparencyShader->setUniform("extra_moments_shadow", bExtra, 11);
    }

    // Other settings
    transparencyShader->setUniform("lightViewMatrix", lightViewMatrix);
    transparencyShader->setUniform("lightSpaceMatrix", lightSpaceMatrix);
    transparencyShader->setUniform("logDepthMinShadow", logDepthMinShadow);
    transparencyShader->setUniform("logDepthMaxShadow", logDepthMaxShadow);
}

void MomentShadowMapping::setShaderDefines()
{
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_STANDARD");
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAPPING_MOMENTS", "");
    sgl::ShaderManager->invalidateShaderCache();
}

void MomentShadowMapping::resolutionChanged()
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    shadowMapFBO = sgl::Renderer->createFBO();

    sgl::TextureSettings textureSettings;
    textureSettings.internalFormat = GL_DEPTH_COMPONENT;
    shadowMap = sgl::TextureManager->createEmptyTexture(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, textureSettings);
    shadowMapFBO->bindTexture(shadowMap, sgl::DEPTH_ATTACHMENT);
}

bool MomentShadowMapping::renderGUI()
{
    bool reRender = false;

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

    if (ImGui::Combo("Moment Mode Shadow", &momentModeIndex, momentModes, IM_ARRAYSIZE(momentModes))) {
        usePowerMoments = (momentModeIndex / 4) == 0;
        numMoments = momentModesNumMoments[momentModeIndex]; // Count complex moments * 2
        USE_R_RG_RGBA_FOR_MBOIT6 = (momentModeIndex == 2) || (momentModeIndex == 6);
        updateMomentMode();
        reRender = true;
    }

    const char *pixelFormatModes[] = {"Float 32-bit", "UNORM Integer 16-bit"};
    if (ImGui::Combo("Pixel Format Shadow", (int*)&pixelFormat, pixelFormatModes, IM_ARRAYSIZE(pixelFormatModes))) {
        updateMomentMode();
        reRender = true;
    }

    if (ImGui::SliderFloat("Overestimation Shadow", &overestimationBeta, 0.0f, 1.0f, "%.2f")) {
        momentUniformData.overestimation = overestimationBeta;
        momentOITUniformBuffer->subData(0, sizeof(MomentOITUniformData), &momentUniformData);
        reRender = true;
    }


    if (ImGui::SliderInt("", &SHADOW_MAP_RESOLUTION, 256, 4096)) {
        reRender = true;
        resolutionChanged();
        createMomentTextures();
    }
    return reRender;
}







void MomentShadowMapping::reloadShaders()
{
    std::string oldGatherHeader = sgl::ShaderManager->getPreprocessorDefine("OIT_GATHER_HEADER");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"GenerateMomentShadowMap.glsl\"");
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAPPING_MOMENTS_GENERATE", "");
    createShadowMapShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_MOMENTS_GENERATE");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", oldGatherHeader);

    sgl::ShaderManager->invalidateShaderCache();
    clearShadowMapShader = sgl::ShaderManager->getShaderProgram(
            {"ClearMomentShadowMap.Vertex", "ClearMomentShadowMap.Fragment"});
    if (clearRenderData) {
        // Copy data to new shader if this function is not called by the constructor
        clearRenderData = clearRenderData->copy(clearShadowMapShader);
    }

    needsNewTransparencyShader = true;
}

void MomentShadowMapping::createMomentTextures()
{
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
    void *emptyData = calloc(SHADOW_MAP_RESOLUTION * SHADOW_MAP_RESOLUTION, sizeof(float) * 8);

    textureSettingsB0 = sgl::TextureSettings();
    textureSettingsB0.type = sgl::TEXTURE_2D_ARRAY;
    textureSettingsB0.internalFormat = internalFormatB0;
    b0 = sgl::TextureManager->createTexture(
            emptyData, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, depthB0,
            sgl::PixelFormat(pixelFormatB0, GL_FLOAT), textureSettingsB0);

    textureSettingsB = textureSettingsB0;
    textureSettingsB.internalFormat = internalFormatB;
    b = sgl::TextureManager->createTexture(
            emptyData, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, depthB,
            sgl::PixelFormat(pixelFormatB, GL_FLOAT), textureSettingsB);

    if (numMoments == 6 && USE_R_RG_RGBA_FOR_MBOIT6) {
        textureSettingsBExtra = textureSettingsB0;
        textureSettingsBExtra.internalFormat = internalFormatBExtra;
        bExtra = sgl::TextureManager->createTexture(
                emptyData, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, depthBExtra,
                sgl::PixelFormat(internalFormatBExtra, GL_FLOAT), textureSettingsBExtra);
    }

    free(emptyData);
}


void MomentShadowMapping::updateMomentMode()
{
    // 1. Set shader state dependent on the selected mode
    sgl::ShaderManager->addPreprocessorDefine("ROV_SHADOW", "1"); // Always use fragment shader interlock
    sgl::ShaderManager->addPreprocessorDefine("NUM_MOMENTS_SHADOW", sgl::toString(numMoments));
    sgl::ShaderManager->addPreprocessorDefine("SINGLE_PRECISION_SHADOW",
                                              sgl::toString((int)(pixelFormat == MBOIT_PIXEL_FORMAT_FLOAT_32)));
    sgl::ShaderManager->addPreprocessorDefine("TRIGONOMETRIC_SHADOW", sgl::toString((int)(!usePowerMoments)));
    sgl::ShaderManager->addPreprocessorDefine("USE_R_RG_RGBA_FOR_MBOIT6_SHADOW",
                                              sgl::toString((int)USE_R_RG_RGBA_FOR_MBOIT6));

    // 2. Re-load the shaders
    reloadShaders();

    // 3. Load textures
    createMomentTextures();


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


/*void MomentShadowMapping::setNewState(const InternalState &newState)
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

    updateMomentMode();
}*/


void MomentShadowMapping::updateDepthRange()
{
    sgl::AABB3 lightSpaceBB = sceneBB.transformed(lightViewMatrix);
    float minViewZ = lightSpaceBB.getMaximum().z;
    float maxViewZ = lightSpaceBB.getMinimum().z;
    minViewZ = std::max(-minViewZ, LIGHT_NEAR_CLIP_DISTANCE);
    maxViewZ = std::min(-maxViewZ, LIGHT_FAR_CLIP_DISTANCE);
    minViewZ = std::min(minViewZ, LIGHT_FAR_CLIP_DISTANCE);
    maxViewZ = std::max(maxViewZ, LIGHT_NEAR_CLIP_DISTANCE);
    logDepthMinShadow = log(minViewZ);
    logDepthMaxShadow = log(maxViewZ);
}

void MomentShadowMapping::setSceneBoundingBox(const sgl::AABB3 &sceneBB)
{
    this->sceneBB = sceneBB;
    updateDepthRange();
}

void MomentShadowMapping::setLightDirection(const glm::vec3 &lightDirection, const sgl::AABB3 &sceneBoundingBox)
{
    ShadowTechnique::setLightDirection(lightDirection, sceneBoundingBox);
    updateDepthRange();
}
