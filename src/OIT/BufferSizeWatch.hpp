//
// Created by christoph on 13.03.19.
//

#ifndef PIXELSYNCOIT_BUFFERSIZEWATCH_HPP
#define PIXELSYNCOIT_BUFFERSIZEWATCH_HPP

#include <string>
#include <cstdint>

class AutoPerfMeasurer;

extern void setCurrentAlgorithmBufferSizeBytes(size_t numBytes);
extern void setPerformanceMeasurer(AutoPerfMeasurer *measurer);

#endif //PIXELSYNCOIT_BUFFERSIZEWATCH_HPP
