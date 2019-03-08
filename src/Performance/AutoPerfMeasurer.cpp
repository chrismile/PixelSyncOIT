//
// Created by christoph on 27.09.18.
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

AutoPerfMeasurer::AutoPerfMeasurer(std::vector<InternalState> _states,
        const std::string &_csvFilename, const std::string &_perfProfileFilename,
        std::function<void(const InternalState&)> _newStateCallback)
       : states(_states), currentStateIndex(0), newStateCallback(_newStateCallback), file(_csvFilename)//,
         //perfTimeProfileFile(_csvFilename)
{
    // Write header
    file.writeRow({"Name", "Average Time (ms)", "Image Filename", "Memory (GB)", "SSIM", "RMSE", "PSNR",
                   "Time Stamp,Frame Time"});

    // Set initial state
    setNextState(true);
}

AutoPerfMeasurer::~AutoPerfMeasurer()
{
    writeCurrentModeData();
    file.close();
    //perfTimeProfileFile.close();
}


float nextModeCounter = 0.0f;
const float TIME_PER_MODE = 10.0f; // in seconds
bool AutoPerfMeasurer::update(float currentTime)
{
    nextModeCounter = currentTime;
    if (nextModeCounter > TIME_PER_MODE) {
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

void AutoPerfMeasurer::writeCurrentModeData()
{
    // Write row with performance metrics of this mode
    timerGL.stopMeasuring();
    double timeMS = timerGL.getTimeMS(currentState.name);
    file.writeCell(currentState.name);
    file.writeCell(sgl::toString(timeMS));

    // Make screenshot of rendering result with current algorithm
    std::string filename = std::string() + "images/" + currentState.name + ".png";
    file.writeCell(filename);
    sgl::BitmapPtr image(new sgl::Bitmap());
    image->fromFile(filename.c_str());
    if (currentState.oitAlgorithm == RENDER_MODE_OIT_DEPTH_PEELING) {
        referenceImage = image;
    }

    // Write current memory consumption in gigabytes
    file.writeCell(sgl::toString(getUsedVideoMemorySizeGB()));

    // Save normalized difference map
    std::string differenceMapFilename = std::string() + "images/" + currentState.name + " Difference" + ".png";
    sgl::BitmapPtr differenceMap = computeNormalizedDifferenceMap(referenceImage, image);
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
    auto performanceProfile = timerGL.getCurrentFrameTimeList();
    for (auto &perfPair : performanceProfile) {
        float timeStamp = perfPair.first;
        uint64_t frameTimeNS = perfPair.second;
        float frameTimeMS = frameTimeNS*1e-6;
        file.writeCell(sgl::toString(timeStamp) + "," + sgl::toString(frameTimeMS));
    }

    file.newRow();

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

void AutoPerfMeasurer::setNextState(bool first)
{
    if (!first) {
        writeCurrentModeData();
        currentStateIndex++;
    }

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

