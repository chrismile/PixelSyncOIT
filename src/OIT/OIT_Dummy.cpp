//
// Created by christoph on 08.09.18.
//

#include <GL/glew.h>

#include "OIT_Dummy.hpp"

OIT_Dummy::OIT_Dummy()
{
    create();
}

void OIT_Dummy::create()
{
    renderShader = sgl::ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    glDisable(GL_STENCIL_TEST);
}
