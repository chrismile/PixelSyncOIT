//
// Created by christoph on 22.01.19.
//

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Utils/Events/Stream/Stream.hpp>
#include <Math/Geometry/MatrixUtil.hpp>

#include "MeshSerializer.hpp"
#include "TrajectoryLoader.hpp"
#include "HairLoader.hpp"

uint32_t toUint32Color(const glm::vec4 &vecColor) {
    uint32_t packedColor;
    packedColor = uint32_t(glm::round(glm::clamp(vecColor.r, 0.0f, 1.0f) * 255.0f)) & 0xFFu;
    packedColor |= (uint32_t(glm::round(glm::clamp(vecColor.g, 0.0f, 1.0f) * 255.0f)) & 0xFFu) << 8;
    packedColor |= (uint32_t(glm::round(glm::clamp(vecColor.b, 0.0f, 1.0f) * 255.0f)) & 0xFFu) << 16;
    packedColor |= (uint32_t(glm::round(glm::clamp(vecColor.a, 0.0f, 1.0f) * 255.0f)) & 0xFFu) << 24;
    return packedColor;
}

/**
 * Loads the specified hair file into memory.
 * For more on the file format see http://www.cemyuksel.com/research/hairmodels/
 */
void loadHairFile(const std::string &hairFilename, HairData &hairData) {
    std::ifstream file(hairFilename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() +
                "Error in readMesh3D: File \"" + hairFilename + "\" not found.");
        return;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);

    // Read magic number
    sgl::BinaryReadStream stream(buffer, size);
    const uint32_t FILE_FORMAT_HAIR_MAGIC_NUMBER = 0x52494148; // 48 41 49 52
    uint32_t magicNumber;
    stream.read(magicNumber);
    if (magicNumber != FILE_FORMAT_HAIR_MAGIC_NUMBER) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadHairFile: Invalid magic number in file \""
                                        + hairFilename + "\".");
        return;
    }



    // Rest of header after magic number
    uint32_t numStrands, totalNumPoints, settingsBitField;
    stream.read(numStrands);
    stream.read(totalNumPoints);
    stream.read(settingsBitField);

    // Read bitfield options
    bool hasSegmentsArray, hasPointsArray, hasThicknessArray, hasOpacityArray, hasColorArray;
    hasSegmentsArray = (settingsBitField & 0x1) == 1;
    hasPointsArray = (settingsBitField >> 1 & 0x1) == 1;
    hasThicknessArray = (settingsBitField >> 2 & 0x1) == 1;
    hasOpacityArray = (settingsBitField >> 3 & 0x1) == 1;
    hasColorArray = (settingsBitField >> 4 & 0x1) == 1;
    // Assertion: Needs to have points array
    if (!hasPointsArray) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadHairFile: Invalid bitfield in file \""
                                        + hairFilename + "\".");
        return;
    }

    // Read default values
    uint32_t defaultNumSegments;
    float defaultThickness, defaultOpacity;
    glm::vec3 defaultColor;
    stream.read(defaultNumSegments);
    stream.read(defaultThickness);
    stream.read(defaultOpacity);
    stream.read(defaultColor);

    // Read file information
    const size_t FILE_INFO_SIZE = 88;
    char fileInformation[FILE_INFO_SIZE];
    stream.read(fileInformation, FILE_INFO_SIZE);


    // ---------- Starting from here: Read actual hair data ----------

    // First: Read number of segments per strand and reserve data for point, thickness and color arrays (if necessary)
    hairData.filename = hairFilename;
    hairData.strands.resize(numStrands);
    if (hasSegmentsArray) {
        std::vector<uint16_t> segmentsArray;
        segmentsArray.resize(numStrands);
        stream.read(&segmentsArray.front(), numStrands * sizeof(uint16_t));
        for (size_t i = 0; i < numStrands; i++) {
            size_t numPoints = segmentsArray.at(i)+1;
            HairStrand &strand = hairData.strands.at(i);
            strand.points.resize(numPoints);
            if (hasThicknessArray) {
                strand.thicknesses.resize(numPoints);
            }
            if (hasOpacityArray || hasColorArray) {
                strand.colors.resize(numPoints);
            }
        }
    } else {
        for (HairStrand &strand : hairData.strands) {
            size_t numPoints = defaultNumSegments+1;
            strand.points.resize(numPoints);
            if (hasThicknessArray) {
                strand.thicknesses.resize(numPoints);
            }
            if (hasOpacityArray || hasColorArray) {
                strand.colors.resize(numPoints);
            }
        }
    }

    // Next: Read points array (obligatory)
    for (HairStrand &strand : hairData.strands) {
        stream.read(&strand.points.front(), strand.points.size() * sizeof(float) * 3);
    }

    // Next: Read thickness array (optional)
    hairData.hasThicknessArray = hasThicknessArray;
    hairData.defaultThickness = defaultThickness;
    if (hasThicknessArray) {
        for (HairStrand &strand : hairData.strands) {
            stream.read(&strand.thicknesses.front(), strand.thicknesses.size() * sizeof(float));
        }
    }

    // Next: Read opacity and color array (optional)
    hairData.hasColorArray = hasOpacityArray || hasColorArray;
    hairData.defaultOpacity = defaultOpacity;
    hairData.defaultColor = defaultColor;
    if (hairData.hasColorArray) {
        // 1. Read color and opacity arrays (optional)
        std::vector<std::vector<float>> opacityArrays;
        std::vector<std::vector<glm::vec3>> colorArrays;
        opacityArrays.resize(numStrands);
        colorArrays.resize(numStrands);
        if (hasOpacityArray) {
            for (size_t i = 0; i < numStrands; i++) {
                std::vector<float> &opacityArray = opacityArrays.at(i);
                opacityArray.resize(hairData.strands.at(i).colors.size());
                stream.read(&opacityArray.front(), hairData.strands.at(i).points.size() * sizeof(float));
            }
        }
        if (hasColorArray) {
            for (size_t i = 0; i < numStrands; i++) {
                std::vector<glm::vec3> &colorArray = colorArrays.at(i);
                colorArray.resize(hairData.strands.at(i).colors.size());
                stream.read(&colorArray.front(), hairData.strands.at(i).points.size() * sizeof(glm::vec3));
            }
        }

        // 2. Merge the arrays
        std::vector<std::vector<glm::vec4>> mergedColorOpacityArrays;
        mergedColorOpacityArrays.resize(numStrands);
        if (hasOpacityArray && hasColorArray) {
            for (size_t i = 0; i < numStrands; i++) {
                size_t numPoints = hairData.strands.at(i).points.size();
                std::vector<float> &opacityArray = opacityArrays.at(i);
                std::vector<glm::vec3> &colorArray = colorArrays.at(i);
                std::vector<glm::vec4> &mergedColorOpacityArray = mergedColorOpacityArrays.at(i);
                mergedColorOpacityArray.resize(hairData.strands.at(i).colors.size());
                for (size_t j = 0; j < numPoints; j++) {
                    mergedColorOpacityArray.at(j) = glm::vec4(colorArray.at(j), opacityArray.at(j));
                }
            }
        } else if (hasOpacityArray && !hasColorArray) {
            for (size_t i = 0; i < numStrands; i++) {
                size_t numPoints = hairData.strands.at(i).points.size();
                std::vector<float> &opacityArray = opacityArrays.at(i);
                std::vector<glm::vec4> &mergedColorOpacityArray = mergedColorOpacityArrays.at(i);
                mergedColorOpacityArray.resize(hairData.strands.at(i).colors.size());
                for (size_t j = 0; j < numPoints; j++) {
                    mergedColorOpacityArray.at(j) = glm::vec4(defaultColor, opacityArray.at(j));
                }
            }
        } else { //if (!hasOpacityArray && hasColorArray) {
            for (size_t i = 0; i < numStrands; i++) {
                size_t numPoints = hairData.strands.at(i).points.size();
                std::vector<glm::vec3> &colorArray = colorArrays.at(i);
                std::vector<glm::vec4> &mergedColorOpacityArray = mergedColorOpacityArrays.at(i);
                mergedColorOpacityArray.resize(hairData.strands.at(i).colors.size());
                for (size_t j = 0; j < numPoints; j++) {
                    mergedColorOpacityArray.at(j) = glm::vec4(colorArray.at(j), defaultOpacity);
                }
            }
        }

        // 3. Convert floating point colors & opacities to 32-bit RGBA colors
        for (size_t i = 0; i < numStrands; i++) {
            size_t numPoints = hairData.strands.at(i).points.size();
            std::vector<glm::vec4> &mergedColorOpacityArray = mergedColorOpacityArrays.at(i);
            std::vector<uint32_t> &colorArray = hairData.strands.at(i).colors;
            for (size_t j = 0; j < numPoints; j++) {
                colorArray.at(j) = toUint32Color(mergedColorOpacityArray.at(j));
            }
        }

    }
}

