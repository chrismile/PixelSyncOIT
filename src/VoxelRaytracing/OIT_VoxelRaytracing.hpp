//
// Created by christoph on 02.10.18.
//

#ifndef PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP
#define PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP

#include "../OIT/OIT_Renderer.hpp"

class OIT_VoxelRaytracing : public OIT_Renderer
{
public:
    OIT_VoxelRaytracing();

    virtual sgl::ShaderProgramPtr getGatherShader() { return renderShader; }

    void create();
    void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO);

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

private:
    void setUniformData();

    sgl::ShaderProgramPtr renderShader;

    sgl::TexturePtr renderImage;
};

#endif //PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP
