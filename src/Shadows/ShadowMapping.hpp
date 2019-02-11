//
// Created by christoph on 10.02.19.
//

#ifndef PIXELSYNCOIT_SHADOWMAPPING_HPP
#define PIXELSYNCOIT_SHADOWMAPPING_HPP

#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include "ShadowTechnique.hpp"

class ShadowMapping : public ShadowTechnique
{
public:
    ShadowMapping();
    virtual ShadowMappingTechniqueName getShadowMappingTechnique() { return SHADOW_MAPPING; }
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction);
    virtual void loadShaders();
    virtual void setUniformValuesCreateShadowMap();
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader);
    virtual void setShaderDefines();
    virtual void resolutionChanged();
    virtual void renderGUI();

private:

    // For rendering to the shadow map
    sgl::FramebufferObjectPtr shadowMapFBO;
    sgl::TexturePtr shadowMap;
};


#endif //PIXELSYNCOIT_SHADOWMAPPING_HPP
