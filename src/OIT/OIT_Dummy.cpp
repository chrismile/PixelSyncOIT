//
// Created by christoph on 08.09.18.
//

#include <GL/glew.h>
#include <Graphics/Shader/ShaderManager.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "OIT_Dummy.hpp"
#include "BufferSizeWatch.hpp"

static bool useDepthBuffer = true;

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
    gatherShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
    glDisable(GL_STENCIL_TEST);
    setCurrentAlgorithmBufferSizeBytes(0);
}

void OIT_Dummy::gatherBegin()
{
    if (useDepthBuffer) {
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void OIT_Dummy::gatherEnd()
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
}

void OIT_Dummy::renderGUI()
{
    ImGui::Separator();
    if (ImGui::Checkbox("Depth Test", &useDepthBuffer)) {
        reRender = true;
    }
}
