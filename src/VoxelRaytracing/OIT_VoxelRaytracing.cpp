//
// Created by christoph on 02.10.18.
//

#include <cmath>
#include <boost/algorithm/string/predicate.hpp>

#include <GL/glew.h>

#include <Utils/File/FileUtils.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "../Performance/InternalState.hpp"
#include "VoxelCurveDiscretizer.hpp"
#include "OIT_VoxelRaytracing.hpp"

//#define VOXEL_RAYTRACING_COMPUTE_SHADER

static bool useNeighborSearch = true;

OIT_VoxelRaytracing::OIT_VoxelRaytracing(sgl::CameraPtr &camera, const sgl::Color &clearColor) : camera(camera), clearColor(clearColor)
{
    create();
}

void OIT_VoxelRaytracing::setLineRadius(float lineRadius)
{
    this->lineRadius = lineRadius;
}

void OIT_VoxelRaytracing::setClearColor(const sgl::Color &clearColor)
{
    this->clearColor = clearColor;
}

void OIT_VoxelRaytracing::setLightDirection(const glm::vec3 &lightDirection)
{
    this->lightDirection = lightDirection;
}

void OIT_VoxelRaytracing::setTransferFunctionTexture(const sgl::TexturePtr &texture)
{
    this->tfTexture = texture;
}

void OIT_VoxelRaytracing::create()
{
    if (useNeighborSearch) {
        sgl::ShaderManager->removePreprocessorDefine("VOXEL_RAY_CASTING_FAST");
    } else {
        sgl::ShaderManager->addPreprocessorDefine("VOXEL_RAY_CASTING_FAST", "");
    }
    //renderShader = sgl::ShaderManager->getShaderProgram({ "VoxelRaytracingMain.Compute" });
}

void OIT_VoxelRaytracing::renderGUI()
{
    ImGui::Separator();

    if (ImGui::Checkbox("Voxel Neighbor Search (Slow)", &useNeighborSearch)) {
        if (useNeighborSearch) {
            sgl::ShaderManager->removePreprocessorDefine("VOXEL_RAY_CASTING_FAST");
        } else {
            sgl::ShaderManager->addPreprocessorDefine("VOXEL_RAY_CASTING_FAST", "");
        }
        reloadShader();
        reRender = true;
    }
}

void OIT_VoxelRaytracing::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
        sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    sgl::TextureSettings settings;
    //renderImage = sgl::TextureManager->createEmptyTexture(width, height, settings);
    renderImage = sceneTexture;
}

void OIT_VoxelRaytracing::loadModel(int modelIndex, std::vector<float> &attributes, float &maxVorticity)
{
    fromFile(MODEL_FILENAMES[modelIndex], attributes, maxVorticity);
}

std::string ivec3ToString(const glm::ivec3 &v) {
    return std::string() + "ivec3(" + sgl::toString(v.x) + ", " + sgl::toString(v.y) + ", " + sgl::toString(v.z) + ")";
}

void OIT_VoxelRaytracing::setNewState(const InternalState &newState)
{
    //int gridResolution = newState.oitAlgorithmSettings.getIntValue("gridResolution");
    //int quantizationResolution = newState.oitAlgorithmSettings.getIntValue("quantizationResolution");

    newState.oitAlgorithmSettings.getValueOpt("useNeighborSearch", useNeighborSearch);
    if (useNeighborSearch) {
        sgl::ShaderManager->removePreprocessorDefine("VOXEL_RAY_CASTING_FAST");
    } else {
        sgl::ShaderManager->addPreprocessorDefine("VOXEL_RAY_CASTING_FAST", "");
    }
    reloadShader();
}

