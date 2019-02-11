//
// Created by christoph on 10.02.19.
//

#ifndef PIXELSYNCOIT_SHADOWTECHNIQUE_HPP
#define PIXELSYNCOIT_SHADOWTECHNIQUE_HPP

#include <glm/glm.hpp>

enum ShadowMappingTechniqueName {
        NO_SHADOW_MAPPING, SHADOW_MAPPING, MOMENT_SHADOW_MAPPING
};
const char *const SHADOW_MAPPING_TECHNIQUE_DISPLAYNAMES[] = {
        "No Shadows", "Shadow Mapping", "Moment Shadow Mapping"
};


class ShadowTechnique
{
public:
    ShadowTechnique() : preRenderPass(false) {}
    virtual ShadowMappingTechniqueName getShadowMappingTechnique()=0;
    sgl::ShaderProgramPtr getShadowMapCreationShader() { return createShadowMapShader; }

    // Call "createShadowMapPass" to generate the shadow map
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction)=0;
    virtual void loadShaders()=0;
    virtual void setUniformValuesCreateShadowMap()=0;
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader)=0;
    virtual void setShaderDefines()=0;
    virtual void resolutionChanged()=0;
    virtual void renderGUI()=0;

    // Called by MainApp if the direction of the directional light changes
    void setLightDirection(const glm::vec3 &lightDirection, const glm::vec3 &sceneCenter);

    // Called by MainApp
    void newModelLoaded(const std::string &filename);

    // Returns whether the 1st pass for generating the shadow map is active
    inline bool isShadowMapCreatePass() { return preRenderPass; }

protected:
    // Geometry pass to generate the shadow map
    sgl::ShaderProgramPtr createShadowMapShader;

    // lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix
    glm::mat4 lightViewMatrix, lightProjectionMatrix, lightSpaceMatrix;
    glm::vec3 lightDirection;
    bool preRenderPass;
};

class NoShadowMapping : public ShadowTechnique
{
public:
    virtual ShadowMappingTechniqueName getShadowMappingTechnique() { return NO_SHADOW_MAPPING; }
    // Call "createShadowMapPass" to generate the shadow map
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction) {}
    virtual void loadShaders() {}
    virtual void setUniformValuesCreateShadowMap() {}
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader) {}
    virtual void setShaderDefines();
    virtual void resolutionChanged() {}
    virtual void renderGUI() {}
};

#endif //PIXELSYNCOIT_SHADOWTECHNIQUE_HPP
