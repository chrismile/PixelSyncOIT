//
// Created by christoph on 24.09.18.
//

#include <GL/glew.h>

#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/ShaderAttributes.hpp>
#include <Utils/AppSettings.hpp>

#include "SSAOUtils.hpp"
#include "SSAO.hpp"

using namespace sgl;

void SSAOHelper::init()
{
    size_t rotationVectorKernelLength = 4;
    size_t rotationVectorKernelSize = rotationVectorKernelLength*rotationVectorKernelLength;
    // GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT (tile random rotation vectors over screen), GL_NEAREST
    std::vector<glm::vec3> rotationVectors = generateRotationVectors(rotationVectorKernelSize);
    sampleKernel = generateSSAOKernel(64); // TODO: Variable number
    TextureSettings rvecTextureSettings(GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT);
    rvecTextureSettings.internalFormat = GL_RGB16F;
    rvecTextureSettings.pixelFormat = GL_RGB;
    rvecTextureSettings.pixelType = GL_FLOAT;
    rotationVectorTexture = TextureManager->createTexture((void*)&rotationVectors.front(),
            rotationVectorKernelLength, rotationVectorKernelLength, rvecTextureSettings);

    loadShaders();


    // Generate render data for fullscreen SSAO texture generation pass
    std::vector<VertexTextured> fullscreenQuad(Renderer->createTexturedQuad(
            AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f))));
    const int stride = sizeof(VertexTextured);
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(
            sizeof(VertexTextured)*fullscreenQuad.size(), &fullscreenQuad.front());
    generateSSAORenderData = ShaderManager->createShaderAttributes(generateSSAOTextureShader);
    generateSSAORenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3, 0, stride);
    generateSSAORenderData->addGeometryBuffer(geomBuffer, "vertexTexcoord", ATTRIB_FLOAT, 2, sizeof(glm::vec3), stride);
}

void SSAOHelper::loadShaders()
{
    geometryPassShader = ShaderManager->getShaderProgram({"GeometryPassSSAO.Vertex", "GeometryPassSSAO.Fragment"});
    generateSSAOTextureShader = ShaderManager->getShaderProgram({"GenerateSSAOTexture.Vertex", "GenerateSSAOTexture.Fragment"});

    generateSSAOTextureShader->setUniformArray("samples", &sampleKernel.front(), sampleKernel.size());
}

void SSAOHelper::resolutionChanged()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    gBufferFBO = Renderer->createFBO();

    TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGB16F;
    textureSettings.pixelFormat = GL_RGB;
    textureSettings.pixelType = GL_FLOAT;
    positionTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    gBufferFBO->bindTexture(positionTexture, COLOR_ATTACHMENT0);

    textureSettings.internalFormat = GL_RGB16F;
    normalTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    gBufferFBO->bindTexture(normalTexture, COLOR_ATTACHMENT1);

    depthStencilRBO = Renderer->createRBO(width, height, DEPTH24_STENCIL8);
    gBufferFBO->bindRenderbuffer(depthStencilRBO, DEPTH_STENCIL_ATTACHMENT);


    generateSSAOTextureFBO = Renderer->createFBO();
    TextureSettings ssaoTextureSettings = textureSettings;
    ssaoTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    generateSSAOTextureFBO->bindTexture(ssaoTexture);
}

void SSAOHelper::preRender(std::function<void()> sceneRenderFunction)
{
    // Setup uniform data
    generateSSAOTextureShader->setUniform("gPositionTexture", positionTexture, 0);
    generateSSAOTextureShader->setUniform("gNormalTexture", normalTexture, 1);
    generateSSAOTextureShader->setUniform("rotationVectorTexture", rotationVectorTexture, 2);


    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // 1. Geometry pass: Create G-buffer data
    Renderer->bindFBO(gBufferFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    preRenderPass = true;
    sceneRenderFunction();
    preRenderPass = false;

    // 2. Use G-buffer to render noisy SSAO texture
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    Renderer->bindFBO(generateSSAOTextureFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    Renderer->render(generateSSAORenderData);
    Renderer->setProjectionMatrix(matrixIdentity());

    // 3. Blur SSAO texture
    Renderer->blurTexture(ssaoTexture);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // 4. Lighting render pass using SSAO texture to read ambient occlusion factor (done by MainApp)
}