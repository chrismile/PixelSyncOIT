//
// Created by christoph on 22.09.18.
//

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/OpenGL/Texture.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_TestLoadStore.hpp"

using namespace sgl;

OIT_TestLoadStore::OIT_TestLoadStore()
{
    create();
}

OIT_TestLoadStore::~OIT_TestLoadStore()
{
    sgl::ShaderManager->removePreprocessorDefine("DIRECT_BLIT_GATHER"); // Remove for case that renderer is switched
}


void OIT_TestLoadStore::create()
{
    // Load dummy shader
    sgl::ShaderManager->addPreprocessorDefine("DIRECT_BLIT_GATHER", "");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "Empty.glsl");
    gatherShader = sgl::ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    glDisable(GL_STENCIL_TEST);


    writeShader = ShaderManager->getShaderProgram({"LoadStoreWrite.Vertex","LoadStoreWrite.Fragment"});
    readShader = ShaderManager->getShaderProgram({"LoadStoreRead.Vertex","LoadStoreRead.Fragment"});

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    writeRenderData = ShaderManager->createShaderAttributes(writeShader);
    readRenderData = ShaderManager->createShaderAttributes(readShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
                                                                  (void*)&fullscreenQuad.front());
    writeRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
    readRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}


void OIT_TestLoadStore::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    void *emptyData = calloc(width * height, sizeof(float)*2);

    TextureSettings textureSettings = TextureSettings();
    textureSettings.pixelType = GL_FLOAT;
    textureSettings.pixelFormat = GL_RED;
    textureSettings.internalFormat = GL_R32F;
    textureSettings.textureWrapR = GL_REPEAT;
    moments = TextureManager->createTextureArray(emptyData, width, height, 2, textureSettings);

    free(emptyData);
}

void OIT_TestLoadStore::gatherEnd()
{
    //writeShader->setUniformImageTexture(0, moments, textureSettings.internalFormat, GL_READ_WRITE, 0, true, 1);
    //readShader->setUniformImageTexture(0, moments, textureSettings.internalFormat, GL_READ_WRITE, 0, true, 1);
    GLuint texture = ((TextureGL*)moments.get())->getTexture();
    glBindImageTexture(0, texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    Renderer->render(writeRenderData);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    Renderer->render(readRenderData);


    glDepthMask(GL_TRUE);
}