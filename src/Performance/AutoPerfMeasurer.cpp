//
// Created by Anonymous User on 27.09.18.
//

#include <GL/glew.h>

#include <Utils/File/Logfile.hpp>
#include <Utils/AppSettings.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Texture/Bitmap.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>

#include "ReferenceMetric.hpp"
#include "AutoPerfMeasurer.hpp"
#include "../OIT/BufferSizeWatch.hpp"

AutoPerfMeasurer::AutoPerfMeasurer(std::vector<InternalState> _states,
        const std::string &_csvFilename, const std::string &_depthComplexityFilename,
        std::function<void(const InternalState&)> _newStateCallback, bool measureTimeCoherence)
       : states(_states), currentStateIndex(0), newStateCallback(_newStateCallback), file(_csvFilename),
         depthComplexityFile(_depthComplexityFilename), errorMetricFile("error_metrics.csv"), perfFile("performance_list.csv"), timeCoherence(measureTimeCoherence)
{
    // Write header
    file.writeRow({"Name", "Average Time (ms)", "Image Filename", "Memory (GB)", "Buffer Size (GB)",
                   "SSIM", "RMSE", "PSNR", "Time Stamp (s), Frame Time (ns)"});
    depthComplexityFile.writeRow({"Current State", "Frame Number", "Min Depth Complexity", "Max Depth Complexity",
                                  "Avg Depth Complexity Used", "Avg Depth Complexity All", "Total Number of Fragments"});
    errorMetricFile.writeRow({"Name", "Error measures"});
    perfFile.writeRow({"Name", "Time per frame (ms)"});
    setPerformanceMeasurer(this);

    // Set initial state
    setNextState(true);
}

AutoPerfMeasurer::~AutoPerfMeasurer()
{
    writeCurrentModeData();

    file.close();
    depthComplexityFile.close();
    errorMetricFile.close();
    perfFile.close();
    //perfTimeProfileFile.close();
}


float nextModeCounter = 0.0f;
const float TIME_PER_MODE = 32.5f; // in seconds
bool AutoPerfMeasurer::update(float currentTime)
{
    nextModeCounter = currentTime;
    if (nextModeCounter >= TIME_PER_MODE) {
        nextModeCounter = 0.0f;
        if (currentStateIndex == states.size()-1) {
            return false; // Terminate program
        }
        setNextState();
    }
    return true;
}

void AutoPerfMeasurer::makeScreenshot()
{
    std::string filename = std::string() + "images/" + currentState.name + ".png";
    saveScreenshot(filename);
}

void AutoPerfMeasurer::makeScreenshot(uint32_t frameNum)
{
    std::string filename = std::string() + "images/" + currentState.name + "_frame_" + std::to_string(frameNum) + ".png";
    saveScreenshot(filename);
}

void AutoPerfMeasurer::writeCurrentModeData()
{
    // Write row with performance metrics of this mode
    timerGL.stopMeasuring();
    double timeMS = timerGL.getTimeMS(currentState.name);
    file.writeCell(currentState.name);
    perfFile.writeCell(currentState.name);
    file.writeCell(sgl::toString(timeMS));

    // Make screenshot of rendering result with current algorithm
    std::string filename = std::string() + "images/" + currentState.name + ".png";
    file.writeCell(filename);
    sgl::BitmapPtr image(new sgl::Bitmap());
    image->fromFile(filename.c_str());
    if (currentState.oitAlgorithm == RENDER_MODE_OIT_DEPTH_PEELING) {
        referenceImage = image;
        stateNameDepthPeeling = currentState.name;
    }

    // Write current memory consumption in gigabytes
    file.writeCell(sgl::toString(getUsedVideoMemorySizeGB()));
    file.writeCell(sgl::toString(currentAlgorithmsBufferSizeBytes*1e-9f));

    // Save normalized difference map
    if (referenceImage != nullptr) {
        std::string differenceMapFilename =
                std::string() + "images/" + currentState.name + " Difference" +
                ".png";
        sgl::BitmapPtr differenceMap = computeNormalizedDifferenceMap(
                referenceImage, image);
        differenceMap->savePNG(differenceMapFilename.c_str());

        // If a reference image is saved: Compute reference metrics (SSIM, RMSE, PSNR)
        if (referenceImage) {
            double ssimMetric = ssim(referenceImage, image);
            double rmseMetric = rmse(referenceImage, image);
            double psnrMetric = psnr(referenceImage, image);
            file.writeCell(sgl::toString(ssimMetric));
            file.writeCell(sgl::toString(rmseMetric));
            file.writeCell(sgl::toString(psnrMetric));
        }
    } else{
        file.writeCell(sgl::toString(0));
        file.writeCell(sgl::toString(0));
        file.writeCell(sgl::toString(0));
    }

    auto performanceProfile = timerGL.getCurrentFrameTimeList();
    for (auto &perfPair : performanceProfile) {
        float timeStamp = perfPair.first;
        uint64_t frameTimeNS = perfPair.second;
        float frameTimeMS = float(frameTimeNS) / float(1.0E6);
//        file.writeCell(sgl::toString(timeS    tamp) + ", " + sgl::toString(frameTimeNS));
        file.writeCell(sgl::toString(frameTimeMS));
        perfFile.writeCell(sgl::toString(frameTimeMS));
    }

    file.newRow();
    perfFile.newRow();

    /*perfTimeProfileFile.writeCell(currentState.name);
    auto performanceProfile = timerGL.getCurrentFrameTimeList();
    for (auto &perfPair : performanceProfile) {
        float timeStamp = perfPair.first;
        uint64_t frameTimeNS = perfPair.second;
        float frameTimeMS = frameTimeNS*1e-6;
        perfTimeProfileFile.writeCell(sgl::toString(timeStamp) + "," + sgl::toString(frameTimeMS));
    }
    perfTimeProfileFile.newRow();*/
}

