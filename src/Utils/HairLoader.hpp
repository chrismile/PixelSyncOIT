//
// Created by christoph on 22.01.19.
//

#ifndef PIXELSYNCOIT_HAIRLOADER_HPP
#define PIXELSYNCOIT_HAIRLOADER_HPP

#include <vector>
#include <map>
#include <string>

#include <glm/glm.hpp>

const float HAIR_MODEL_SCALING_FACTOR = 0.005f;

struct HairStrand {
    // Obligatory data
    std::vector<glm::vec3> points;

    // --- For all following data fields: If the vector is empty, assume constant default value for all points ---
    // Thickness
    std::vector<float> thicknesses;

    // Opacity & color (merge to one array if either opacity or color is given)
    std::vector<uint32_t> colors;
};

struct HairData {
    std::vector<HairStrand> strands;

    // Only used if arrays in strands empty
    bool hasThicknessArray;
    bool hasColorArray;
    float defaultThickness;
    float defaultOpacity;
    glm::vec3 defaultColor;
};

/**
 * Loads the specified hair file into memory.
 * For more on the file format see http://www.cemyuksel.com/research/hairmodels/
 */
void loadHairFile(const std::string &hairFilename, HairData &hairData);

void convertHairDataToBinaryTriangleMesh(
        const std::string &hairFilename,
        const std::string &binaryFilename);

void downscaleHairData(HairData &hairData, float scalingFactor);

#endif //PIXELSYNCOIT_HAIRLOADER_HPP
