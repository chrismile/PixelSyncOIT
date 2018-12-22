/*
 * VideoWriter.cpp
 *
 *  Created on: 17.11.2017
 *      Author: Christoph Neuhauser
 */

#include <cerrno>
#include <cstring>
#include <GL/glew.h>

#include <Graphics/Window.hpp>
#include <Utils/AppSettings.hpp>
#include <Utils/Convert.hpp>
#include <Utils/File/Logfile.hpp>

#include "VideoWriter.hpp"

VideoWriter::VideoWriter(const char *filename, int frameW, int frameH, int framerate)
		: frameW(frameW), frameH(frameH), framebuf(NULL) {
	openFile(filename, framerate);
}

VideoWriter::VideoWriter(const char *filename, int framerate) : framebuf(NULL) {
	sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
	frameW = window->getWidth();
	frameH = window->getHeight();
	openFile(filename, framerate);
}

void VideoWriter::openFile(const char *filename, int framerate) {
	std::string command = std::string() + "ffmpeg -y -f rawvideo -s "
			+ sgl::toString(frameW) + "x" + sgl::toString(frameH) + " -pix_fmt rgb24 -r " + sgl::toString(framerate)
			+ " -i - -vf vflip -an -b:v 100M \"" + filename + "\"";
	std::cout << command << std::endl;
	avfile = popen(command.c_str(), "w");
	if (avfile == NULL) {
		sgl::Logfile::get()->writeError("ERROR in VideoWriter::VideoWriter: Couldn't open file.");
		sgl::Logfile::get()->writeError(std::string() + "Error in errno: " + strerror(errno));
	}
}

VideoWriter::~VideoWriter() {
	if (framebuf != NULL) {
		delete[] framebuf;
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
	if (framebuf == NULL) {
		framebuf = new uint8_t[frameW*frameH*3];
	}
	glReadPixels(0, 0, frameW, frameH, GL_RGB, GL_UNSIGNED_BYTE, framebuf);
	pushFrame(framebuf);
}
