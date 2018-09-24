//
// Created by christoph on 24.09.18.
//

#ifndef PIXELSYNCOIT_SSAO_HPP
#define PIXELSYNCOIT_SSAO_HPP

#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Texture/TextureManager.hpp>

class SSAOHelper
{
public:
    SSAOHelper() { init(); }
    void resolutionChanged();

    inline const std::string &getGatherShaderName();

    // Call "preRender" to generate the SSAO texture, then fetch it using "getSSAOTexture"
    void preRender(std::function<void()> sceneRenderFunction);
    inline sgl::TexturePtr getSSAOTexture() { return ssaoTexture; }
    inline sgl::ShaderProgramPtr getGeometryPassShader() { return geometryPassShader; }
    inline bool isPreRenderPass() { return preRenderPass; }

private:
    void init();
    void loadShaders();

    // Shaders
    sgl::ShaderProgramPtr geometryPassShader;
    sgl::ShaderProgramPtr generateSSAOTextureShader;

    // Data for generating the G-buffer
    sgl::FramebufferObjectPtr gBufferFBO;
    sgl::TexturePtr positionTexture;
    sgl::TexturePtr normalTexture;
    sgl::RenderbufferObjectPtr depthStencilRBO;

    // Data for rendering the SSAO texture (also using textures above)
    sgl::FramebufferObjectPtr generateSSAOTextureFBO;
    sgl::TexturePtr ssaoTexture;
    sgl::ShaderAttributesPtr generateSSAORenderData;

    // Internal data for the algorithm
    std::vector<glm::vec3> sampleKernel;
    sgl::TexturePtr rotationVectorTexture;

    // State variables
    bool preRenderPass = false;
};


#endif //PIXELSYNCOIT_SSAO_HPP
