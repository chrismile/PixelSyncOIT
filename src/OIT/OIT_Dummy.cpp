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
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "GatherDummy.glsl");
    std::list<std::string> shaderIDs = {gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"};
    if (gatherShaderName.find("Vorticity") != std::string::npos) {
        shaderIDs.push_back(gatherShaderName + ".Geometry");
    }
    gatherShader = sgl::ShaderManager->getShaderProgram(shaderIDs);
    glDisable(GL_STENCIL_TEST);
}
