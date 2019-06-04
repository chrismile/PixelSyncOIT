//
// Created by christoph on 04.06.19.
//

#include <cstdio>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/AABB3.hpp>
#include <Utils/Events/Stream/Stream.hpp>
#include "NetCDFConverter.hpp"
#include "TrajectoryFile.hpp"

Trajectories loadTrajectoriesFromFile(const std::string &filename, TrajectoryType trajectoryType)
{
    Trajectories trajectories;

    std::string lowerCaseFilename = boost::to_lower_copy(filename);
    if (boost::ends_with(lowerCaseFilename, ".obj")) {
        trajectories = loadTrajectoriesFromObj(filename, trajectoryType);
    } else if (boost::ends_with(lowerCaseFilename, ".nc")) {
        trajectories = loadTrajectoriesFromNetCdf(filename, trajectoryType);
    } else if (boost::ends_with(lowerCaseFilename, ".binlines")) {
        trajectories = loadTrajectoriesFromBinLines(filename, trajectoryType);
    }

    sgl::AABB3 boundingBox;
    for (Trajectory &trajectory : trajectories) {
        for (glm::vec3 &position : trajectory.positions) {
            boundingBox.combine(position);
        }
    }

    // Normalize data for rings
    float minValue = std::min(boundingBox.getMinimum().x, std::min(boundingBox.getMinimum().y, boundingBox.getMinimum().z));
    float maxValue = std::max(boundingBox.getMaximum().x, std::max(boundingBox.getMaximum().y, boundingBox.getMaximum().z));

    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;
    if (isConvectionRolls) {
        minValue = 0;
        maxValue = 0.5;
    }

    glm::vec3 minVec(minValue, minValue, minValue);
    glm::vec3 maxVec(maxValue, maxValue, maxValue);

    if (isRings || isConvectionRolls) {
        for (Trajectory &trajectory : trajectories) {
            for (glm::vec3 &position : trajectory.positions) {
                position = (position - minVec) / (maxVec - minVec);
                if (isConvectionRolls) {
                    glm::vec3 dims = glm::vec3(1);
                    dims.y = boundingBox.getDimensions().y;
                    position -= dims;
                }
            }
        }
    }

    return trajectories;
}

Trajectories loadTrajectoriesFromObj(const std::string &filename, TrajectoryType trajectoryType)
{
    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;
    Trajectories trajectories;

    std::vector<glm::vec3> globalLineVertices;
    std::vector<float> globalLineVertexAttributes;

    FILE *file = fopen(filename.c_str(), "r");
    if (!file) {
        sgl::Logfile::get()->writeError(std::string() + "Error in convertObjTrajectoryDataToBinaryLineMesh: File \""
                                        + filename + "\" does not exist.");
        return trajectories;
    }
    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *fileBuffer = new char[length];
    fread(fileBuffer, 1, length, file);
    fclose(file);
    std::string lineBuffer;
    std::string numberString;

    for (size_t charPtr = 0; charPtr < length; charPtr++) {
        while (charPtr < length) {
            char currentChar = fileBuffer[charPtr];
            if (currentChar == '\n' || currentChar == '\r') {
                charPtr++;
                break;
            }
            lineBuffer.push_back(currentChar);
            charPtr++;
        }

        if (lineBuffer.size() == 0) {
            continue;
        }

        char command = lineBuffer.at(0);
        char command2 = ' ';
        if (lineBuffer.size() > 1) {
            command2 = lineBuffer.at(1);
        }

        if (command == 'g') {
            // New path
            /*static int ctr = 0;
            if (ctr >= 999) {
                Logfile::get()->writeInfo(std::string() + "Parsing trajectory line group " + line.at(1) + "...");
            }
            ctr = (ctr + 1) % 1000;*/
        } else if (command == 'v' && command2 == 't') {
            // Path line vertex attribute
            float attr = 0.0f;
            sscanf(lineBuffer.c_str()+2, "%f", &attr);
            globalLineVertexAttributes.push_back(attr);
        } else if (command == 'v' && command2 == 'n') {
            // Not supported so far
        } else if (command == 'v') {
            // Path line vertex position
            glm::vec3 position;
            if (isConvectionRolls) {
                sscanf(lineBuffer.c_str()+2, "%f %f %f", &position.x, &position.z, &position.y);
            } else {
                sscanf(lineBuffer.c_str()+2, "%f %f %f", &position.x, &position.y, &position.z);
            }
            globalLineVertices.push_back(position);
        } else if (command == 'l') {
            // Get indices of current path line
            std::vector<uint32_t> currentLineIndices;
            for (size_t linePtr = 2; linePtr < lineBuffer.size(); linePtr++) {
                char currentChar = lineBuffer.at(linePtr);
                bool isWhitespace = currentChar == ' ' || currentChar == '\t';
                if (isWhitespace && numberString.size() != 0) {
                    currentLineIndices.push_back(atoi(numberString.c_str()) - 1);
                    numberString.clear();
                } else if (!isWhitespace) {
                    numberString.push_back(currentChar);
                }
            }
            if (numberString.size() != 0) {
                currentLineIndices.push_back(atoi(numberString.c_str()) - 1);
                numberString.clear();
            }

            Trajectory trajectory;

            std::vector<float> pathLineVorticities;
            trajectory.positions.reserve(currentLineIndices.size());
            pathLineVorticities.reserve(currentLineIndices.size());
            for (size_t i = 0; i < currentLineIndices.size(); i++) {
                trajectory.positions.push_back(globalLineVertices.at(currentLineIndices.at(i)));
                pathLineVorticities.push_back(globalLineVertexAttributes.at(currentLineIndices.at(i)));
            }

            // Compute importance criteria
            computeTrajectoryAttributes(
                    trajectoryType, trajectory.positions, pathLineVorticities, trajectory.attributes);

            // Line filtering for WCB trajectories
            //if (trajectoryType == TRAJECTORY_TYPE_WCB) {
            //  if (importanceCriteriaIn.at(3).size() > 0 && importanceCriteriaIn.at(3).at(0) < 500.0f) {
            //      continue;
            //  }
            //}

            trajectories.push_back(trajectory);
        } else if (command = '#') {
            // Ignore comments
        } else {
            //Logfile::get()->writeError(std::string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
        }

        lineBuffer.clear();
    }

    return trajectories;
}

