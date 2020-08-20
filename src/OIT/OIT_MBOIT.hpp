//
// Created by christoph on 09.09.18.
//

#ifndef PIXELSYNCOIT_OIT_MBOIT_HPP
#define PIXELSYNCOIT_OIT_MBOIT_HPP

#include <Math/Geometry/AABB3.hpp>
#include <Math/Geometry/Sphere.hpp>
#include <Graphics/Texture/TextureManager.hpp>

#include "OIT_Renderer.hpp"
#include "OIT_MBOIT_Utils.hpp"

/**
 * C. Munstermann, S. Krumpen, R. Klein, and C. Peters, "Moment-based order-independent transparency," Proceedings of
 * the ACM on Computer Graphics and Interactive Techniques, vol. 1, no. 1, pp. 7:1–7:20, May 2018.
 *
 * This implementation uses pixel sync (i.e., ARB_fragment_shader_interlock).
 */
class OIT_MBOIT : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader()
    {
        if (pass == 1) {
            return mboitPass1Shader;
        } else {
            return mboitPass2Shader;
        }
    }
    virtual void setGatherShaderList(const std::list<std::string> &shaderIDs);

    OIT_MBOIT();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
            sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene();
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

    // For determining minimum and maximum (screen-space) depth
    void setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB, sgl::CameraPtr &camera);

    // OIT Renderers can render their own ImGui elements
    virtual void renderGUI();
    virtual bool needsNewShader() { bool tmp = useNewShader; useNewShader = false; return tmp; }

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

private:
    void clear();
    void setUniformData();
    void reloadShaders();
    void updateMomentMode();

    sgl::ShaderProgramPtr mboitPass1Shader;
    sgl::ShaderProgramPtr mboitPass2Shader;
    sgl::ShaderProgramPtr blendShader;
    sgl::GeometryBufferPtr fragmentNodes;

    MomentOITUniformData momentUniformData;
    sgl::GeometryBufferPtr momentOITUniformBuffer;

    // Indicates whether a new OIT mode was selected
    bool useNewShader = false;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    //sgl::ShaderAttributesPtr clearRenderData;

    // Internal state
    int pass = 1;

    sgl::TexturePtr b0;
    sgl::TexturePtr b;
    sgl::TexturePtr bExtra;
    sgl::TextureSettings textureSettingsB0;
    sgl::TextureSettings textureSettingsB;
    sgl::TextureSettings textureSettingsBExtra;

    // Framebuffer used for rendering the scene
    sgl::FramebufferObjectPtr sceneFramebuffer;
    sgl::RenderbufferObjectPtr sceneDepthRBO;

    sgl::FramebufferObjectPtr blendFBO;
    sgl::TexturePtr blendRenderTexture;
};


#endif //PIXELSYNCOIT_OIT_MBOIT_HPP
