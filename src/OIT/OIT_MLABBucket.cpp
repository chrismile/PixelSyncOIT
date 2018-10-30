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
#include <ImGui/ImGuiWrapper.hpp>

#include "TilingMode.hpp"
#include "OIT_MLABBucket.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
static bool useStencilBuffer = true;

// Maximum number of buckets
static int numBuckets = 4;

// Maximum number of nodes per bucket
static int nodesPerBucket = 4;


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

    size_t bufferSizeBytes = (sizeof(uint32_t) + sizeof(float)) * numBuckets * nodesPerBucket * width * height;
    fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

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

    if (ImGui::SliderInt("Nodes per Bucket", &nodesPerBucket, 1, 8)) {
        updateLayerMode();
        reRender = true;
    }

    if (selectTilingModeUI()) {
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
    float minViewZ = screenSpaceBB.getMaximum().z;
    float maxViewZ = screenSpaceBB.getMinimum().z;
    minViewZ = std::max(-minViewZ, camera->getNearClipDistance());
    maxViewZ = std::min(-maxViewZ, camera->getFarClipDistance());
    minViewZ = std::min(minViewZ, camera->getFarClipDistance());
    maxViewZ = std::max(maxViewZ, camera->getNearClipDistance());
    float logmin = log(minViewZ);
    float logmax = log(maxViewZ);
    gatherShader->setUniform("logDepthMin", logmin);
    gatherShader->setUniform("logDepthMax", logmax);
    //resolveShader->setUniform("logDepthMin", logmin);
    //resolveShader->setUniform("logDepthMax", logmax);
}

void OIT_MLABBucket::reloadShaders()
{
    gatherShader = ShaderManager->getShaderProgram(gatherShaderIDs);
    resolveShader = ShaderManager->getShaderProgram({"MLABBucketResolve.Vertex", "MLABBucketResolve.Fragment"});
    clearShader = ShaderManager->getShaderProgram({"MLABBucketClear.Vertex", "MLABBucketClear.Fragment"});
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
    useStencilBuffer = newState.useStencilBuffer;
    updateLayerMode();
}



void OIT_MLABBucket::setUniformData()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
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

void OIT_MLABBucket::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_MLABBucket::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
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

    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}