void downscaleHairData(HairData &hairData, float scalingFactor)
{
    hairData.defaultThickness *= scalingFactor;
    for (HairStrand &hairStrand : hairData.strands) {
        for (glm::vec3 &pt : hairStrand.points) {
            pt *= scalingFactor;
        }
    }

    // For some hair data files: Swap y-axis and z-axis
    if (!boost::starts_with(hairData.filename, "Data/Hair/ponytail")
            && !boost::starts_with(hairData.filename, "Data/Hair/bear")) {
        glm::mat4 hairRotationMatrix = sgl::matrixRowMajor(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
        );
        for (HairStrand &hairStrand : hairData.strands) {
            for (glm::vec3 &pt : hairStrand.points) {
                pt = sgl::transformPoint(hairRotationMatrix, pt);
            }
        }
    }
}

void convertHairDataToBinaryTriangleMesh(
        const std::string &hairFilename,
        const std::string &binaryFilename)
{
    // First, load the hair data from the specified file
    HairData hairData;
    loadHairFile(hairFilename, hairData);
    downscaleHairData(hairData, HAIR_MODEL_SCALING_FACTOR);

    BinaryMesh binaryMesh;
    binaryMesh.submeshes.push_back(BinarySubMesh());
    BinarySubMesh &submesh = binaryMesh.submeshes.front();
    submesh.vertexMode = sgl::VERTEX_MODE_TRIANGLES;

    std::vector<glm::vec3> globalVertexPositions;
    std::vector<glm::vec3> globalNormals;
    std::vector<uint32_t> globalColors;
    std::vector<uint32_t> globalIndices;

    initializeCircleData(3, hairData.defaultThickness);

    size_t numStrands = hairData.strands.size();
    for (HairStrand &strand : hairData.strands) {
        std::vector<glm::vec3> &pathLineCenters = strand.points;
        std::vector<float> &pathLineThicknesses = strand.thicknesses;
        std::vector<uint32_t> &pathLineColors = strand.colors;

        // Create tube render data
        std::vector<glm::vec3> localVertices;
        std::vector<uint32_t> localColors;
        std::vector<glm::vec3> localNormals;
        std::vector<uint32_t> localIndices;
        if (hairData.hasThicknessArray) {
            sgl::Logfile::get()->writeError("Error in convertHairDataToBinaryTriangleMesh: Variable thickness not yet "
                                            "supported.");
        } else {
            createTubeRenderData(pathLineCenters, pathLineColors, localVertices, localNormals, localColors, localIndices);
        }
        //createTubeRenderData(pathLineCenters, pathLineThicknesses, hairData.defaultThickness, pathLineColors,
        //        localVertices, localColors, localIndices);
        //createNormals(localVertices, localIndices, localNormals);

        // Local -> global
        for (size_t i = 0; i < localIndices.size(); i++) {
            globalIndices.push_back(localIndices.at(i) + globalVertexPositions.size());
        }
        globalVertexPositions.insert(globalVertexPositions.end(), localVertices.begin(), localVertices.end());
        globalColors.insert(globalColors.end(), localColors.begin(), localColors.end());
        globalNormals.insert(globalNormals.end(), localNormals.begin(), localNormals.end());
    }


    submesh.material.diffuseColor = hairData.defaultColor;
    submesh.material.opacity = hairData.defaultOpacity;
    submesh.indices = globalIndices;

    BinaryMeshAttribute positionAttribute;
    positionAttribute.name = "vertexPosition";
    positionAttribute.attributeFormat = sgl::ATTRIB_FLOAT;
    positionAttribute.numComponents = 3;
    positionAttribute.data.resize(globalVertexPositions.size() * sizeof(glm::vec3));
    memcpy(&positionAttribute.data.front(), &globalVertexPositions.front(), globalVertexPositions.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(positionAttribute);

    BinaryMeshAttribute lineNormalsAttribute;
    lineNormalsAttribute.name = "vertexNormal";
    lineNormalsAttribute.attributeFormat = sgl::ATTRIB_FLOAT;
    lineNormalsAttribute.numComponents = 3;
    lineNormalsAttribute.data.resize(globalNormals.size() * sizeof(glm::vec3));
    memcpy(&lineNormalsAttribute.data.front(), &globalNormals.front(), globalNormals.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(lineNormalsAttribute);

    if (hairData.hasColorArray) {
        BinaryMeshAttribute colorsAttribute;
        colorsAttribute.name = "vertexColor";
        colorsAttribute.attributeFormat = sgl::ATTRIB_UNSIGNED_BYTE;
        colorsAttribute.numComponents = 4;
        colorsAttribute.data.resize(globalColors.size() * sizeof(uint32_t));
        memcpy(&colorsAttribute.data.front(), &globalColors.front(), globalColors.size() * sizeof(uint32_t));
        submesh.attributes.push_back(colorsAttribute);
    }

    if (hairData.hasThicknessArray) {
        // TODO
        /*BinaryMeshAttribute thicknessesAttribute;
        thicknessesAttribute.name = "vertexThickness";
        thicknessesAttribute.attributeFormat = sgl::ATTRIB_UNSIGNED_INT;
        thicknessesAttribute.numComponents = 1;
        thicknessesAttribute.data.resize(globalColors.size() * sizeof(uint32_t));
        memcpy(&thicknessesAttribute.data.front(), &globalThicknesses.front(), globalThicknesses.size() * sizeof(float));
        submesh.attributes.push_back(thicknessesAttribute);*/
    }

    sgl::Logfile::get()->writeInfo(std::string() + "Summary: "
                              + sgl::toString(globalVertexPositions.size()) + " vertices, "
                              + sgl::toString(globalIndices.size()) + " indices.");
    sgl::Logfile::get()->writeInfo(std::string() + "Writing binary mesh...");
    writeMesh3D(binaryFilename, binaryMesh);
}
