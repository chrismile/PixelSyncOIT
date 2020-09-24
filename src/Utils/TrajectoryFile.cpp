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
#include "BezierCurve.hpp"
#include <iostream>
#include <memory>
#include <sstream>

Trajectories convertTrajectoriesToBezierCurves(const Trajectories& inTrajectories)
{
    // 1) Determine Bezier segments
    std::vector<std::vector<BezierCurve>> curves(inTrajectories.size());
    // Store the arclength of all segments along a curve
    std::vector<float> curveArcLengths(inTrajectories.size(), 0.0f);

    // Average segment length;
    float avgSegLength = 0.0f;
    float minSegLength = std::numeric_limits<float>::max();
    float numSegments = 0;

    int32_t trajCounter = 0;
    for (const auto& trajectory : inTrajectories)
    {
        std::vector<BezierCurve>& curveSet = curves[trajCounter];

        const int maxVertices = trajectory.positions.size();

        Trajectory BezierTrajectory;

        float minT = 0.0f;
        float maxT = 1.0f;

        for (int v = 0; v < maxVertices - 1; ++v)
        {
            const glm::vec3& pos0 = trajectory.positions[std::max(0, v - 1)];
            const glm::vec3& pos1 = trajectory.positions[v];
            const glm::vec3& pos2 = trajectory.positions[v + 1];
            const glm::vec3& pos3 = trajectory.positions[std::min(v + 2, maxVertices - 1)];

            const glm::vec3 cotangent1 = glm::normalize(pos2 - pos0);
            const glm::vec3 cotangent2 = glm::normalize(pos3 - pos1);
            const glm::vec3 tangent = pos2 - pos1;
            const float lenTangent = glm::length(tangent);

            avgSegLength += lenTangent;
            minSegLength = std::min(minSegLength, lenTangent);
            numSegments++;

            glm::vec3 C0 = pos1;
            glm::vec3 C1 = pos1 + cotangent1 * lenTangent * 0.5f;
            glm::vec3 C2 = pos2 - cotangent2 * lenTangent * 0.5f;
            glm::vec3 C3 = pos2;

//            const std::vector<float>& attributes = trajectory.attributes[v];

//            curveSet.emplace_back({{ C0, C1, C2, C3 }}, minT, maxT);
            BezierCurve BCurve({{ C0, C1, C2, C3}}, minT, maxT);

            curveSet.push_back(BCurve);
            curveArcLengths[trajCounter] += BCurve.totalArcLength;

            minT += 1.0f;
            maxT += 1.0f;
        }

        trajCounter++;
    }

    avgSegLength /= numSegments;

    // 2) Compute min max values of all attributes across all trajectories
    const uint32_t numVariables = inTrajectories[0].attributes.size();
    if (numVariables <= 0)
    {
        std::cout << "[ERROR]: No variable was found in trajectory file" << std::endl;
        std::exit(-2);
    }

    std::vector<glm::vec2> attributesMinMax(numVariables,
            glm::vec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()));

    const uint32_t maxNumVariables = inTrajectories[0].attributes.size();
    const uint32_t numLines = inTrajectories.size();

    // 2.5) Create buffer array with all variables and statistics
    std::vector<std::vector<float>> multiVarData(numLines);
    std::vector<std::vector<float>> multiVarHistograms(numLines);
    std::vector<LineDesc> lineDescs(numLines);
    std::vector<std::vector<VarDesc>> lineMultiVarDescs(numLines);
    std::vector<std::vector<VarDesc>> lineHistogramDescs(numLines);

    uint32_t lineOffset = 0;
    uint32_t lineID = 0;
    uint32_t numBins = 0;
    uint32_t histogramOffset = 0;

    for (const auto& trajectory : inTrajectories)
    {
        uint32_t varOffsetPerLine = 0;
        const int maxVertices = trajectory.positions.size();

        for (auto v = 0; v < maxNumVariables; ++v)
        {
            VarDesc varDescPerLine = {0 };
            varDescPerLine.minMax = glm::vec2(std::numeric_limits<float>::max(),
                                       std::numeric_limits<float>::lowest());
            varDescPerLine.startIndex = varOffsetPerLine;
            varDescPerLine.dummy = 0.0f;

            const auto& variableArray = trajectory.attributes[v];

//            uint32_t vCounter = 0;
            for (const auto& variable : variableArray)
            {
                attributesMinMax[v].x = std::min(attributesMinMax[v].x, variable);
                attributesMinMax[v].y = std::max(attributesMinMax[v].y, variable);

                varDescPerLine.minMax.x = std::min(varDescPerLine.minMax.x, variable);
                varDescPerLine.minMax.y = std::max(varDescPerLine.minMax.y, variable);

                multiVarData[lineID].push_back(variable);
                numBins = trajectory.numBins;



//                vCounter++;
            }

            // Create histogram buffer
            for (int v = 0; v < maxVertices - 1; ++v)
            {
                std::copy(trajectory.histograms[v].begin(),
                          trajectory.histograms[v].end(),
                          std::back_inserter(multiVarHistograms[lineID]));
            }

            lineMultiVarDescs[lineID].push_back(varDescPerLine);
            varOffsetPerLine += variableArray.size();
        }
        lineDescs[lineID].startIndex = lineOffset;
        lineDescs[lineID].numValues = varOffsetPerLine;
        lineDescs[lineID].startHistogramIndex = histogramOffset;

        histogramOffset += maxVertices * numBins * numVariables;
        lineOffset += varOffsetPerLine;
        lineID++;
    }


    // 3) Compute several equally-distributed / equi-distant points along Bezier curves.
    // Store these points in a new trajectory
    float rollSegLength = avgSegLength / maxNumVariables;// avgSegLength * 0.2f;

    Trajectories newTrajectories(inTrajectories.size());

    for (int32_t traj = 0; traj < int(inTrajectories.size()); ++traj)
    {
        float curArcLength = 0.0f;
        Trajectory newTrajectory;

        // Obtain set of Bezier Curves
        std::vector<BezierCurve>& BCurves = curves[traj];
        // Obtain total arc length
        const float totalArcLength = curveArcLengths[traj];

        glm::vec3 pos;
        glm::vec3 tangent;
        uint32_t lineID = 0;
        uint32_t varIDPerLine = 0;
        std::vector<float> attributes = inTrajectories[traj].attributes[lineID];
        // Start with first segment
        BCurves[0].evaluate(0, pos, tangent);

        newTrajectory.positions.push_back(pos);
        newTrajectory.tangents.push_back(tangent);
        newTrajectory.segmentID.push_back(lineID);

        // Now we store variable, min, and max, and var ID per vertex as new attributes
        newTrajectory.attributes.resize(8);
        float varValue = inTrajectories[traj].attributes[varIDPerLine][lineID];
        newTrajectory.attributes[0].push_back(varValue);
        newTrajectory.attributes[1].push_back(attributesMinMax[varIDPerLine].x);
        newTrajectory.attributes[2].push_back(attributesMinMax[varIDPerLine].y);
        // var ID
        newTrajectory.attributes[3].push_back(static_cast<float>(varIDPerLine));
        // var element index
        newTrajectory.attributes[4].push_back(static_cast<float>(lineID));
        // line ID
        newTrajectory.attributes[5].push_back(static_cast<float>(traj));
        // next line ID
        newTrajectory.attributes[6].push_back(static_cast<float>(std::min(lineID, uint32_t(BCurves.size() - 1))));
        // interpolant t
        newTrajectory.attributes[7].push_back(0.0f);



//        newTrajectory.attributes.resize(inTrajectories[traj].attributes.size());


//        for (auto a = 0; a < inTrajectories[traj].attributes.size(); ++a)
//        {
//            newTrajectory.attributes[a].push_back(inTrajectories[traj].attributes[a][lineID]);
//        }

        curArcLength += rollSegLength;
//
//        lineID = 0;
        float sumArcLengths = 0.0f;  //BCurves[0].totalArcLength;
        float sumArcLengthsNext = BCurves[0].totalArcLength;

        varIDPerLine++;


        while(curArcLength <= totalArcLength)
        {
            // Obtain current Bezier segment index based on arc length
            while (sumArcLengthsNext <= curArcLength)
            {
                varIDPerLine = 0;
                lineID++;
                sumArcLengths = sumArcLengthsNext;
                sumArcLengthsNext += BCurves[lineID].totalArcLength;
            }

            const auto& BCurve = BCurves[lineID];

            float t = BCurve.solveTForArcLength(curArcLength - sumArcLengths);

            BCurves[lineID].evaluate(t, pos, tangent);
//            attributes = inTrajectories[traj].attributes[lineID];

            newTrajectory.positions.push_back(pos);
            newTrajectory.tangents.push_back(tangent);
            newTrajectory.segmentID.push_back(lineID);

            if (varIDPerLine < numVariables)
            {
                float varValue = inTrajectories[traj].attributes[varIDPerLine][lineID];
                newTrajectory.attributes[0].push_back(varValue);
                newTrajectory.attributes[1].push_back(attributesMinMax[varIDPerLine].x);
                newTrajectory.attributes[2].push_back(attributesMinMax[varIDPerLine].y);
                // var ID
                newTrajectory.attributes[3].push_back(static_cast<float>(varIDPerLine));

            }
            else
            {
                newTrajectory.attributes[0].push_back(0.0);
                newTrajectory.attributes[1].push_back(0.0);
                newTrajectory.attributes[2].push_back(0.0);
                newTrajectory.attributes[3].push_back(-1.0);
            }

            // var element index
            newTrajectory.attributes[4].push_back(static_cast<float>(lineID));
            // line ID
            newTrajectory.attributes[5].push_back(static_cast<float>(traj));
            // next line ID
            newTrajectory.attributes[6].push_back(static_cast<float>(std::min(lineID + 1, uint32_t(BCurves.size() - 1))));
            // interpolant t
            float normalizedT = BCurve.normalizeT(t);
            newTrajectory.attributes[7].push_back(normalizedT);


            // We only store one variable per trajectory vertex
//            for (auto a = 0; a < inTrajectories[traj].attributes.size(); ++a)
//            {
//                newTrajectory.attributes[a].push_back(inTrajectories[traj].attributes[a][lineID]);
//            }

            curArcLength += rollSegLength;
            varIDPerLine++;
        }

        newTrajectory.lineDesc = lineDescs[traj];
        newTrajectory.multiVarData = multiVarData[traj];
        newTrajectory.multiVarHistograms = multiVarHistograms[traj];
        newTrajectory.numBins = numBins;
        newTrajectory.multiVarDescs = lineMultiVarDescs[traj];
        newTrajectory.multiVarNames = inTrajectories[traj].multiVarNames;

        newTrajectories[traj] = newTrajectory;
    }

    return newTrajectories;
}

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

    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isUCLA = trajectoryType == TRAJECTORY_TYPE_UCLA;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;
    bool isCfdData = trajectoryType == TRAJECTORY_TYPE_CFD;
    bool isMultiVar = trajectoryType == TRAJECTORY_TYPE_MULTIVAR;

    if (isMultiVar)
    {
        // Convert to Bezier curve segments
        trajectories = convertTrajectoriesToBezierCurves(trajectories);
    }


    sgl::AABB3 boundingBox;
    for (Trajectory &trajectory : trajectories) {
        for (glm::vec3 &position : trajectory.positions) {
            boundingBox.combine(position);
        }
    }

    glm::vec3 minVec(boundingBox.getMinimum());
    glm::vec3 maxVec(boundingBox.getMaximum());

    float minAttr = std::numeric_limits<float>::max();
    float maxAttr = std::numeric_limits<float>::lowest();

    if (isConvectionRolls) {
        minVec = glm::vec3(0);
        maxVec = glm::vec3(0.5);
    } else if (isUCLA)
    {
        minVec = glm::vec3(glm::min(boundingBox.getMinimum().x, std::min(boundingBox.getMinimum().y, boundingBox.getMinimum().z)));
        maxVec = glm::vec3(glm::max(boundingBox.getMaximum().x, std::max(boundingBox.getMaximum().y, boundingBox.getMaximum().z)));

        for(const Trajectory& traj : trajectories)
        {
            for (const float& attr : traj.attributes[0])
            {
                minAttr = std::min(minAttr, attr);
                maxAttr = std::max(maxAttr, attr);
            }
        }

    } else {
        // Normalize data for rings
        float minValue = glm::min(boundingBox.getMinimum().x, std::min(boundingBox.getMinimum().y, boundingBox.getMinimum().z));
        float maxValue = glm::max(boundingBox.getMaximum().x, std::max(boundingBox.getMaximum().y, boundingBox.getMaximum().z));
        minVec = glm::vec3(minValue);
        maxVec = glm::vec3(maxValue);
    }

    if (isRings || isConvectionRolls || isCfdData || isUCLA) {
        for (Trajectory &trajectory : trajectories) {
            for (glm::vec3 &position : trajectory.positions) {
                position = (position - minVec) / (maxVec - minVec);
                if (isConvectionRolls || isCfdData) {
                    glm::vec3 dims = glm::vec3(1);
                    dims.y = boundingBox.getDimensions().y;
                    position -= dims;
                }
            }
        }
    }

    // if UCLA --> normalize attributes
    if (isUCLA)
    {
        for(Trajectory& traj : trajectories)
        {
            for (float& attr : traj.attributes[0])
            {
                attr = (attr - minAttr) / (maxAttr - minAttr);
            }
        }
    }

    return trajectories;
}

