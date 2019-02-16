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

const float LIGHT_NEAR_CLIP_DISTANCE = 0.1f;
const float LIGHT_FAR_CLIP_DISTANCE = 4.0f;


class ShadowTechnique
{
public:
    ShadowTechnique() : preRenderPass(false), needsNewTransparencyShader(false) {}
    virtual ShadowMappingTechniqueName getShadowMappingTechnique()=0;
    sgl::ShaderProgramPtr getShadowMapCreationShader() { return createShadowMapShader; }
    bool getNeedsNewTransparencyShader() { return needsNewTransparencyShader; }

    // Call "createShadowMapPass" to generate the shadow map
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction)=0;
    virtual void loadShaders()=0;
    virtual void setUniformValuesCreateShadowMap()=0;
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader)=0;
    virtual void setShaderDefines()=0;
    virtual void resolutionChanged()=0;
    virtual bool renderGUI()=0;

    // Called by MainApp if the direction of the directional light changes
    virtual void setLightDirection(const glm::vec3 &lightDirection, const glm::vec3 &sceneCenter);

    // Called by MainApp
    void newModelLoaded(const std::string &filename);

    // Returns whether the 1st pass for generating the shadow map is active
    inline bool isShadowMapCreatePass() { return preRenderPass; }

    // Called by MainApp. For shadow techniques that need access to the gather shader.
    virtual void setGatherShaderList(const std::list<std::string> &shaderIDs) {}

protected:
    // Geometry pass to generate the shadow map
    sgl::ShaderProgramPtr createShadowMapShader;

    // lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix
    glm::mat4 lightViewMatrix, lightProjectionMatrix, lightSpaceMatrix;
    glm::vec3 lightDirection;
    bool preRenderPass;
    bool needsNewTransparencyShader;
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
    virtual bool renderGUI() { return false; }
};

#endif //PIXELSYNCOIT_SHADOWTECHNIQUE_HPP
