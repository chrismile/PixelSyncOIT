//
// Created by christoph on 10.02.19.
//

#ifndef PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
#define PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP

#include <Math/Geometry/AABB3.hpp>
#include "ShadowTechnique.hpp"
#include "../OIT/OIT_MBOIT_Utils.hpp"

// For high-quality test video w/o public interface to shadow technique.
extern void setHighResMomentShadowMapping();

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
    virtual bool renderGUI();

    // Called by MainApp
    virtual void setGatherShaderList(const std::list<std::string> &shaderIDs);
    void setSceneBoundingBox(const sgl::AABB3 &sceneBB);
    // Called by MainApp if the direction of the directional light changes
    virtual void setLightDirection(const glm::vec3 &lightDirection, const sgl::AABB3 &sceneBoundingBox);

private:
    // Called when new moment mode was set
    void updateMomentMode();
    // Reloads b0, b, bExtra
    void createMomentTextures();
    // Called when some setting was changed and the shaders need to be reloaded
    void reloadShaders();
    // Called by "setSceneBoundingBox" and "setLightDirection"
    void updateDepthRange();

    // Gather shader name used for shading
    std::list<std::string> gatherShaderIDs = {"PseudoPhong.Vertex", "PseudoPhong.Fragment"};

    // Additional shader for clearing moment textures
    sgl::ShaderProgramPtr clearShadowMapShader;
    // Clear render data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr clearRenderData;

    // Global uniform data containing settings
    MomentOITUniformData momentUniformData;
    sgl::GeometryBufferPtr momentOITUniformBuffer;

    // Moment textures
    sgl::TexturePtr b0;
    sgl::TexturePtr b;
    sgl::TexturePtr bExtra;
    sgl::TextureSettings textureSettingsB0;
    sgl::TextureSettings textureSettingsB;
    sgl::TextureSettings textureSettingsBExtra;

    // For rendering to the moment shadow map
    sgl::FramebufferObjectPtr shadowMapFBO;

    // For computing logarithmic depth
    float logDepthMinShadow = 0.1f;
    float logDepthMaxShadow = 1.0f;
    sgl::AABB3 sceneBB;

    // TODO dummy
    sgl::TexturePtr shadowMap;
};


#endif //PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
