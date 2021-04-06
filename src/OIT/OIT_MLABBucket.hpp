//
// Created by christoph on 30.10.18.
//

#ifndef PIXELSYNCOIT_OIT_MLABBUCKET_HPP
#define PIXELSYNCOIT_OIT_MLABBUCKET_HPP

#include <Math/Geometry/AABB3.hpp>

#include "OIT_Renderer.hpp"

/**
 * An order independent transparency renderer using pixel sync (i.e., ARB_fragment_shader_interlock).
 *
 * This is an improvement of the original MLAB technique presented in:
 * M. Kern, C. Neuhauser, T. Maack, M. Han, W. Usher and R. Westermann, "A Comparison of Rendering Techniques for 3D
 * Line Sets with Transparency," in IEEE Transactions on Visualization and Computer Graphics, doi:
 * 10.1109/TVCG.2020.2975795.
 *
 * Original MLAB:
 * M. Salvi and K. Vaidyanathan, “Multi-layer alpha blending,” in Proceedings of the 18th Meeting of the ACM SIGGRAPH
 * Symposium on Interactive 3D Graphics and Games, ser. I3D ’14. New York, NY, USA: ACM, 2014, pp. 151–158
 */
class OIT_MLABBucket : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader();
    virtual void setGatherShaderList(const std::vector<std::string> &shaderIDs);

    OIT_MLABBucket();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                   sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene();
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

    void renderGUI();
    void updateLayerMode();
    void reloadShaders();

    // For determining minimum and maximum (screen-space) depth
    void setScreenSpaceBoundingBox(const sgl::AABB3 &screenSpaceBB, sgl::CameraPtr &camera);

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

private:
    void clear();
    void setUniformData();

    sgl::GeometryBufferPtr fragmentNodes;
    sgl::GeometryBufferPtr minDepthBuffer;

    sgl::TexturePtr boundingBoxesTexture;
    sgl::TexturePtr numUsedBucketsTexture;
    sgl::TexturePtr transmittanceTexture;
    sgl::TextureSettings boundingBoxesTextureSettings;
    sgl::TextureSettings numUsedBucketsTextureSettings;
    sgl::TextureSettings transmittanceTextureSettings;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    sgl::ShaderAttributesPtr clearRenderData;
    sgl::ShaderProgramPtr minDepthPassShader;

    // Internal state
    int pass = 2;

    sgl::FramebufferObjectPtr sceneFramebuffer;
    sgl::TexturePtr sceneTexture;
    sgl::RenderbufferObjectPtr sceneDepthRBO;

    bool clearBitSet;
};

#endif //PIXELSYNCOIT_OIT_MLABBUCKET_HPP
