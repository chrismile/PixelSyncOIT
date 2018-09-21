//
// Created by christoph on 09.09.18.
//

#ifndef PIXELSYNCOIT_OIT_MBOIT_HPP
#define PIXELSYNCOIT_OIT_MBOIT_HPP

#include <Math/Geometry/AABB3.hpp>
#include <Math/Geometry/Sphere.hpp>
#include <Graphics/Texture/TextureManager.hpp>

#include "OIT_Renderer.hpp"

#define MBOIT_NUM_FRAGMENTS 8

// A fragment node stores rendering information about a list of fragments
struct MBOITFragmentNode_compressed
{
    // Linear depth, i.e. distance to viewer
    float depth[MBOIT_NUM_FRAGMENTS];
    // RGB color (3 bytes), translucency (1 byte)
    uint premulColor[MBOIT_NUM_FRAGMENTS];
};

/**
 * An order independent transparency renderer using pixel sync.
 *
 * (To be precise, it doesn't use the Intel-specific Pixel Sync extension
 * INTEL_fragment_shader_ordering, but the vendor-independent ARB_fragment_shader_interlock).
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
    virtual void setGatherShader(const std::string &name);

    OIT_MBOIT();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene();
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

    // For determining minimum and maximum (screen-space) depth
    void setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB);

private:
    void clear();
    void setUniformData();

    sgl::ShaderProgramPtr mboitPass1Shader;
    sgl::ShaderProgramPtr mboitPass2Shader;
    sgl::ShaderProgramPtr blendShader;
    //sgl::ShaderProgramPtr clearShader;
    sgl::GeometryBufferPtr fragmentNodes;
    sgl::GeometryBufferPtr numFragmentsBuffer;


    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    //sgl::ShaderAttributesPtr clearRenderData;

    // Internal state
    int pass = 1;
    bool clearBitSet;

    sgl::TexturePtr b0;
    sgl::TexturePtr b;
    sgl::TextureSettings textureSettingsB0;
    sgl::TextureSettings textureSettingsB;

    // Framebuffer used for rendering the scene
    sgl::FramebufferObjectPtr sceneFramebuffer;

    sgl::FramebufferObjectPtr blendFBO;
    sgl::TexturePtr blendRenderTexture;
};


#endif //PIXELSYNCOIT_OIT_MBOIT_HPP