Trajectories loadTrajectoriesFromNetCdf(const std::string &filename, TrajectoryType trajectoryType) {
    Trajectories trajectories = loadNetCdfFile(filename);

    for (Trajectory &trajectory : trajectories) {
        // Compute importance criteria
        std::vector<float> oldAttributes = trajectory.attributes.at(0);
        trajectory.attributes.clear();
        computeTrajectoryAttributes(
                trajectoryType, trajectory.positions, oldAttributes, trajectory.attributes);
    }

    return trajectories;
}

Trajectories loadTrajectoriesFromBinLines(const std::string &filename, TrajectoryType trajectoryType) {
    Trajectories trajectories;

    std::ifstream file(filename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadTrajectoriesFromBinLines: File \""
                + filename + "\" not found.");
        return trajectories;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);

    // Read format version
    sgl::BinaryReadStream stream(buffer, size);
    const uint32_t LINE_FILE_FORMAT_VERSION = 1u;
    uint32_t versionNumber;
    stream.read(versionNumber);
    if (versionNumber != LINE_FILE_FORMAT_VERSION) {
        sgl::Logfile::get()->writeError(std::string()
                + "Error in loadTrajectoriesFromBinLines: Invalid magic number in file \"" + filename + "\".");
        return trajectories;
    }



    // Rest of header after format version
    uint32_t numTrajectories, numAttributes, trajectoryNumPoints;
    stream.read(numTrajectories);
    stream.read(numAttributes);
    trajectories.resize(numTrajectories);

    for (uint32_t trajectoryIndex = 0; trajectoryIndex < numTrajectories; trajectoryIndex++) {
        Trajectory &currentTrajectory = trajectories.at(trajectoryIndex);
        stream.read(trajectoryNumPoints);
        currentTrajectory.positions.resize(trajectoryNumPoints);
        stream.read((void*)&currentTrajectory.positions.front(), sizeof(glm::vec3)*trajectoryNumPoints);
        currentTrajectory.attributes.resize(numAttributes);
        for (uint32_t attributeIndex = 0; attributeIndex < numAttributes; attributeIndex++) {
            std::vector<float> &currentAttribute = currentTrajectory.attributes.at(attributeIndex);
            currentAttribute.resize(trajectoryNumPoints);
            stream.read((void*)&currentAttribute.front(), sizeof(float)*trajectoryNumPoints);
        }
    }

    return trajectories;
}
