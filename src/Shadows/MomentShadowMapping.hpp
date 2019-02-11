//
// Created by christoph on 10.02.19.
//

#ifndef PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
#define PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP

#include "ShadowTechnique.hpp"

class MomentShadowMapping : public ShadowTechnique
{
public:
    MomentShadowMapping();
    virtual ShadowMappingTechniqueName getShadowMappingTechnique() { return MOMENT_SHADOW_MAPPING; }
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction);
    virtual void loadShaders();
    virtual void setUniformValuesCreateShadowMap();
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader);
    virtual void setShaderDefines();
    virtual void resolutionChanged();
    virtual void renderGUI();

private:
    // Additional shader for clearing moment textures
    sgl::ShaderProgramPtr clearShadowMapShader;

};


#endif //PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
