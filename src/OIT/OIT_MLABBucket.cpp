//
// Created by christoph on 30.10.18.
//

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "TilingMode.hpp"
#include "OIT_MLABBucket.hpp"
#include "BufferSizeWatch.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
static bool useStencilBuffer = true;

// Maximum number of buckets
static int numBuckets = 1;

// Maximum number of nodes per bucket
static int nodesPerBucket = 4;

// How to assign pixels to buffers
static int bucketMode = 4;

// Opacity threshold for lower back buffer boundary
static float lowerBackBufferOpacity = 0.2;

// Opacity threshold for upper back buffer boundary
static float upperBackBufferOpacity = 0.98;


OIT_MLABBucket::OIT_MLABBucket()
{
    clearBitSet = true;
    create();
}

void OIT_MLABBucket::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_MLABBucket::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MLABBucketGather.glsl\"");
    ShaderManager->addPreprocessorDefine("MLAB_DEPTH_OPACITY_BUCKETS", "");

    updateLayerMode();

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(resolveShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
                                                                  (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

    clearRenderData = ShaderManager->createShaderAttributes(clearShader);
    geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
                                                (void*)&fullscreenQuad.front());
    clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);


}

void OIT_MLABBucket::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                 sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    this->sceneFramebuffer = sceneFramebuffer;
    this->sceneTexture = sceneTexture;
    this->sceneDepthRBO = sceneDepthRBO;

    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    getScreenSizeWithTiling(width, height);

    size_t bufferSizeBytes = (sizeof(uint32_t) + sizeof(float)) * numBuckets * nodesPerBucket * width * height;
    fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    size_t minDepthBufferSizeBytes = sizeof(float) * 2 * width * height;
    minDepthBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    minDepthBuffer = Renderer->createGeometryBuffer(minDepthBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    /*textureSettingsB0 = TextureSettings();
    textureSettingsB0.type = TEXTURE_2D_ARRAY;
    textureSettingsB0.pixelType = GL_FLOAT;
    textureSettingsB0.pixelFormat = pixelFormatB0;
    textureSettingsB0.internalFormat = internalFormatB0;
    b0 = TextureManager->createTexture(emptyData, width, height, depthB0, textureSettingsB0);*/

    setCurrentAlgorithmBufferSizeBytes(bufferSizeBytes + minDepthBufferSizeBytes);


    boundingBoxesTextureSettings = TextureSettings();
    boundingBoxesTextureSettings.type = TEXTURE_2D_ARRAY;
    boundingBoxesTextureSettings.pixelType = GL_FLOAT;
    boundingBoxesTextureSettings.pixelFormat = GL_RGBA;
    boundingBoxesTextureSettings.internalFormat = GL_RGBA32F; // TODO: GL_RGBA16
    boundingBoxesTextureSettings.textureMinFilter = GL_NEAREST;
    boundingBoxesTextureSettings.textureMagFilter = GL_NEAREST;

    numUsedBucketsTextureSettings = TextureSettings();
    numUsedBucketsTextureSettings.type = TEXTURE_2D;
    numUsedBucketsTextureSettings.pixelType = GL_UNSIGNED_INT;
    numUsedBucketsTextureSettings.pixelFormat = GL_RED_INTEGER;
    numUsedBucketsTextureSettings.internalFormat = GL_R8UI; // GL_RGBA16
    numUsedBucketsTextureSettings.textureMinFilter = GL_NEAREST;
    numUsedBucketsTextureSettings.textureMagFilter = GL_NEAREST;

    transmittanceTextureSettings = TextureSettings();
    transmittanceTextureSettings.type = TEXTURE_2D;
    transmittanceTextureSettings.pixelType = GL_FLOAT;
    transmittanceTextureSettings.pixelFormat = GL_RED;
    transmittanceTextureSettings.internalFormat = GL_R32F;
    transmittanceTextureSettings.textureMinFilter = GL_NEAREST;
    transmittanceTextureSettings.textureMagFilter = GL_NEAREST;

    boundingBoxesTexture = sgl::TextureManager->createTexture(NULL, width, height, numBuckets, boundingBoxesTextureSettings);
    numUsedBucketsTexture = sgl::TextureManager->createTexture(NULL, width, height, numUsedBucketsTextureSettings);
    transmittanceTexture = sgl::TextureManager->createTexture(NULL, width, height, transmittanceTextureSettings);


    // Buffer has to be cleared again
    clearBitSet = true;
}