void AutoPerfMeasurer::writeCurrentErrorMetricData()
{
    if (currentState.oitAlgorithm == RENDER_MODE_OIT_DEPTH_PEELING) {
        stateNameDepthPeeling = currentState.name;
        return;
    }

    std::vector<std::string> errorMetrics = { "RMSE", "PSNR", "SSIM" };
    const uint32_t MAX_FRAMES = 64;

    std::vector<double> rmseValues; rmseValues.reserve(MAX_FRAMES);
    std::vector<double> psnrValues; psnrValues.reserve(MAX_FRAMES);
    std::vector<double> ssimValues; ssimValues.reserve(MAX_FRAMES);

    std::vector<std::vector<double>> errorMeasures;

    for (uint32_t f = 1; f <= MAX_FRAMES; ++f)
    {
        // reference image
        std::string filenameGT = std::string() + "images/" + stateNameDepthPeeling + "_frame_" + std::to_string(f) + ".png";
        sgl::BitmapPtr refImage(new sgl::Bitmap());
        refImage->fromFile(filenameGT.c_str());

        // output image
        std::string filename = std::string() + "images/" + currentState.name + "_frame_" + std::to_string(f) + ".png";
        sgl::BitmapPtr outputImage(new sgl::Bitmap());
        outputImage->fromFile(filename.c_str());

        // Save normalized difference map
        std::string differenceMapFilename = std::string() + "images/" + currentState.name + " Difference_frame_" + std::to_string(f) + ".png";
        sgl::BitmapPtr differenceMap = computeNormalizedDifferenceMap(refImage, outputImage);
        differenceMap->savePNG(differenceMapFilename.c_str());

//        if (f == 1)
//        {
//            errorMetricFile.writeCell(filenameGT);
//            errorMetricFile.writeCell(filename);
//        }

        rmseValues.push_back(rmse(refImage, outputImage));
        psnrValues.push_back(psnr(refImage, outputImage));
        ssimValues.push_back(ssim(refImage, outputImage));
//        errorMetricFile.writeCell(sgl::toString(metric));
    }

    errorMeasures.push_back(rmseValues);
    errorMeasures.push_back(psnrValues);
    errorMeasures.push_back(ssimValues);


    for (uint32_t i = 0; i < errorMeasures.size(); ++i)
    {
        std::string metricName = errorMetrics[i];

        errorMetricFile.writeCell(currentState.name + " (" + metricName + ")");

        for (uint32_t f = 0; f < MAX_FRAMES; ++f)
        {
            errorMetricFile.writeCell(sgl::toString(errorMeasures[i][f]));
        }

        errorMetricFile.newRow();
    }
}

