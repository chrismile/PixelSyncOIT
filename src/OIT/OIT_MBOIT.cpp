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
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "OIT_MBOIT_Utils.hpp"
#include "OIT_MBOIT.hpp"

using namespace sgl;

struct MomentOITUniformData
{
    glm::vec4 wrapping_zone_parameters;
    float overestimation;
    float moment_bias;
};

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

enum MBOITPixelFormat {
    MBOIT_PIXEL_FORMAT_FLOAT_32, MBOIT_PIXEL_FORMAT_UNORM_16
};

// Internal mode
static bool usePowerMoments = true;
static int numMoments = 4;
static MBOITPixelFormat pixelFormat = MBOIT_PIXEL_FORMAT_FLOAT_32;

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

    ShaderManager->addPreprocessorDefine("SINGLE_PRECISION", "1");
    ShaderManager->addPreprocessorDefine("NUM_MOMENTS", "4");
    ShaderManager->addPreprocessorDefine("SINGLE_PRECISION", "1");
    ShaderManager->addPreprocessorDefine("ROV", "1");

    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass1.glsl\"");
    mboitPass1Shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass2.glsl\"");
    mboitPass2Shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    blendShader = ShaderManager->getShaderProgram({"MBOITBlend.Vertex", "MBOITBlend.Fragment"});
    //clearShader = ShaderManager->getShaderProgram({"MBOITClear.Vertex", "MBOITClear.Fragment"});

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(blendShader);

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
    momentOITUniformBuffer = Renderer->createGeometryBuffer(sizeof(MomentOITUniformData), &uniformData, UNIFORM_BUFFER);
}

void OIT_MBOIT::setGatherShader(const std::string &name)
{
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass1.glsl\"");
    mboitPass1Shader = ShaderManager->getShaderProgram({name + ".Vertex", name + ".Fragment"});
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MBOITPass2.glsl\"");
    mboitPass2Shader = ShaderManager->getShaderProgram({name + ".Vertex", name + ".Fragment"});
}

void OIT_MBOIT::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    this->sceneFramebuffer = sceneFramebuffer;

    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    int depth = 1;

    // Highest memory requirement: (width * height * sizeof(DATATYPE) * #maxBufferEntries * #moments
    void *emptyData = calloc(width * height, sizeof(float) * 4 * 8);

    textureSettingsB0 = TextureSettings();
    textureSettingsB0.pixelType = GL_FLOAT;

    textureSettingsB0.pixelFormat = GL_RED;
    textureSettingsB0.internalFormat = GL_R32F; // GL_R16
    b0 = TextureManager->createTexture3D(emptyData, width, height, depth, textureSettingsB0);

    textureSettingsB = textureSettingsB0;
    textureSettingsB.pixelFormat = GL_RGBA;
    textureSettingsB.internalFormat = GL_RGBA32F; // GL_RGBA16
    b = TextureManager->createTexture3D(emptyData, width, height, depth, textureSettingsB);

    free(emptyData);


    blendFBO = Renderer->createFBO();
    TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGBA32F;
    blendRenderTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    blendFBO->bindTexture(blendRenderTexture);
    blendFBO->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);
}

void OIT_MBOIT::updateMomentMode()
{
    ;
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

    mboitPass1Shader->setUniformBuffer(1, "MomentOITUniformData", momentOITUniformBuffer);
    mboitPass2Shader->setUniformBuffer(1, "MomentOITUniformData", momentOITUniformBuffer);
}

void OIT_MBOIT::renderGUI()
{
    ImGui::Separator();

    const char *momentModes[] = {"Power Moments: 4", "Power Moments: 6", "Power Moments: 8",
                                 "Trigonometric Moments: 2", "Trigonometric Moments: 3", "Trigonometric Moments: 4"};
    static int momentModeIndex = 0;
    if (ImGui::Combo("Moment Mode", &momentModeIndex, momentModes, IM_ARRAYSIZE(momentModes))) {
        usePowerMoments = (momentModeIndex / 3) == 0;
        numMoments = usePowerMoments ? momentModeIndex*2 + 4 : momentModeIndex - 1;
        updateMomentMode();
        reRender = true;
    }

    const char *pixelFormatModes[] = {"Float 32-bit", "UNORM Integer 16-bit"};
    if (ImGui::Combo("Pixel Format", (int*)&pixelFormat, pixelFormatModes, IM_ARRAYSIZE(pixelFormatModes))) {
        updateMomentMode();
        reRender = true;
    }
}



void OIT_MBOIT::setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB)
{
    sgl::Sphere sphere = sgl::Sphere(screenSpaceBB.getCenter(), screenSpaceBB.getExtent().length());
    float minViewZ = sphere.center.z - sphere.radius;
    float maxViewZ = sphere.center.z + sphere.radius;
    float logmin = log(minViewZ);
    float logmax = log(maxViewZ);
    // TODO: Negative values
    logmin = log(0.1f);
    logmax = log(10.0f);
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
    Renderer->clearFramebuffer();
    Renderer->render(blitRenderData);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glDepthMask(GL_TRUE);
}