void OIT_MLABBucket::renderGUI()
{
    ImGui::Separator();

    if (ImGui::SliderInt("Num Buckets", &numBuckets, 1, 8)) {
        updateLayerMode();
        reRender = true;
    }

    if (ImGui::SliderFloat("Back Bucket Lower Opacity", &lowerBackBufferOpacity, 0.0f, 1.0f)) {
        updateLayerMode();
        reRender = true;
    }

    if (ImGui::SliderFloat("Back Bucket Upper Opacity", &upperBackBufferOpacity, 0.0f, 1.0f)) {
        updateLayerMode();
        reRender = true;
    }

    if (ImGui::SliderInt("Nodes per Bucket", &nodesPerBucket, 1, 8)) {
        updateLayerMode();
        reRender = true;
    }

    const char *bucketModes[] = {"Depth Buckets", "Opacity Buckets", "Combined Buckets", "Transmittance Buckets",
                                 "Min Depth Buckets"};
    if (ImGui::Combo("Pixel Format", (int*)&bucketMode, bucketModes, IM_ARRAYSIZE(bucketModes))) {
        reloadShaders();
        clearBitSet = true;
        reRender = true;
    }

    if (selectTilingModeUI())
    {
        reloadShaders();
        clearBitSet = true;
        reRender = true;
    }

}

void OIT_MLABBucket::updateLayerMode()
{
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("BUFFER_SIZE", numBuckets * nodesPerBucket);
    ShaderManager->addPreprocessorDefine("NUM_BUCKETS", numBuckets);
    ShaderManager->addPreprocessorDefine("NODES_PER_BUCKET", nodesPerBucket);
    reloadShaders();
    resolutionChanged(sceneFramebuffer, sceneTexture, sceneDepthRBO);
}

void OIT_MLABBucket::setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB, sgl::CameraPtr &camera)
{
    // Add offset of 0.1 for e.g. point data sets where additonal vertices may be added in the shader for quads.
    float minViewZ = screenSpaceBB.getMaximum().z + 0.1;
    float maxViewZ = screenSpaceBB.getMinimum().z - 0.1;
    minViewZ = std::max(-minViewZ, camera->getNearClipDistance());
    maxViewZ = std::min(-maxViewZ, camera->getFarClipDistance());
    minViewZ = std::min(minViewZ, camera->getFarClipDistance());
    maxViewZ = std::max(maxViewZ, camera->getNearClipDistance());
    float logmin = log(minViewZ);
    float logmax = log(maxViewZ);
    if (gatherShader->hasUniform("logDepthMin")) {
        gatherShader->setUniform("logDepthMin", logmin);
        gatherShader->setUniform("logDepthMax", logmax);
        //resolveShader->setUniform("logDepthMin", logmin);
        //resolveShader->setUniform("logDepthMax", logmax);
    }
    if (bucketMode == 4 && minDepthPassShader->hasUniform("logDepthMin")) {
        minDepthPassShader->setUniform("logDepthMin", logmin);
        minDepthPassShader->setUniform("logDepthMax", logmax);
    }
}

sgl::ShaderProgramPtr OIT_MLABBucket::getGatherShader()
{
    if (bucketMode != 4 || pass == 2) {
        return gatherShader;
    } else {
        return minDepthPassShader;
    }
}

void OIT_MLABBucket::setGatherShaderList(const std::list<std::string> &shaderIDs)
{
    gatherShaderIDs = shaderIDs;
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MLABBucketGather.glsl\"");
    gatherShader = ShaderManager->getShaderProgram(gatherShaderIDs);
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MinDepthPass.glsl\"");
    minDepthPassShader = ShaderManager->getShaderProgram(gatherShaderIDs);
}

void OIT_MLABBucket::reloadShaders()
{
    ShaderManager->invalidateShaderCache();

    ShaderManager->removePreprocessorDefine("MLAB_DEPTH_OPACITY_BUCKETS");
    ShaderManager->removePreprocessorDefine("MLAB_TRANSMITTANCE_BUCKETS");
    ShaderManager->removePreprocessorDefine("MLAB_MIN_DEPTH_BUCKETS");
    ShaderManager->removePreprocessorDefine("MLAB_DEPTH_BUCKETS");
    ShaderManager->removePreprocessorDefine("MLAB_OPACITY_BUCKETS");

    if (bucketMode == 0) {
        ShaderManager->addPreprocessorDefine("MLAB_DEPTH_BUCKETS", "");
    } else if (bucketMode == 1) {
        ShaderManager->addPreprocessorDefine("MLAB_OPACITY_BUCKETS", "");
    } else if (bucketMode == 2) {
        ShaderManager->addPreprocessorDefine("MLAB_DEPTH_OPACITY_BUCKETS", "");
    } else if (bucketMode == 3) {
        ShaderManager->addPreprocessorDefine("MLAB_TRANSMITTANCE_BUCKETS", "");
    } else {
        ShaderManager->addPreprocessorDefine("MLAB_MIN_DEPTH_BUCKETS", "");
    }

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MLABBucketGather.glsl\"");
    gatherShader = ShaderManager->getShaderProgram(gatherShaderIDs);
    resolveShader = ShaderManager->getShaderProgram({"MLABBucketResolve.Vertex", "MLABBucketResolve.Fragment"});
    clearShader = ShaderManager->getShaderProgram({"MLABBucketClear.Vertex", "MLABBucketClear.Fragment"});
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"MinDepthPass.glsl\"");
    minDepthPassShader = ShaderManager->getShaderProgram(gatherShaderIDs);
    //needsNewShaders = true;

    if (blitRenderData) {
        blitRenderData = blitRenderData->copy(resolveShader);
    }
    if (clearRenderData) {
        clearRenderData = clearRenderData->copy(clearShader);
    }
}