void AutoPerfMeasurer::setNextState(bool first)
{
    if (!first) {
        if (timeCoherence) {
            writeCurrentErrorMetricData();
        } else {
            writeCurrentModeData();
        }

        currentStateIndex++;
    }

    depthComplexityFrameNumber = 0;
    currentAlgorithmsBufferSizeBytes = 0;
    currentState = states.at(currentStateIndex);
    sgl::Logfile::get()->writeInfo(std::string() + "New state: " + currentState.name);
    newStateCallback(currentState);
}

void AutoPerfMeasurer::startMeasure(float timeStamp)
{
    timerGL.start(currentState.name, timeStamp);
}

void AutoPerfMeasurer::endMeasure()
{
    timerGL.end();
}


void AutoPerfMeasurer::resolutionChanged(sgl::FramebufferObjectPtr _sceneFramebuffer)
{
    sceneFramebuffer = _sceneFramebuffer;
}

void AutoPerfMeasurer::pushDepthComplexityFrame(uint64_t minComplexity, uint64_t maxComplexity,
        float avgUsed, float avgAll, uint64_t totalNumFragments)
{
    depthComplexityFile.writeCell(currentState.name);
    depthComplexityFile.writeCell(sgl::toString((int)depthComplexityFrameNumber));
    depthComplexityFile.writeCell(sgl::toString((int)minComplexity));
    depthComplexityFile.writeCell(sgl::toString((int)maxComplexity));
    depthComplexityFile.writeCell(sgl::toString(avgUsed));
    depthComplexityFile.writeCell(sgl::toString(avgAll));
    depthComplexityFile.writeCell(sgl::toString((int)totalNumFragments));
    depthComplexityFile.newRow();
    depthComplexityFrameNumber++;
}

void AutoPerfMeasurer::setCurrentAlgorithmBufferSizeBytes(size_t numBytes)
{
    currentAlgorithmsBufferSizeBytes = numBytes;
}

void AutoPerfMeasurer::saveScreenshot(const std::string &filename)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    //sgl::Renderer->bindFBO(sceneFramebuffer);
    //sgl::Renderer->unbindFBO();
    sgl::BitmapPtr bitmap(new sgl::Bitmap(width, height, 32));
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
    bitmap->savePNG(filename.c_str(), true);
    //sgl::Renderer->unbindFBO();
}

/*#ifndef GL_QUERY_RESOURCE_TYPE_VIDMEM_ALLOC_NV
#include <SDL2/SDL.h>
#endif*/

void AutoPerfMeasurer::setInitialFreeMemKilobytes(int initialFreeMemKilobytes)
{
    this->initialFreeMemKilobytes = initialFreeMemKilobytes;
}

float AutoPerfMeasurer::getUsedVideoMemorySizeGB()
{
    // https://www.khronos.org/registry/OpenGL/extensions/NVX/NVX_gpu_memory_info.txt
    if (sgl::SystemGL::get()->isGLExtensionAvailable("GL_NVX_gpu_memory_info")) {
        GLint freeMemKilobytes = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeMemKilobytes);
        float usedGigabytes = (initialFreeMemKilobytes-freeMemKilobytes) * 1e-6;
        return usedGigabytes;
    }
    // https://www.khronos.org/registry/OpenGL/extensions/NV/NV_query_resource.txt
    /*if (sgl::SystemGL::get()->isGLExtensionAvailable("GL_NV_query_resource")) {
        // Doesn't work for whatever reason :(
        /*GLint buffer[4096];
#ifndef GL_QUERY_RESOURCE_TYPE_VIDMEM_ALLOC_NV
#define GL_QUERY_RESOURCE_TYPE_VIDMEM_ALLOC_NV 0x9540
        typedef GLint (*PFNGLQUERYRESOURCENVPROC) (GLenum queryType, GLint tagId, GLuint bufSize, GLint *buffer);
        PFNGLQUERYRESOURCENVPROC glQueryResourceNV
                = (PFNGLQUERYRESOURCENVPROC)SDL_GL_GetProcAddress("glQueryResourceNV");
        glQueryResourceNV(GL_QUERY_RESOURCE_TYPE_VIDMEM_ALLOC_NV, -1, 4096, buffer);
#else
        glQueryResourceNV(GL_QUERY_RESOURCE_TYPE_VIDMEM_ALLOC_NV, 0, 6, buffer);
#endif
        // Used video memory stored at int at index 5 (in kilobytes).
        size_t usedKilobytes = buffer[5];
        float usedGigabytes = usedKilobytes * 1e-6;
        return usedGigabytes;
    }*/

    // Fallback
    return 0.0f;
}

