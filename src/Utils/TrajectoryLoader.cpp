//
// Created by christoph on 08.09.18.
//

#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Math/Math.hpp>

#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "MeshSerializer.hpp"
#include "TrajectoryLoader.hpp"

using namespace sgl;

static std::vector<glm::vec2> circlePoints2D;

void getPointsOnCircle(std::vector<glm::vec2> &points, const glm::vec2 &center, float radius, int numSegments)
{
    float theta = 2.0f * 3.1415926f / (float)numSegments;
    float tangetialFactor = tan(theta); // opposite / adjacent
    float radialFactor = cos(theta); // adjacent / hypotenuse
    glm::vec2 position(radius, 0.0f);

    for (int i = 0; i < numSegments; i++) {
        points.push_back(position + center);

        // Add the tangent vector and correct the position using the radial factor.
        glm::vec2 tangent(-position.y, position.x);
        position += tangetialFactor * tangent;
        position *= radialFactor;
    }
}

const int NUM_CIRCLE_SEGMENTS = 3;
const float TUBE_RADIUS = 0.001f;

void initializeCircleData(int numSegments, float radius)
{
    circlePoints2D.clear();
    getPointsOnCircle(circlePoints2D, glm::vec2(0.0f, 0.0f), radius, numSegments);
}

/**
 * Returns a oriented and shifted copy of a 2D circle in 3D space.
 * The number
 * @param center: The center of the circle in 3D space.
 * @param normal: The normal orthogonal to the circle plane.
 * @param lastTangent: The tangent of the last circle.
 * @return The points on the oriented circle.
 */
