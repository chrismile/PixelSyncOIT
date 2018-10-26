//
// Created by christoph on 25.10.18.
//

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "TestPixelSyncPerformance.hpp"

TestPixelSyncPerformance::TestPixelSyncPerformance()
{
    create();
}

TestPixelSyncPerformance::~TestPixelSyncPerformance()
{
    timer.printTotalAvgTime();
}

void TestPixelSyncPerformance::create()
{
    if (!sgl::SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        sgl::Logfile::get()->writeError("Error in OIT_KBuffer::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    sgl::ShaderManager->invalidateShaderCache();
    sgl::ShaderManager->addPreprocessorDefine("TEST_PIXEL_SYNC", "");
    sgl::ShaderManager->addPreprocessorDefine("TEST_COMPUTE", "");
    resolveShader = sgl::ShaderManager->getShaderProgram({"TestPixelSyncOverhead.Vertex", "TestPixelSyncOverhead.Fragment"});

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0), glm::vec3(-1,1,0)};
    std::vector<uint32_t> indices;

    const size_t NUM_QUADS = 100;
    indices.reserve(NUM_QUADS*6);
    for (int i = 0; i < NUM_QUADS; i++) {
        indices.push_back(0);
        indices.push_back(1);
        indices.push_back(2);
        indices.push_back(1);
        indices.push_back(0);
        indices.push_back(3);
    }

    renderData = sgl::ShaderManager->createShaderAttributes(resolveShader);
    sgl::GeometryBufferPtr positionBuffer = sgl::Renderer->createGeometryBuffer(
            sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
    sgl::GeometryBufferPtr indexBuffer = sgl::Renderer->createGeometryBuffer(
            sizeof(glm::vec3)*indices.size(), (void*)&indices.front(), sgl::INDEX_BUFFER);
    renderData->addGeometryBuffer(positionBuffer, "vertexPosition", sgl::ATTRIB_FLOAT, 3);
    renderData->setIndexGeometryBuffer(indexBuffer, sgl::ATTRIB_UNSIGNED_INT);
}

void TestPixelSyncPerformance::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                    sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    dataBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    dataBuffer = sgl::Renderer->createGeometryBuffer(width * height * sizeof(float), NULL, sgl::SHADER_STORAGE_BUFFER);
}

void TestPixelSyncPerformance::setNewState(const InternalState &newState)
{
    sgl::ShaderManager->invalidateShaderCache();
    if (newState.oitAlgorithmSettings.getBoolValue("usePixelSync")) {
        sgl::ShaderManager->addPreprocessorDefine("TEST_PIXEL_SYNC", "");
    } else {
        sgl::ShaderManager->removePreprocessorDefine("TEST_PIXEL_SYNC");
    }
    if (newState.oitAlgorithmSettings.getValue("testType") == "compute") {
        sgl::ShaderManager->addPreprocessorDefine("TEST_COMPUTE", "");
        sgl::ShaderManager->removePreprocessorDefine("TEST_SUM");
    }
    if (newState.oitAlgorithmSettings.getValue("testType") == "sum") {
        sgl::ShaderManager->addPreprocessorDefine("TEST_SUM", "");
        sgl::ShaderManager->removePreprocessorDefine("TEST_COMPUTE");
    }
    resolveShader = sgl::ShaderManager->getShaderProgram({"TestPixelSyncOverhead.Vertex", "TestPixelSyncOverhead.Fragment"});
}

void TestPixelSyncPerformance::setUniformData()
{
    sgl:: Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(0, "DataBuffer", dataBuffer);
}

void TestPixelSyncPerformance::gatherBegin()
{
    setUniformData();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    sgl::Renderer->setProjectionMatrix(sgl::matrixIdentity());
    sgl::Renderer->setViewMatrix(sgl::matrixIdentity());
    sgl::Renderer->setModelMatrix(sgl::matrixIdentity());
    GLuint bufferID = static_cast<sgl::GeometryBufferGL*>(dataBuffer.get())->getBuffer();
    GLubyte val = 0;
    glClearNamedBufferData(bufferID, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void TestPixelSyncPerformance::renderToScreen()
{
    timer.start("Rendering time");
    sgl::Renderer->render(renderData);
    timer.end();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    sgl::Renderer->clearFramebuffer();
}

void TestPixelSyncPerformance::gatherEnd()
{
    glDepthMask(GL_TRUE);
}