void OIT_MLABBucket::setNewState(const InternalState &newState)
{
    numBuckets = newState.oitAlgorithmSettings.getIntValue("numBuckets");
    nodesPerBucket = newState.oitAlgorithmSettings.getIntValue("nodesPerBucket");
    bucketMode = newState.oitAlgorithmSettings.getIntValue("bucketMode");
    lowerBackBufferOpacity = newState.oitAlgorithmSettings.getFloatValue("lowerOpacity");
    upperBackBufferOpacity = newState.oitAlgorithmSettings.getFloatValue("upperOpacity");
    useStencilBuffer = newState.useStencilBuffer;
    updateLayerMode();
}



void OIT_MLABBucket::setUniformData()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    getScreenSizeWithTiling(width, height);

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
    if (bucketMode == 4) {
        gatherShader->setShaderStorageBuffer(1, "MinDepthBuffer", minDepthBuffer);
    }

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    if (bucketMode == 4) {
        minDepthPassShader->setUniform("viewportW", width);
        minDepthPassShader->setShaderStorageBuffer(1, "MinDepthBuffer", minDepthBuffer);

        // depth bucket mode (num buckets = 2)
        minDepthPassShader->setUniform("lowerBackBufferOpacity", lowerBackBufferOpacity);
        minDepthPassShader->setUniform("upperBackBufferOpacity", upperBackBufferOpacity);
    }

    //mboitPass1Shader->setUniformImageTexture(0, b0, textureSettingsB0.internalFormat, GL_READ_WRITE, 0, true, 0);

    if (bucketMode == 2) {
        gatherShader->setUniformImageTexture(0, boundingBoxesTexture, boundingBoxesTextureSettings.internalFormat,
                                             GL_READ_WRITE, 0, true, 0);
        resolveShader->setUniformImageTexture(0, boundingBoxesTexture, boundingBoxesTextureSettings.internalFormat,
                                              GL_READ_WRITE, 0, true, 0);
        clearShader->setUniformImageTexture(0, boundingBoxesTexture, boundingBoxesTextureSettings.internalFormat,
                                            GL_READ_WRITE, 0, true, 0);
    }
    if (bucketMode == 3){
        gatherShader->setUniformImageTexture(0, transmittanceTexture, transmittanceTextureSettings.internalFormat,
                                             GL_READ_WRITE, 0, true, 0);
        resolveShader->setUniformImageTexture(0, transmittanceTexture, transmittanceTextureSettings.internalFormat,
                                              GL_READ_WRITE, 0, true, 0);
        clearShader->setUniformImageTexture(0, transmittanceTexture, transmittanceTextureSettings.internalFormat,
                                            GL_READ_WRITE, 0, true, 0);
    }

    if (bucketMode == 2 || bucketMode == 3) {
        gatherShader->setUniformImageTexture(1, numUsedBucketsTexture, numUsedBucketsTextureSettings.internalFormat,
                                             GL_READ_WRITE, 0, true, 0);
        resolveShader->setUniformImageTexture(1, numUsedBucketsTexture, numUsedBucketsTextureSettings.internalFormat,
                                              GL_READ_WRITE, 0, true, 0);
        clearShader->setUniformImageTexture(1, numUsedBucketsTexture, numUsedBucketsTextureSettings.internalFormat,
                                            GL_READ_WRITE, 0, true, 0);
    }
}

void OIT_MLABBucket::gatherBegin()
{
    setUniformData();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    if (clearBitSet) {
        Renderer->render(clearRenderData);
        clearBitSet = false;
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glEnable(GL_DEPTH_TEST);

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

}

void OIT_MLABBucket::renderScene()
{
    pass = 1;
    renderSceneFunction();

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}


void OIT_MLABBucket::gatherEnd()
{
    if (bucketMode == 4) {
        //if (useStencilBuffer) {
        //    glStencilMask(0x00);
        //}

        pass = 2;
        renderSceneFunction();

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void OIT_MLABBucket::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    Renderer->render(blitRenderData);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}
