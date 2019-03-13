//
// Created by christoph on 13.03.19.
//

#include "BufferSizeWatch.hpp"
#include "../Performance/AutoPerfMeasurer.hpp"

// Global pointer
AutoPerfMeasurer *g_Measurer = NULL;

void setCurrentAlgorithmBufferSizeBytes(size_t numBytes)
{
    if (g_Measurer != NULL) {
        g_Measurer->setCurrentAlgorithmBufferSizeBytes(numBytes);
    }
}

void setPerformanceMeasurer(AutoPerfMeasurer *measurer)
{
    g_Measurer = measurer;
}
