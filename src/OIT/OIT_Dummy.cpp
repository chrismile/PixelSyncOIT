//
// Created by christoph on 08.09.18.
//

#include <GL/glew.h>
#include <Graphics/Shader/ShaderManager.hpp>

#include "OIT_Dummy.hpp"

OIT_Dummy::OIT_Dummy()
{
    create();
}

OIT_Dummy::~OIT_Dummy()
{
    sgl::ShaderManager->removePreprocessorDefine("DIRECT_BLIT_GATHER"); // Remove for case that renderer is switched
}

void OIT_Dummy::create()
{
    sgl::ShaderManager->addPreprocessorDefine("DIRECT_BLIT_GATHER", "");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "Empty.glsl");
    gatherShader = sgl::ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    glDisable(GL_STENCIL_TEST);
}
