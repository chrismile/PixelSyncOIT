//
// Created by christoph on 25.10.18.
//

#ifndef PIXELSYNCOIT_TESTPIXELSYNCPERFORMANCE_H
#define PIXELSYNCOIT_TESTPIXELSYNCPERFORMANCE_H

#include <Graphics/OpenGL/TimerGL.hpp>
#include "../OIT/OIT_Renderer.hpp"

class TestPixelSyncPerformance : public OIT_Renderer {
public:
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    TestPixelSyncPerformance();
    ~TestPixelSyncPerformance();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                   sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene() { }
    virtual void gatherEnd();
    virtual void renderToScreen();

    virtual bool isTestingMode() { return true; }

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

private:
    void setUniformData();
    sgl::ShaderAttributesPtr renderData;
    sgl::GeometryBufferPtr dataBuffer;
    //sgl::TimerGL timer;
};


#endif //PIXELSYNCOIT_TESTPIXELSYNCPERFORMANCE_H
