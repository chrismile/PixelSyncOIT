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

void OIT_Dummy::create()
{
    sgl::ShaderManager->addPreprocessorDefine("DIRECT_BLIT_GATHER", "");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "Blit.glsl");
    gatherShader = sgl::ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    sgl::ShaderManager->removePreprocessorDefine("DIRECT_BLIT_GATHER"); // Remove for case that renderer is switched
    glDisable(GL_STENCIL_TEST);
}
