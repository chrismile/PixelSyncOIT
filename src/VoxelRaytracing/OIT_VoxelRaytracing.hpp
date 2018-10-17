//
// Created by christoph on 02.10.18.
//

#ifndef PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP
#define PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP

#include "../OIT/OIT_Renderer.hpp"
#include "VoxelData.hpp"

class OIT_VoxelRaytracing : public OIT_Renderer
{
public:
    OIT_VoxelRaytracing(sgl::CameraPtr &camera, const sgl::Color &clearColor);

    virtual sgl::ShaderProgramPtr getGatherShader() { return renderShader; }

    void create();
    void loadModel(int modelIndex);
    void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
            sgl::RenderbufferObjectPtr &sceneDepthRBO);
    void setLineRadius(float lineRadius);
    void setClearColor(const sgl::Color &clearColor);
    void setLightDirection(const glm::vec3 &lightDirection);
    void setTransferFunctionTexture(const sgl::TexturePtr &texture);

    virtual void gatherBegin() {}
    virtual void renderScene() {}
    virtual void gatherEnd() {}
    virtual void setGatherShader(const std::string &name) {}

    // Contains all logic in raytracing renderer
    virtual void renderToScreen();

private:
    void fromFile(const std::string &filename);
    void setUniformData();

    sgl::ShaderProgramPtr renderShader;
    sgl::TexturePtr renderImage;

    // Data from MainApp
    sgl::CameraPtr camera;
    float lineRadius;
    sgl::Color clearColor;
    glm::vec3 lightDirection;
    sgl::TexturePtr tfTexture;

    // Data compressed for GPU
    VoxelGridDataGPU data;
};

#endif //PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP
