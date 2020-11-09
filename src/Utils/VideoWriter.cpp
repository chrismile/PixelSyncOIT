/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2020, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cerrno>
#include <cstring>
#include <iostream>
#include <GL/glew.h>

#include <Graphics/Window.hpp>
#include <Utils/AppSettings.hpp>
#include <Utils/Convert.hpp>
#include <Utils/File/Logfile.hpp>

#include "VideoWriter.hpp"

VideoWriter::VideoWriter(const char *filename, int frameW, int frameH, int framerate)
        : frameW(frameW), frameH(frameH), framebuffer(NULL) {
    openFile(filename, framerate);
}

VideoWriter::VideoWriter(const char *filename, int framerate) : framebuffer(NULL) {
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    frameW = window->getWidth();
    frameH = window->getHeight();
    openFile(filename, framerate);
}

void VideoWriter::openFile(const char *filename, int framerate) {
    std::string command = std::string() + "ffmpeg -y -f rawvideo -s "
            + sgl::toString(frameW) + "x" + sgl::toString(frameH) + " -pix_fmt rgb24 -r " + sgl::toString(framerate)
//            + " -i - -vf vflip -an -b:v 100M \"" + filename + "\"";
            + " -i - -vf vflip -an -vcodec libx264 -crf 15 \"" + filename + "\"";
    std::cout << command << std::endl;
    avfile = popen(command.c_str(), "w");
    if (avfile == NULL) {
        sgl::Logfile::get()->writeError("ERROR in VideoWriter::VideoWriter: Couldn't open file.");
        sgl::Logfile::get()->writeError(std::string() + "Error in errno: " + strerror(errno));
    }
}

VideoWriter::~VideoWriter() {
    if (framebuffer != NULL) {
        delete[] framebuffer;
    }
    if (avfile) {
        pclose(avfile);
    }
}

void VideoWriter::pushFrame(uint8_t *pixels) {
    if (avfile) {
        fwrite((const void *)pixels, frameW*frameH*3, 1, avfile);
    }
}

void VideoWriter::pushWindowFrame() {
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    if (frameW != window->getWidth() || frameH != window->getHeight()) {
        sgl::Logfile::get()->writeError("ERROR in VideoWriter::VideoWriter: Window size changed.");
        sgl::Logfile::get()->writeError(std::string()
                + "Expected " + sgl::toString(frameW) + "x" + sgl::toString(frameH)
                + ", but got " + sgl::toString(window->getWidth()) + "x" + sgl::toString(window->getHeight()) + ".");
        return;
    }
    if (framebuffer == NULL) {
        framebuffer = new uint8_t[frameW * frameH * 3];
    }

    if (frameW % 4 != 0) {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
    }
    glReadPixels(0, 0, frameW, frameH, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
    pushFrame(framebuffer);
}