Trajectories loadTrajectoriesFromObj(const std::string &filename, TrajectoryType trajectoryType)
{
    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;
    bool isUCLA = trajectoryType == TRAJECTORY_TYPE_UCLA;

    bool isMultiVar = trajectoryType == TRAJECTORY_TYPE_MULTIVAR;
//    const uint8_t NUM_MULTI_VARIABLES = 6;

    Trajectories trajectories;

    std::vector<glm::vec3> globalLineVertices;
    std::vector<float> globalLineVertexAttributes;
    std::vector<float> globalLineHistograms;
    std::vector<std::string> globalMultiVarNames;
    uint8_t globalNumVars = 0;
    float globalNumBinsPerHisto = 0;

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

    for (size_t charPtr = 0; charPtr < length; ) {
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
            if (isMultiVar)
            {
//                float attrs[NUM_MULTI_VARIABLES];
                // Replace with string stream to dynamically parse a number of attributes
                // Use different attribute names to store multiple variables at each vertex (for statistics computation)
                std::stringstream lineStream(lineBuffer.c_str() + 2);

                while (lineStream.good())
                {
                    float value = 0;
                    lineStream >> value;
                    globalLineVertexAttributes.push_back(value);
                }

//                sscanf(lineBuffer.c_str() + 2, "%f %f %f %f %f %f", &attrs[0], &attrs[1], &attrs[2],
//                       &attrs[3], &attrs[4], &attrs[5]);
//
//                for (auto j = 0; j < NUM_MULTI_VARIABLES; ++j)
//                {
////                    float attr = 0;
////                    sscanf(lineBuffer.c_str() + 2 + 2 * j, "%f", &attr);
//                    globalLineVertexAttributes.push_back(attrs[j]);
//                }
            }
            else
            {
                // Path line vertex attribute
                float attr = 0.0f;

                sscanf(lineBuffer.c_str() + 2, "%f", &attr);
                globalLineVertexAttributes.push_back(attr);
            }
        } else if (isMultiVar && (command == 'v' && command2 == 'g'))
        {
            std::stringstream lineStream(lineBuffer.c_str() + 2);
//            uint8_t counter = 0;
            while (lineStream.good())
            {
                std::string name;
                lineStream >> name;
                globalMultiVarNames.push_back(name);
            }

            globalNumVars = globalMultiVarNames.size();

            std::cout << "Found " << globalMultiVarNames.size()
                      << " variables\n";

        } else if (isMultiVar && (command == 'v' && command2 == 'h'))
        {
            std::stringstream lineStream(lineBuffer.c_str() + 2);
            lineStream >> globalNumBinsPerHisto;

//            uint8_t counter = 0;
            while (lineStream.good())
            {
                float value;
                lineStream >> value;
                globalLineHistograms.push_back(value);
            }

            std::cout << "Number of histogram bins: " << globalNumBinsPerHisto << "\n";


            } else if (command == 'v' && command2 == 'n') {
            // Not supported so far
        } else if (command == 'v') {
            // Path line vertex position
            glm::vec3 position;
            if (isUCLA)
            {
                sscanf(lineBuffer.c_str()+2, "%f %f %f", &position.x, &position.y, &position.z);
            }
            else if (isConvectionRolls) {
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

            if (currentLineIndices.size() < 3)
            {
                lineBuffer.clear();
                continue;
            }

            Trajectory trajectory;

            std::vector<float> pathLineVorticities;
            trajectory.positions.reserve(currentLineIndices.size());
            trajectory.histograms.reserve(currentLineIndices.size());
            pathLineVorticities.reserve(currentLineIndices.size());
            for (size_t i = 0; i < currentLineIndices.size(); i++) {
                glm::vec3 pos = globalLineVertices.at(currentLineIndices.at(i));

                // Remove invalid line points (used in many scientific datasets to indicate invalid lines).
                const float MAX_VAL = 1e10f;
                if (std::fabs(pos.x) > MAX_VAL || std::fabs(pos.y) > MAX_VAL || std::fabs(pos.z) > MAX_VAL) {
                    continue;
                }

                const uint32_t currentLineIndex = currentLineIndices.at(i);

                std::vector<float> histogramValues;

                trajectory.positions.push_back(globalLineVertices.at(currentLineIndex));

                uint8_t numVariables = (isMultiVar) ? globalNumVars : 1;
                for (auto v = 0; v < numVariables; ++v)
                {
                    pathLineVorticities.push_back(
                            globalLineVertexAttributes.at(currentLineIndex * numVariables + v));

                    if (isMultiVar)
                    {
                        trajectory.numBins = globalNumBinsPerHisto;

                        for (auto h = 0; h < globalNumBinsPerHisto; ++h)
                        {
//                            const uint8_t numBins = 5;
                            const uint32_t index = currentLineIndex *   numVariables *
                                                   globalNumBinsPerHisto + v * globalNumBinsPerHisto + h;

                            histogramValues.push_back(
                                globalLineHistograms.at(index));
                        }


                    }

                }

                if (isMultiVar)
                {
                    trajectory.histograms.push_back(histogramValues);
                }
            }

            // Compute importance criteria
            computeTrajectoryAttributes(
                    trajectoryType, trajectory.positions, pathLineVorticities,
                    trajectory.attributes, globalNumVars);





            // Line filtering for WCB trajectories
            //if (trajectoryType == TRAJECTORY_TYPE_WCB) {
            //  if (importanceCriteriaIn.at(3).size() > 0 && importanceCriteriaIn.at(3).at(0) < 500.0f) {
            //      continue;
            //  }
            //}

            for (auto& varName : globalMultiVarNames)
            {
                trajectory.multiVarNames.push_back(varName);
            }

            trajectories.push_back(trajectory);
        } else if (command = '#') {
            // Ignore comments
        } else {
            //Logfile::get()->writeError(std::string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
        }

        lineBuffer.clear();
    }

    // compute byte size of raw representation with 1 attribute for paper
    uint64_t byteSize = 0;
    for (const auto& traj : trajectories)
    {
        byteSize += traj.positions.size() * sizeof(float) * 3;
        byteSize += traj.attributes[0].size() * sizeof(float);
        if (isMultiVar)
        {
            byteSize += traj.histograms.size() * sizeof(float);
        }
    }

    byteSize = byteSize / 1024 / 1024;
    std::cout << "Raw byte size of obj file: " << byteSize << "MB" << std::endl << std::flush;

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