void insertOrientedCirclePoints(std::vector<glm::vec3> &vertices, std::vector<glm::vec3> &normals,
        const glm::vec3 &center, const glm::vec3 &normal, glm::vec3 &lastTangent)
{
    if (circlePoints2D.size() == 0) {
        initializeCircleData(NUM_CIRCLE_SEGMENTS, TUBE_RADIUS);
    }

    glm::vec3 tangent, binormal;
    glm::vec3 helperAxis = lastTangent;
    //if (std::abs(glm::dot(helperAxis, normal)) > 0.9f) {
    if (glm::length(glm::cross(helperAxis, normal)) < 0.01f) {
        // If normal == helperAxis
        helperAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    tangent = glm::normalize(helperAxis - normal * glm::dot(helperAxis, normal)); // Gram-Schmidt
    //glm::vec3 tangent = glm::normalize(glm::cross(normal, helperAxis));
    binormal = glm::normalize(glm::cross(normal, tangent));
    lastTangent = tangent;


    // In column-major order
    glm::mat4 tangentFrameMatrix(
            tangent.x,  tangent.y,  tangent.z,  0.0f,
            binormal.x, binormal.y, binormal.z, 0.0f,
            normal.x,   normal.y,   normal.z,   0.0f,
            0.0f,       0.0f,       0.0f,       1.0f);
    glm::mat4 translation(
            1.0f,     0.0f,     0.0f,     0.0f,
            0.0f,     1.0f,     0.0f,     0.0f,
            0.0f,     0.0f,     1.0f,     0.0f,
            center.x, center.y, center.z, 1.0f);
    glm::mat4 transform = translation * tangentFrameMatrix;

    for (const glm::vec2 &circlePoint : circlePoints2D) {
        glm::vec4 transformedPoint = transform * glm::vec4(circlePoint.x, circlePoint.y, 0.0f, 1.0f);
        vertices.push_back(glm::vec3(transformedPoint.x, transformedPoint.y, transformedPoint.z));
        glm::vec3 normal = glm::vec3(transformedPoint.x, transformedPoint.y, transformedPoint.z) - center;
        normal = glm::normalize(normal);
        normals.push_back(normal);
    }
}


struct TubeNode
{
    /// Center vertex position
    glm::vec3 center;

    /// Tangent pointing in direction of next node (or negative direction of last node for the final node in the list).
    glm::vec3 tangent;

    /// Circle points (circle with center of tube node, in plane with normal vector of tube node)
    std::vector<glm::vec3> circleVertices;

    std::vector<uint32_t> circleIndices;
};

/**
 * @param pathLineCenters: The (input) path line points to create a tube from.
 * @param pathLineAttributes: The (input) path line point vertex attributes (belonging to pathLineCenters).
 * @param vertices: The (output) vertex points, which are a set of oriented circles around the centers (see above).
 * @param indices: The (output) indices specifying how tube triangles are built from the circle vertices.
 */
template<typename T>
void createTubeRenderData(const std::vector<glm::vec3> &pathLineCenters,
                          const std::vector<T> &pathLineAttributes,
                          std::vector<glm::vec3> &vertices,
                          std::vector<glm::vec3> &normals,
                          std::vector<T> &vertexAttributes,
                          std::vector<uint32_t> &indices)
{
    int n = (int)pathLineCenters.size();
    if (n < 2) {
        sgl::Logfile::get()->writeError("Error in createTube: n < 2");
        return;
    }

    /// Circle points (circle with center of tube node, in plane with normal vector of tube node)
    vertices.reserve(n*circlePoints2D.size());
    normals.reserve(n*circlePoints2D.size());
    vertexAttributes.reserve(n*circlePoints2D.size());
    indices.reserve((n-1)*circlePoints2D.size()*6);

    std::vector<TubeNode> tubeNodes;
    tubeNodes.reserve(n);
    int numVertexPts = 0;

    // Turbulence dataset: Remove fixed point whirls
    glm::vec3 diffFirstLast = pathLineCenters.front() - pathLineCenters.back();
    if (glm::length(diffFirstLast) < 0.01f) {
        return;
    }

    // First, create a list of tube nodes
    glm::vec3 lastNormal = glm::vec3(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < n; i++) {
        glm::vec3 tangent;
        if (i == 0) {
            // First node
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = pathLineCenters.at(i) - pathLineCenters.at(i-1);
        } else {
            // Node with two neighbors - use both normals
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
            //normal += pathLineCenters.at(i) - pathLineCenters.at(i-1);
        }

        if (glm::length(tangent) < 0.0001f) {
            // In case the two vertices are almost identical, just skip this path line segment
            continue;
        }

        TubeNode node;
        node.center = pathLineCenters.at(i);
        node.tangent = glm::normalize(tangent);
        insertOrientedCirclePoints(vertices, normals, node.center, node.tangent, lastNormal);
        node.circleIndices.reserve(circlePoints2D.size());
        for (int j = 0; j < circlePoints2D.size(); j++) {
            node.circleIndices.push_back(j + numVertexPts*circlePoints2D.size());
            if (pathLineAttributes.size() > 0) {
                vertexAttributes.push_back(pathLineAttributes.at(i));
            }
        }
        tubeNodes.push_back(node);
        numVertexPts++;
    }


    // Create tube triangles/indices for the vertex data
    for (int i = 0; i < numVertexPts-1; i++) {
        std::vector<uint32_t> &circleIndicesCurrent = tubeNodes.at(i).circleIndices;
        std::vector<uint32_t> &circleIndicesNext = tubeNodes.at(i+1).circleIndices;
        for (int j = 0; j < circlePoints2D.size(); j++) {
            // Build two CCW triangles (one quad) for each side
            // Triangle 1
            indices.push_back(circleIndicesCurrent.at(j));
            indices.push_back(circleIndicesCurrent.at((j+1)%circlePoints2D.size()));
            indices.push_back(circleIndicesNext.at((j+1)%circlePoints2D.size()));

            // Triangle 2
            indices.push_back(circleIndicesCurrent.at(j));
            indices.push_back(circleIndicesNext.at((j+1)%circlePoints2D.size()));
            indices.push_back(circleIndicesNext.at(j));
        }
    }

    // Only one vertex left -> Output nothing (tube consisting only of one point)
    if (numVertexPts <= 1) {
        vertices.clear();
        normals.clear();
        vertexAttributes.clear();
    }
}
void createTubeRenderData(const std::vector<glm::vec3> &pathLineCenters,
                          std::vector<std::vector<float>> &importanceCriteriaLine,
                          std::vector<glm::vec3> &vertices,
                          std::vector<glm::vec3> &normals,
                          std::vector<std::vector<float>> &importanceCriteriaVertex,
                          std::vector<uint32_t> &indices)
{
    int n = (int)pathLineCenters.size();
    int numImportanceCriteria = (int)importanceCriteriaLine.size();
    if (n < 2) {
        sgl::Logfile::get()->writeError("Error in createTube: n < 2");
        return;
    }

    /// Circle points (circle with center of tube node, in plane with normal vector of tube node)
    vertices.reserve(n*circlePoints2D.size());
    normals.reserve(n*circlePoints2D.size());
    importanceCriteriaVertex.resize(numImportanceCriteria);
    for (int i = 0; i < numImportanceCriteria; i++) {
        importanceCriteriaVertex.at(i).reserve(n);
    }
    indices.reserve((n-1)*circlePoints2D.size()*6);

    // List of all line nodes (points with data)
    std::vector<TubeNode> tubeNodes;
    tubeNodes.reserve(n);
    int numVertexPts = 0;

    // First, create a list of tube nodes
    glm::vec3 lastNormal = glm::vec3(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < n; i++) {
        // Normal of line, e.g.
        glm::vec3 tangent;

        if (i == 0) {
            // First node
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = pathLineCenters.at(i) - pathLineCenters.at(i-1);
        } else {
            // Node with two neighbors - use both normals
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
            //normal += pathLineCenters.at(i) - pathLineCenters.at(i-1);
        }

        float lineSegmentLength = glm::length(tangent);

        if (lineSegmentLength < 0.0001f) {
            //normal = glm::vec3(1.0f, 0.0f, 0.0f);
            // In case the two vertices are almost identical, just skip this path line segment
            continue;
        }
        tangent = glm::normalize(tangent);

        TubeNode node;
        node.center = pathLineCenters.at(i);
        node.tangent = tangent;
        insertOrientedCirclePoints(vertices, normals, node.center, node.tangent, lastNormal);
        node.circleIndices.reserve(circlePoints2D.size());
        for (int j = 0; j < circlePoints2D.size(); j++) {
            node.circleIndices.push_back(j + numVertexPts*circlePoints2D.size());
            for (int k = 0; k < numImportanceCriteria; k++) {
                importanceCriteriaVertex.at(k).push_back(importanceCriteriaLine.at(k).at(i));
            }
        }
        tubeNodes.push_back(node);
        numVertexPts++;
    }


    // Create tube triangles/indices for the vertex data
    for (int i = 0; i < numVertexPts-1; i++) {
        std::vector<uint32_t> &circleIndicesCurrent = tubeNodes.at(i).circleIndices;
        std::vector<uint32_t> &circleIndicesNext = tubeNodes.at(i+1).circleIndices;
        for (int j = 0; j < circlePoints2D.size(); j++) {
            // Build two CCW triangles (one quad) for each side
            // Triangle 1
            indices.push_back(circleIndicesCurrent.at(j));
            indices.push_back(circleIndicesCurrent.at((j+1)%circlePoints2D.size()));
            indices.push_back(circleIndicesNext.at((j+1)%circlePoints2D.size()));

            // Triangle 2
            indices.push_back(circleIndicesCurrent.at(j));
            indices.push_back(circleIndicesNext.at((j+1)%circlePoints2D.size()));
            indices.push_back(circleIndicesNext.at(j));
        }
    }

    // Only one vertex left -> Output nothing (tube consisting only of one point)
    if (numVertexPts <= 1) {
        vertices.clear();
        normals.clear();
        importanceCriteriaVertex.clear();
    }
}

template
void createTubeRenderData<uint32_t>(const std::vector<glm::vec3> &pathLineCenters,
                                    const std::vector<uint32_t> &pathLineAttributes,
                                    std::vector<glm::vec3> &vertices,
                                    std::vector<glm::vec3> &normals,
                                    std::vector<uint32_t> &vertexAttributes,
                                    std::vector<uint32_t> &indices);




/**
 * Creates normals for the specified indexed vertex set.
 * NOTE: If a vertex is indexed by more than one triangle, then the average normal is stored per vertex.
 * If you want to have non-smooth normals, then make sure each vertex is only referenced by one face.
 */
void createNormals(const std::vector<glm::vec3> &vertices,
                   const std::vector<uint32_t> &indices,
                   std::vector<glm::vec3> &normals)
{
    // For finding all triangles with a specific index. Maps vertex index -> first triangle index.
    //Logfile::get()->writeInfo(std::string() + "Creating index map for "
    //        + sgl::toString(indices.size()) + " indices...");
    std::multimap<size_t, size_t> indexMap;
    for (size_t j = 0; j < indices.size(); j++) {
        indexMap.insert(std::make_pair(indices.at(j), (j/3)*3));
    }

    //Logfile::get()->writeInfo(std::string() + "Computing normals for "
    //        + sgl::toString(vertices.size()) + " vertices...");
    normals.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++) {
        glm::vec3 normal(0.0f, 0.0f, 0.0f);
        int numTrianglesSharedBy = 0;
        auto triangleRange = indexMap.equal_range(i);
        for (auto it = triangleRange.first; it != triangleRange.second; it++) {
            size_t j = it->second;
            size_t i1 = indices.at(j), i2 = indices.at(j+1), i3 = indices.at(j+2);
            glm::vec3 faceNormal = glm::cross(vertices.at(i1) - vertices.at(i2), vertices.at(i1) - vertices.at(i3));
            faceNormal = glm::normalize(faceNormal);
            normal += faceNormal;
            numTrianglesSharedBy++;
        }
        // Naive code, O(n^2)
        /*for (size_t j = 0; j < indices.size(); j += 3) {
            // Does this triangle contain vertex #i?
            if (indices.at(j) == i || indices.at(j+1) == i || indices.at(j+2) == i) {
                size_t i1 = indices.at(j), i2 = indices.at(j+1), i3 = indices.at(j+2);
                glm::vec3 faceNormal = glm::cross(vertices.at(i1) - vertices.at(i2), vertices.at(i1) - vertices.at(i3));
                faceNormal = glm::normalize(faceNormal);
                normal += faceNormal;
                numTrianglesSharedBy++;
            }
        }*/

        if (numTrianglesSharedBy == 0) {
            Logfile::get()->writeError("Error in createNormals: numTrianglesSharedBy == 0");
            exit(1);
        }
        normal /= (float)numTrianglesSharedBy;
        normals.push_back(normal);
    }
}


