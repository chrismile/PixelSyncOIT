/*
 * VideoWriter.hpp
 *
 *  Created on: 17.11.2017
 *      Author: Christoph Neuhauser
 */

#ifndef UTILS_VIDEOWRITER_HPP_
#define UTILS_VIDEOWRITER_HPP_

#include <string>
#include <cstdio>

/** Video writer using the libav command line tool. Supports mp4 video.
 * Install the necessary dependencies for this writer to work:
 * https://wiki.ubuntuusers.de/avconv/
 */
class VideoWriter
{
public:
    /// Open mp4 video file with specified frame width and height
    VideoWriter(const char *filename, int frameW, int frameH, int framerate = 25);
    /// Open mp4 video file with frame width and height specified by application window
    VideoWriter(const char *filename, int framerate = 25);
    /// Closes file automatically
    ~VideoWriter();
    /// Push a 24-bit RGB frame (with width and height specified in constructor)
    void pushFrame(uint8_t *pixels);
    /// Retrieves frame automatically from current window
    void pushWindowFrame();

private:
    void openFile(const char *filename, int framerate = 25);
    FILE *avfile;
    int frameW;
    int frameH;
    uint8_t *framebuf; ///< Used for pushWindowFrame
};

#endif /* UTILS_VIDEOWRITER_HPP_ */