void OIT_VoxelRaytracing::fromFile(const std::string &filename, std::vector<float> &attributes, float &maxVorticity)
{
    // Check if voxel grid is already created
    // Pure filename without extension (to create compressed .voxel filename)
    std::string modelFilenamePure = sgl::FileUtils::get()->removeExtension(filename);

    std::string modelFilenameVoxelGrid = modelFilenamePure + ".voxel";

    // Can be either hair dataset or trajectory dataset
    isHairDataset = boost::starts_with(modelFilenamePure, "Data/Hair");

    VoxelGridDataCompressed compressedData;
    if (!sgl::FileUtils::get()->exists(modelFilenameVoxelGrid)) {
        VoxelCurveDiscretizer discretizer(glm::ivec3(128), glm::ivec3(64));
        if (isHairDataset) {
            std::string modelFilenameHair = modelFilenamePure + ".hair";
            discretizer.createFromHairDataset(modelFilenameHair, lineRadius, hairStrandColor);
        } else {
            std::string modelFilenameObj = modelFilenamePure + ".obj";
            discretizer.createFromTrajectoryDataset(modelFilenameObj, attributes, maxVorticity);
        }
        compressedData = discretizer.compressData();
        saveToFile(modelFilenameVoxelGrid, compressedData);
    } else {
        loadFromFile(modelFilenameVoxelGrid, compressedData);
        if (isHairDataset) {
            lineRadius = compressedData.hairThickness;
            hairStrandColor = compressedData.hairStrandColor;
        } else {
            attributes = compressedData.attributes;
            maxVorticity = compressedData.maxVorticity;
        }
    }
    compressedToGPUData(compressedData, data);

    // Create shader program
    sgl::ShaderManager->invalidateShaderCache();
    sgl::ShaderManager->addPreprocessorDefine("gridResolution", ivec3ToString(data.gridResolution));
    sgl::ShaderManager->addPreprocessorDefine("GRID_RESOLUTION_LOG2",
            sgl::toString(sgl::intlog2(data.gridResolution.x)));
    sgl::ShaderManager->addPreprocessorDefine("GRID_RESOLUTION", data.gridResolution.x);
    sgl::ShaderManager->addPreprocessorDefine("quantizationResolution", ivec3ToString(data.quantizationResolution));
    sgl::ShaderManager->addPreprocessorDefine("QUANTIZATION_RESOLUTION", sgl::toString(data.quantizationResolution.x));
    sgl::ShaderManager->addPreprocessorDefine("QUANTIZATION_RESOLUTION_LOG2",
            sgl::toString(sgl::intlog2(data.quantizationResolution.x)));
    if (isHairDataset) {
        sgl::ShaderManager->addPreprocessorDefine("HAIR_RENDERING", "");
    } else {
        sgl::ShaderManager->removePreprocessorDefine("HAIR_RENDERING");
    }
#ifdef VOXEL_RAYTRACING_COMPUTE_SHADER
    renderShader = sgl::ShaderManager->getShaderProgram({ "VoxelRaytracingMain.Compute" });
#else
    renderShader = sgl::ShaderManager->getShaderProgram({ "VoxelRaytracingMainFrag.Vertex",
                                                          "VoxelRaytracingMainFrag.Fragment" });

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = sgl::ShaderManager->createShaderAttributes(renderShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    sgl::GeometryBufferPtr geomBuffer = sgl::Renderer->createGeometryBuffer(
            sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", sgl::ATTRIB_FLOAT, 3);
#endif
}

void OIT_VoxelRaytracing::reloadShader()
{
    sgl::ShaderManager->invalidateShaderCache();
    renderShader = sgl::ShaderManager->getShaderProgram({ "VoxelRaytracingMainFrag.Vertex",
                                                          "VoxelRaytracingMainFrag.Fragment" });
    blitRenderData = blitRenderData->copy(renderShader);
}

void OIT_VoxelRaytracing::setUniformData()
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    // Set camera data
    renderShader->setUniform("fov", camera->getFOVy());
    renderShader->setUniform("aspectRatio", camera->getAspectRatio());
    glm::mat4 inverseViewMatrix = glm::inverse(camera->getViewMatrix());
    renderShader->setUniform("inverseViewMatrix", inverseViewMatrix);

    float voxelSpaceRatio = lineRadius * glm::length(data.worldToVoxelGridMatrix[0]);
    renderShader->setUniform("lineRadius", voxelSpaceRatio);
    renderShader->setUniform("clearColor", clearColor);
    if (renderShader->hasUniform("lightDirection")) {
        renderShader->setUniform("lightDirection", lightDirection);
    }

    if (isHairDataset) {
        renderShader->setUniform("hairStrandColor", hairStrandColor);
    }

    //renderShader->setShaderStorageBuffer(0, "LineSegmentBuffer", NULL);
    renderShader->setUniformImageTexture(0, renderImage, GL_RGBA8, GL_WRITE_ONLY);
    renderShader->setUniform("viewportSize", glm::ivec2(width, height));

    sgl::ShaderManager->bindShaderStorageBuffer(0, data.voxelLineListOffsets);
    sgl::ShaderManager->bindShaderStorageBuffer(1, data.numLinesInVoxel);
    sgl::ShaderManager->bindShaderStorageBuffer(2, data.lineSegments);
    if (renderShader->hasUniform("densityTexture")) {
        renderShader->setUniform("densityTexture", data.densityTexture, 0);
    }
    if (renderShader->hasUniform("octreeTexture")) {
        renderShader->setUniform("octreeTexture", data.octreeTexture, 1);
    }
    if (renderShader->hasUniform("transferFunctionTexture")) {
        renderShader->setUniform("transferFunctionTexture", this->tfTexture, 2);
    }

    renderShader->setUniform("worldSpaceToVoxelSpace", data.worldToVoxelGridMatrix);
    //renderShader->setUniform("voxelSpaceToWorldSpace", glm::inverse(data.worldToVoxelGridMatrix));
}

inline int nextPowerOfTwo(int x) {
    int powerOfTwo = 1;
    while (powerOfTwo < x) {
        powerOfTwo *= 2;
    }
    return powerOfTwo;
}

// Utility functions for dispatching compute shaders

inline int iceil(int x, int y) {
    return (x - 1) / y + 1;
}

glm::ivec2 rangePadding2D(int w, int h, glm::ivec2 localSize) {
    return glm::ivec2(iceil(w, localSize[0])*localSize[0], iceil(h, localSize[1])*localSize[1]);
}

glm::ivec2 rangePadding1D(int w, int localSize) {
    return glm::ivec2(iceil(w, localSize)*localSize);
}

glm::ivec2 getNumWorkGroups(const glm::ivec2 &globalWorkSize, const glm::ivec2 &localWorkSize) {
    // Ceil
    return glm::ivec2(1) + ((globalWorkSize - glm::ivec2(1)) / localWorkSize);
}

void OIT_VoxelRaytracing::renderToScreen()
{
    setUniformData();

    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

#ifdef VOXEL_RAYTRACING_COMPUTE_SHADER
    //sgl::ShaderManager->getMaxComputeWorkGroupCount();
    glm::ivec2 numWorkGroups = getNumWorkGroups(glm::ivec2(width, height), glm::ivec2(8, 4)); // last vector: local work group size
    renderShader->dispatchCompute(numWorkGroups.x, numWorkGroups.y);
#else
    sgl::Renderer->render(blitRenderData);
#endif
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
