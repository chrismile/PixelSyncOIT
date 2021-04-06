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
    void loadModel(int modelIndex, TrajectoryType trajectoryType, std::vector<float> &attributes, float &maxVorticity);
    void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
            sgl::RenderbufferObjectPtr &sceneDepthRBO);
    void setLineRadius(float lineRadius);
    float getLineRadius();
    void setClearColor(const sgl::Color &clearColor);
    void setLightDirection(const glm::vec3 &lightDirection);
    void setTransferFunctionTexture(const sgl::TexturePtr &texture);

    virtual void gatherBegin() {}
    virtual void renderScene() {}
    virtual void gatherEnd() {}
    virtual void setGatherShaderList(const std::vector<std::string> &shaderIDs) {}

    // Contains all logic in raytracing renderer
    virtual void renderToScreen();

    // Render options in GUI menu controlling parameters of ray caster
    virtual void renderGUI();

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

    // Recompute density and AO factor if the transfer function changed.
    void onTransferFunctionMapRebuilt();

private:
    void fromFile(const std::string &filename, TrajectoryType trajectoryType, std::vector<float> &attributes,
            float &maxVorticity);
    void reloadShader();
    void setUniformData();

    sgl::ShaderProgramPtr renderShader;
    sgl::TexturePtr renderImage;

#ifndef VOXEL_RAYTRACING_COMPUTE_SHADER
    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
#endif

    // Data from MainApp
    sgl::CameraPtr camera;
    float lineRadius;
    sgl::Color clearColor;
    glm::vec3 lightDirection;
    sgl::TexturePtr tfTexture;
    // For hair datasets
    bool isHairDataset = false;
    glm::vec4 hairStrandColor;

    // Data compressed for GPU
    VoxelGridDataGPU data;
    VoxelGridDataCompressed compressedData;
    int maxNumLinesPerVoxel = 32;
};

#endif //PIXELSYNCOIT_OIT_VOXELRAYTRACING_HPP