void convertObjTrajectoryDataToBinaryTriangleMesh(
        TrajectoryType trajectoryType,
        const std::string &objFilename,
        const std::string &binaryFilename)
{
    std::ifstream file(objFilename.c_str());

    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in convertObjTrajectoryDataToBinaryMesh: File \""
                                        + objFilename + "\" does not exist.");
        return;
    }

    auto start = std::chrono::system_clock::now();

    if (trajectoryType == TRAJECTORY_TYPE_RINGS) {
        initializeCircleData(3, 0.05);
    }
    else if (trajectoryType == TRAJECTORY_TYPE_ANEURISM) {
        initializeCircleData(3, TUBE_RADIUS);
    } else {
        initializeCircleData(3, TUBE_RADIUS);
    }

    BinaryMesh binaryMesh;
    binaryMesh.submeshes.push_back(BinarySubMesh());
    BinarySubMesh &submesh = binaryMesh.submeshes.front();
    submesh.vertexMode = VERTEX_MODE_TRIANGLES;

    std::vector<glm::vec3> globalVertexPositions;
    std::vector<glm::vec3> globalNormals;
    std::vector<std::vector<float>> globalImportanceCriteria;
    std::vector<uint32_t> globalIndices;

    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;

    sgl::AABB3 boundingBox;

    std::vector<glm::vec3> globalLineVertices;
    std::vector<float> globalLineVertexAttributes;

    uint32_t numLines = 0;
    uint32_t numLineSegments = 0;

    std::string lineString;
    while (getline(file, lineString)) {
        while (lineString.size() > 0 && (lineString[lineString.size()-1] == '\r' || lineString[lineString.size()-1] == ' ')) {
            // Remove '\r' of Windows line ending
            lineString = lineString.substr(0, lineString.size() - 1);
        }
        std::vector<std::string> line;
        boost::algorithm::split(line, lineString, boost::is_any_of("\t "), boost::token_compress_on);

        std::string command = line.at(0);

        if (command == "g") {
            // New path
            static int ctr = 0;
            if (ctr >= 999) {
//                Logfile::get()->writeInfo(std::string() + "Parsing trajectory line group " + line.at(1) + "...");
            }
            ctr = (ctr + 1) % 1000;
        } else if (command == "v") {
            // Path line vertex position
            glm::vec3 position;
            if (isConvectionRolls) {
                position = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(3)),
                                                       fromString<float>(line.at(2)));
            } else
            {
                position = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
                                                       fromString<float>(line.at(3)));
            }

            globalLineVertices.push_back(position);
            boundingBox.combine(position);


        } else if (command == "vt") {
            // Path line vertex attribute
            globalLineVertexAttributes.push_back(fromString<float>(line.at(1)));
        } else if (command == "l") {
            // Get indices of current path line
            std::vector<uint32_t> currentLineIndices;
            for (size_t i = 1; i < line.size(); i++) {
                currentLineIndices.push_back(atoi(line.at(i).c_str()) - 1);
            }

            numLines++;
            numLineSegments += currentLineIndices.size() - 1;

            // pathLineCenters: The path line points to create a tube from.
            std::vector<glm::vec3> pathLineCenters;
            std::vector<float> pathLineVorticities;
            pathLineCenters.reserve(currentLineIndices.size());
            pathLineVorticities.reserve(currentLineIndices.size());
            for (size_t i = 0; i < currentLineIndices.size(); i++) {
                pathLineCenters.push_back(globalLineVertices.at(currentLineIndices.at(i)));
                pathLineVorticities.push_back(globalLineVertexAttributes.at(currentLineIndices.at(i)));
            }

            // Compute importance criteria
            std::vector<std::vector<float>> importanceCriteriaLine;
            computeTrajectoryAttributes(trajectoryType, pathLineCenters, pathLineVorticities, importanceCriteriaLine);

            // Line filtering for WCB trajectories
//            if (trajectoryType == TRAJECTORY_TYPE_WCB) {
//                if (importanceCriteriaLine.at(3).size() > 0 && importanceCriteriaLine.at(3).at(0) < 500.0f) {
//                    continue;
//                }
//            }

            // Create tube render data
            std::vector<glm::vec3> localVertices;
            std::vector<std::vector<float>> importanceCriteriaVertex;
            std::vector<glm::vec3> localNormals;
            std::vector<uint32_t> localIndices;
            createTubeRenderData(pathLineCenters, importanceCriteriaLine, localVertices, localNormals,
                                 importanceCriteriaVertex, localIndices);

            // Local -> global
            for (size_t i = 0; i < localIndices.size(); i++) {
                globalIndices.push_back(localIndices.at(i) + globalVertexPositions.size());
            }
            globalVertexPositions.insert(globalVertexPositions.end(), localVertices.begin(), localVertices.end());
            globalNormals.insert(globalNormals.end(), localNormals.begin(), localNormals.end());
            if (globalImportanceCriteria.empty()) {
                globalImportanceCriteria.insert(globalImportanceCriteria.end(), importanceCriteriaVertex.begin(),
                        importanceCriteriaVertex.end());
            } else {
                for (size_t i = 0; i < globalImportanceCriteria.size(); i++) {
                    globalImportanceCriteria.at(i).insert(globalImportanceCriteria.at(i).end(),
                            importanceCriteriaVertex.at(i).begin(), importanceCriteriaVertex.at(i).end());
                }
            }
        } else if (boost::starts_with(command, "#") || command == "") {
            // Ignore comments and empty lines
        } else {
//            Logfile::get()->writeError(std::string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
        }
    }

    // Normalize data for rings
    float minValue = std::min(boundingBox.getMinimum().x, std::min(boundingBox.getMinimum().y, boundingBox.getMinimum().z));
    float maxValue = std::max(boundingBox.getMaximum().x, std::max(boundingBox.getMaximum().y, boundingBox.getMaximum().z));

    if (isConvectionRolls)
    {
        minValue = 0;
        maxValue = 0.5;
    }

    glm::vec3 minVec(minValue, minValue, minValue);
    glm::vec3 maxVec(maxValue, maxValue, maxValue);

    if (isRings || isConvectionRolls)
    {
        for (auto& vertex : globalVertexPositions)
        {
            vertex = (vertex - minVec) / (maxVec - minVec);

            if (isConvectionRolls)
            {
                glm::vec3 dims = glm::vec3(1);
                dims.y = boundingBox.getDimensions().y;
                vertex -= dims;
            }

        }
    }

    submesh.material.diffuseColor = glm::vec3(165, 220, 84) / 255.0f;
    submesh.material.opacity = 120 / 255.0f;
    submesh.indices = globalIndices;

    BinaryMeshAttribute positionAttribute;
    positionAttribute.name = "vertexPosition";
    positionAttribute.attributeFormat = ATTRIB_FLOAT;
    positionAttribute.numComponents = 3;
    positionAttribute.data.resize(globalVertexPositions.size() * sizeof(glm::vec3));
    memcpy(&positionAttribute.data.front(), &globalVertexPositions.front(), globalVertexPositions.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(positionAttribute);

    BinaryMeshAttribute lineNormalsAttribute;
    lineNormalsAttribute.name = "vertexNormal";
    lineNormalsAttribute.attributeFormat = ATTRIB_FLOAT;
    lineNormalsAttribute.numComponents = 3;
    lineNormalsAttribute.data.resize(globalNormals.size() * sizeof(glm::vec3));
    memcpy(&lineNormalsAttribute.data.front(), &globalNormals.front(), globalNormals.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(lineNormalsAttribute);


    std::vector<std::vector<uint16_t>> globalImportanceCriteriaUnorm;
    packUnorm16ArrayOfArrays(globalImportanceCriteria, globalImportanceCriteriaUnorm);

    for (size_t i = 0; i < globalImportanceCriteriaUnorm.size(); i++) {
        std::vector<uint16_t> &currentAttr = globalImportanceCriteriaUnorm.at(i);
        BinaryMeshAttribute vertexAttribute;
        vertexAttribute.name = "vertexAttribute" + sgl::toString(i);
        vertexAttribute.attributeFormat = ATTRIB_UNSIGNED_SHORT;
        vertexAttribute.numComponents = 1;
        vertexAttribute.data.resize(currentAttr.size() * sizeof(uint16_t));
        memcpy(&vertexAttribute.data.front(), &currentAttr.front(), currentAttr.size() * sizeof(uint16_t));
        submesh.attributes.push_back(vertexAttribute);
    }


    file.close();
    auto end = std::chrono::system_clock::now();

    Logfile::get()->writeInfo(std::string() + "Summary: "
                              + sgl::toString(globalVertexPositions.size()) + " vertices, "
                              + sgl::toString(globalIndices.size()) + " indices.");
    Logfile::get()->writeInfo(std::string() + "Writing binary mesh...");
    writeMesh3D(binaryFilename, binaryMesh);

    // compute size of renderable geometry;
    float byteSize = positionAttribute.data.size() * sizeof(uint8_t) + lineNormalsAttribute.data.size() * sizeof(uint8_t)
                                    + submesh.attributes[0].data.size() * sizeof(uint8_t) + submesh.indices.size() * sizeof(uint32_t);

    float MBSize = byteSize / 1024. / 1024.;

    Logfile::get()->writeInfo(std::string() +  "Byte Size Mesh Structure: " + std::to_string(MBSize) + " MB");
    Logfile::get()->writeInfo(std::string() +  "Num Lines: " + std::to_string(numLines / 1000.) + " Tsd.") ;
    Logfile::get()->writeInfo(std::string() +  "Num LineSegments: " + std::to_string(numLineSegments / 1.0E6) + " Mio");

    auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    Logfile::get()->writeInfo(std::string() + "Computational time to create binmesh: "
                              + std::to_string(elapsed.count()));

}




void computeLineNormal(const glm::vec3 &tangent, glm::vec3 &normal, const glm::vec3 &lastNormal)
{
    glm::vec3 helperAxis = lastNormal;
    if (glm::length(glm::cross(helperAxis, tangent)) < 0.01f) {
        // If tangent == helperAxis
        helperAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    normal = glm::normalize(helperAxis - tangent * glm::dot(helperAxis, tangent)); // Gram-Schmidt
    //glm::vec3 binormal = glm::normalize(glm::cross(tangent, normal));
}

/**
 * @param pathLineCenters: The (input) path line points to create a tube from.
 * @param pathLineAttributes: The (input) path line point vertex attributes (belonging to pathLineCenters).
 * @param vertices: The (output) vertex points, which are a set of oriented circles around the centers (see above).
 * @param indices: The (output) indices specifying how tube triangles are built from the circle vertices.
 */
void createTangentAndNormalData(std::vector<glm::vec3> &pathLineCenters,
                                std::vector<std::vector<float>> &importanceCriteriaIn,
                                std::vector<glm::vec3> &vertices,
                                std::vector<std::vector<float>> &importanceCriteriaOut,
                                std::vector<glm::vec3> &tangents,
                                std::vector<glm::vec3> &normals,
                                std::vector<uint32_t> &indices)
{
    int n = (int)pathLineCenters.size();
    int numImportanceCriteria = (int)importanceCriteriaIn.size();
    if (n < 2) {
        sgl::Logfile::get()->writeError("Error in createTube: n < 2");
        return;
    }

    vertices.reserve(n);
    importanceCriteriaOut.resize(numImportanceCriteria);
    for (int i = 0; i < numImportanceCriteria; i++) {
        importanceCriteriaOut.at(i).reserve(n);
    }
    tangents.reserve(n);
    normals.reserve(n);
    indices.reserve(n);

    // First, create a list of tube nodes
    glm::vec3 lastNormal = glm::vec3(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < n; i++) {
        glm::vec3 center = pathLineCenters.at(i);
        glm::vec3 tangent;
        if (i == 0) {
            // First node
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = pathLineCenters.at(i) - pathLineCenters.at(i-1);
        } else {
            // Node with two neighbors - use both normals
            tangent = pathLineCenters.at(i+1) - pathLineCenters.at(i);
            //normal += pathLineCenters.at(i) - pathLineCenters.at(i-1);
        }
        if (glm::length(tangent) < 0.0001f) {
            // In case the two vertices are almost identical, just skip this path line segment
            continue;
        }
        tangent = glm::normalize(tangent);

        glm::vec3 normal;
        computeLineNormal(tangent, normal, lastNormal);
        lastNormal = normal;

        vertices.push_back(pathLineCenters.at(i));
        for (int j = 0; j < numImportanceCriteria; j++) {
            importanceCriteriaOut.at(j).push_back(importanceCriteriaIn.at(j).at(i));
        }
        tangents.push_back(tangent);
        normals.push_back(normal);
    }

    // Create indices
    for (int i = 0; i < (int)vertices.size() - 1; i++) {
        indices.push_back(i);
        indices.push_back(i+1);
    }
}


void convertObjTrajectoryDataToBinaryLineMesh(
        TrajectoryType trajectoryType,
        const std::string &objFilename,
        const std::string &binaryFilename)
{
    std::ifstream file(objFilename.c_str());

    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in convertObjTrajectoryDataToBinaryMesh: File \""
                                        + objFilename + "\" does not exist.");
        return;
    }

    auto start = std::chrono::system_clock::now();

    if (trajectoryType == TRAJECTORY_TYPE_RINGS) {
        initializeCircleData(3, 0.05);
    }
    else if (trajectoryType == TRAJECTORY_TYPE_ANEURISM) {
        initializeCircleData(3, TUBE_RADIUS);
    } else {
        initializeCircleData(3, TUBE_RADIUS);
    }

    BinaryMesh binaryMesh;
    binaryMesh.submeshes.push_back(BinarySubMesh());
    BinarySubMesh &submesh = binaryMesh.submeshes.front();
    submesh.vertexMode = VERTEX_MODE_LINES;

    std::vector<glm::vec3> globalVertexPositions;
    std::vector<glm::vec3> globalNormals;
    std::vector<glm::vec3> globalTangents;
    std::vector<std::vector<float>> globalImportanceCriteria;
    std::vector<uint32_t> globalIndices;

    std::vector<glm::vec3> globalLineVertices;
    std::vector<float> globalLineVertexAttributes;

    sgl::AABB3 boundingBox;
    bool isConvectionRolls = trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
    bool isRings = trajectoryType == TRAJECTORY_TYPE_RINGS;

    std::string lineString;
    while (getline(file, lineString)) {
        while (lineString.size() > 0 && (lineString[lineString.size()-1] == '\r' || lineString[lineString.size()-1] == ' ')) {
            // Remove '\r' of Windows line ending
            lineString = lineString.substr(0, lineString.size() - 1);
        }
        std::vector<std::string> line;
        boost::algorithm::split(line, lineString, boost::is_any_of("\t "), boost::token_compress_on);

        std::string command = line.at(0);

        if (command == "g") {
            // New path
            static int ctr = 0;
//            if (ctr >= 999) {
//                Logfile::get()->writeInfo(std::string() + "Parsing trajectory line group " + line.at(1) + "...");
//            }
            ctr = (ctr + 1) % 1000;
        } else if (command == "v") {
            // Path line vertex position
            glm::vec3 position;
            if (isConvectionRolls) {
                position = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(3)),
                                     fromString<float>(line.at(2)));
            } else
            {
                position = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
                                     fromString<float>(line.at(3)));
            }

            globalLineVertices.push_back(position);
            boundingBox.combine(position);
        } else if (command == "vt") {
            // Path line vertex attribute
            globalLineVertexAttributes.push_back(fromString<float>(line.at(1)));
        } else if (command == "l") {
            // Get indices of current path line
            std::vector<uint32_t> currentLineIndices;
            for (size_t i = 1; i < line.size(); i++) {
                currentLineIndices.push_back(atoi(line.at(i).c_str()) - 1);
            }

            // pathLineCenters: The path line points to create a tube from.
            std::vector<glm::vec3> pathLineCenters;
            std::vector<float> pathLineVorticities;
            pathLineCenters.reserve(currentLineIndices.size());
            pathLineVorticities.reserve(currentLineIndices.size());
            for (size_t i = 0; i < currentLineIndices.size(); i++) {
                pathLineCenters.push_back(globalLineVertices.at(currentLineIndices.at(i)));
                pathLineVorticities.push_back(globalLineVertexAttributes.at(currentLineIndices.at(i)));
            }

            // Compute importance criteria
            std::vector<std::vector<float>> importanceCriteriaIn;
            computeTrajectoryAttributes(trajectoryType, pathLineCenters, pathLineVorticities, importanceCriteriaIn);

            // Line filtering for WCB trajectories
//            if (trajectoryType == TRAJECTORY_TYPE_WCB) {
//                if (importanceCriteriaIn.at(3).size() > 0 && importanceCriteriaIn.at(3).at(0) < 500.0f) {
//                    continue;
//                }
//            }

            // Create tube render data
            std::vector<glm::vec3> localVertices;
            std::vector<glm::vec3> localTangents;
            std::vector<glm::vec3> localNormals;
            std::vector<uint32_t> localIndices;
            std::vector<std::vector<float>> importanceCriteriaOut;
            createTangentAndNormalData(pathLineCenters, importanceCriteriaIn, localVertices,
                    importanceCriteriaOut, localTangents, localNormals, localIndices);

            // Local -> global
            for (size_t i = 0; i < localIndices.size(); i++) {
                globalIndices.push_back(localIndices.at(i) + globalVertexPositions.size());
            }
            globalVertexPositions.insert(globalVertexPositions.end(), localVertices.begin(), localVertices.end());
            globalTangents.insert(globalTangents.end(), localTangents.begin(), localTangents.end());
            globalNormals.insert(globalNormals.end(), localNormals.begin(), localNormals.end());
            if (globalImportanceCriteria.empty()) {
                globalImportanceCriteria.insert(globalImportanceCriteria.end(), importanceCriteriaOut.begin(),
                        importanceCriteriaOut.end());
            } else {
                for (size_t i = 0; i < globalImportanceCriteria.size(); i++) {
                    globalImportanceCriteria.at(i).insert(globalImportanceCriteria.at(i).end(),
                            importanceCriteriaOut.at(i).begin(), importanceCriteriaOut.at(i).end());
                }
            }
        } else if (boost::starts_with(command, "#") || command == "") {
            // Ignore comments and empty lines
        } else {
//            Logfile::get()->writeError(std::string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
        }
    }

    // Normalize data for rings
    float minValue = std::min(boundingBox.getMinimum().x, std::min(boundingBox.getMinimum().y, boundingBox.getMinimum().z));
    float maxValue = std::max(boundingBox.getMaximum().x, std::max(boundingBox.getMaximum().y, boundingBox.getMaximum().z));


    if (isConvectionRolls)
    {
        minValue = 0;
        maxValue = 0.5;
    }

    glm::vec3 minVec(minValue, minValue, minValue);
    glm::vec3 maxVec(maxValue, maxValue, maxValue);

    if (isRings || isConvectionRolls)
    {
        for (auto& vertex : globalVertexPositions)
        {
            vertex = (vertex - minVec) / (maxVec - minVec);

            if (isConvectionRolls)
            {
                glm::vec3 dims = glm::vec3(1);
                dims.y = boundingBox.getDimensions().y;
                vertex -= dims;
            }

        }
    }

    submesh.material.diffuseColor = glm::vec3(165, 220, 84) / 255.0f;
    submesh.material.opacity = 120 / 255.0f;
    submesh.indices = globalIndices;

    BinaryMeshAttribute positionAttribute;
    positionAttribute.name = "vertexPosition";
    positionAttribute.attributeFormat = ATTRIB_FLOAT;
    positionAttribute.numComponents = 3;
    positionAttribute.data.resize(globalVertexPositions.size() * sizeof(glm::vec3));
    memcpy(&positionAttribute.data.front(), &globalVertexPositions.front(), globalVertexPositions.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(positionAttribute);

    BinaryMeshAttribute lineNormalsAttribute;
    lineNormalsAttribute.name = "vertexLineNormal";
    lineNormalsAttribute.attributeFormat = ATTRIB_FLOAT;
    lineNormalsAttribute.numComponents = 3;
    lineNormalsAttribute.data.resize(globalNormals.size() * sizeof(glm::vec3));
    memcpy(&lineNormalsAttribute.data.front(), &globalNormals.front(), globalNormals.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(lineNormalsAttribute);

    BinaryMeshAttribute lineTangentAttribute;
    lineTangentAttribute.name = "vertexLineTangent";
    lineTangentAttribute.attributeFormat = ATTRIB_FLOAT;
    lineTangentAttribute.numComponents = 3;
    lineTangentAttribute.data.resize(globalTangents.size() * sizeof(glm::vec3));
    memcpy(&lineTangentAttribute.data.front(), &globalTangents.front(), globalTangents.size() * sizeof(glm::vec3));
    submesh.attributes.push_back(lineTangentAttribute);


    std::vector<std::vector<uint16_t>> globalImportanceCriteriaUnorm;
    packUnorm16ArrayOfArrays(globalImportanceCriteria, globalImportanceCriteriaUnorm);

    for (size_t i = 0; i < globalImportanceCriteriaUnorm.size(); i++) {
        std::vector<uint16_t> &currentAttr = globalImportanceCriteriaUnorm.at(i);
        BinaryMeshAttribute vertexAttribute;
        vertexAttribute.name = "vertexAttribute" + sgl::toString(i);
        vertexAttribute.attributeFormat = ATTRIB_UNSIGNED_SHORT;
        vertexAttribute.numComponents = 1;
        vertexAttribute.data.resize(currentAttr.size() * sizeof(uint16_t));
        memcpy(&vertexAttribute.data.front(), &currentAttr.front(), currentAttr.size() * sizeof(uint16_t));
        submesh.attributes.push_back(vertexAttribute);
    }

    file.close();

    auto end = std::chrono::system_clock::now();

    Logfile::get()->writeInfo(std::string() + "Summary: "
                              + sgl::toString(globalVertexPositions.size()) + " vertices, "
                              + sgl::toString(globalIndices.size()) + " indices.");
    Logfile::get()->writeInfo(std::string() + "Writing binary mesh...");
    writeMesh3D(binaryFilename, binaryMesh);


    auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    Logfile::get()->writeInfo(std::string() + "Computational time to create binmesh: "
                        + std::to_string(elapsed.count()));
}

