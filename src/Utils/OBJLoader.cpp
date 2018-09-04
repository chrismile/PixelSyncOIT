/*
 * OBJLoader.cpp
 *
 *  Created on: 13.05.2018
 *      Author: Christoph Neuhauser
 */

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <glm/glm.hpp>

#include <Utils/File/Logfile.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Utils/Convert.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Renderer.hpp>

#include "MeshSerializer.hpp"
#include "KDTree.hpp"

using namespace std;
using namespace sgl;

// Converts a triangle strip to normal triangles (see OpenGL vertex modes for more information)
void triangleStripToTriangles(vector<uint32_t> tristrip, vector<uint32_t> &triangles) {
	if (tristrip.size() < 3) {
		return;
	}

	for (size_t i = 3; i < tristrip.size(); i++) {
		triangles.push_back(tristrip.at(i));
	}
	for (size_t i = 3; i < tristrip.size(); i++) {
		if (i % 2 == 0) {
			triangles.push_back(tristrip[i-2]);
			triangles.push_back(tristrip[i-1]);
			triangles.push_back(tristrip[i]);
		} else {
			triangles.push_back(tristrip[i]);
			triangles.push_back(tristrip[i-1]);
			triangles.push_back(tristrip[i-2]);
		}
	}
}

// Converts a triangle fan to normal triangles (see OpenGL vertex modes for more information)
void triangleFanToTriangles(vector<uint32_t> trifan, vector<uint32_t> &triangles) {
	if (trifan.size() < 3) {
		return;
	}

	//std::reverse(triangles.begin(), triangles.end());

	for (size_t i = 3; i < trifan.size(); i++) {
		triangles.push_back(trifan.at(i));
	}
	for (size_t i = 3; i < trifan.size(); i++) {
		triangles.push_back(trifan[0]);
		triangles.push_back(trifan[i-1]);
		triangles.push_back(trifan[i]);
	}
}

struct TempSubmesh
{
	TempSubmesh() : smooth(false) {}

	ObjMaterial material;
	bool smooth;

	std::vector<uint32_t> vertexIndices;
	std::vector<uint32_t> tecoordIndices;
	std::vector<uint32_t> normalIndices;
};

void globalIndicesToLocal(std::vector<uint32_t> &globalIndexBuffer, std::vector<glm::vec3> &globalVertexBuffer,
                          std::vector<uint32_t> &localIndexBuffer, std::vector<glm::vec3> &localVertexBuffer)
{
    // Create global indices <-> local indices mapping
    std::vector<uint32_t> sortedIndexBuffer = globalIndexBuffer;
    std::sort(sortedIndexBuffer.begin(), sortedIndexBuffer.end());
    std::map<uint32_t, uint32_t> mappingGlobalToLocal;
    std::map<uint32_t, uint32_t> mappingLocalToGlobal;
    size_t index = 0;
    for (size_t i = 0; i < globalIndexBuffer.size(); i++) {
        if (i == 0 || sortedIndexBuffer.at(i) != sortedIndexBuffer.at(i-1)) {
            mappingGlobalToLocal.insert(make_pair(sortedIndexBuffer.at(i), index));
            mappingLocalToGlobal.insert(make_pair(index, sortedIndexBuffer.at(i)));
            index++;
        }
    }

    localVertexBuffer.resize(index);
    for (size_t i = 0; i < index; i++) {
        localVertexBuffer.at(i) = globalVertexBuffer.at(mappingLocalToGlobal[i]);
    }
    localIndexBuffer.resize(globalIndexBuffer.size());
    for (size_t i = 0; i < globalIndexBuffer.size(); i++) {
        localIndexBuffer.at(i) = mappingGlobalToLocal[globalIndexBuffer.at(i)];
    }
}

void processTempSubmesh(ObjMesh &mesh, TempSubmesh &tempSubmesh, std::vector<glm::vec3> &globalVertices,
        std::vector<glm::vec2> &globalTexcoords, std::vector<glm::vec3> &globalNormals)
{
    // Input
    bool smooth = tempSubmesh.smooth;
    std::vector<uint32_t> &vertexIndices = tempSubmesh.vertexIndices;
    std::vector<uint32_t> &tecoordIndices = tempSubmesh.tecoordIndices;
    std::vector<uint32_t> &normalIndices = tempSubmesh.normalIndices;

    // Output
    ObjSubmesh submesh;
    std::vector<uint32_t> &indices = submesh.indices;
    std::vector<glm::vec3> &vertices = submesh.vertices;
    std::vector<glm::vec2> &texcoords = submesh.texcoords;
    std::vector<glm::vec3> &normals = submesh.normals;
    submesh.material = tempSubmesh.material;


    if (!smooth) {
        for (size_t i = 0; i < vertexIndices.size(); i++) {
            vertices.push_back(globalVertices.at(vertexIndices.at(i))); // TODO: -1
            //if (tecoordIndices.size() > 0)
            //	texcoords.push_back(globalTexcoords.at(tecoordIndices.at(i))); // TODO: -1
            if (normalIndices.size() > 0)
                normals.push_back(globalNormals.at(normalIndices.at(i))); // TODO: -1
        }

        if (normalIndices.size() == 0) {
            // Compute manually
            for (size_t i = 0; i < vertices.size(); i += 3) {
                glm::vec3 normal = glm::cross(vertices.at(i) - vertices.at(i+1), vertices.at(i) - vertices.at(i+2));
                normal = glm::normalize(normal);
                normals.push_back(normal);
                normals.push_back(normal);
                normals.push_back(normal);
            }
        }
    } else {
        std::vector<uint32_t> &globalIndices = vertexIndices;
        globalIndicesToLocal(globalIndices, globalVertices, indices, vertices);


        // For finding all triangles with a specific index. Maps vertex index -> first triangle index.
        std::multimap<size_t, size_t> indexMap;
        for (size_t j = 0; j < indices.size(); j++) {
            indexMap.insert(make_pair(indices.at(j), (j/3)*3));
        }

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
                Logfile::get()->writeError("Error in parseObjMesh: numTrianglesSharedBy == 0");
                exit(1); // TODO: Global vertex buffer
            }
            normal /= (float)numTrianglesSharedBy;
            normals.push_back(normal);
        }
    }

    mesh.submeshes.push_back(submesh);
}

void addMaterialsFromFile(const std::string &filename, const std::string &objFilename,
        std::map<std::string, ObjMaterial> &materials)
{
    std::string absFilename = filename;
    if (!FileUtils::get()->exists(absFilename)) {
        absFilename = FileUtils::get()->getPathToFile(objFilename) + FileUtils::get()->getPureFilename(absFilename);
    }

    std::ifstream file(absFilename.c_str());

    if (!file.is_open()) {
        Logfile::get()->writeError(string() + "Error in parseObjMesh: File \"" + filename + "\" does not exist.");
        return;
    }

    std::string materialName;
    ObjMaterial currMaterial;


    std::string lineString;
    while (getline(file, lineString)) {
        while (lineString.size() > 0 && (lineString[lineString.size()-1] == '\r' || lineString[lineString.size()-1] == ' ')) {
            // Remove '\r' of Windows line ending
            lineString = lineString.substr(0, lineString.size() - 1);
        }
        std::vector<std::string> line;
        boost::algorithm::split(line, lineString, boost::is_any_of("\t "), boost::token_compress_on);

        std::string command = line.at(0);

        if (command == "newmtl") {
            // New object
            if (materialName.length() != 0) {
                materials.insert(make_pair(materialName, currMaterial));
            }
            materialName = line.at(1);
            currMaterial = ObjMaterial();
        } else if (command == "Ka") {
            // Ambient color
            currMaterial.ambientColor = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
                                                  fromString<float>(line.at(3)));
        }  else if (command == "Kd") {
            // Diffuse color
            currMaterial.diffuseColor = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
                                                  fromString<float>(line.at(3)));
        } else if (command == "Ks") {
            // Specular color
            currMaterial.specularColor = glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
                                                   fromString<float>(line.at(3)));
        } else if (command == "Ns") {
            // Specular exponent
            currMaterial.specularExponent = fromString<float>(line.at(1));
        } else if (command == "d") {
            // Opacity
            currMaterial.opacity = fromString<float>(line.at(1));
        } else if (command == "tr") {
            // Transparency (= 1 - opacity)
            currMaterial.opacity = 1.0f - fromString<float>(line.at(1));
        } else if (command == "illum") {
            // TODO
        } else if (boost::starts_with(command, "#") || command == "") {
            // Ignore comments and empty lines
        } else {
            //Logfile::get()->writeError(string() + "Error in addMaterialsFromFile: Unknown command \"" + command + "\".");
        }
    }

    if (materialName.length() != 0) {
        materials.insert(make_pair(materialName, currMaterial));
    }

    file.close();
}

void convertObjMeshToBinary(
		const std::string &objFilename,
		const std::string &binaryFilename)
{
	std::ifstream file(objFilename.c_str());

	if (!file.is_open()) {
		Logfile::get()->writeError(string() + "Error in parseObjMesh: File \"" + objFilename + "\" does not exist.");
		return;
	}

	vector<TempSubmesh> tempMesh;
	TempSubmesh currSubmesh;
	uint32_t indexOffset = 1;
	std::map<std::string, ObjMaterial> materials;

    std::vector<glm::vec3> globalVertices;
    std::vector<glm::vec2> globalTexcoords;
    std::vector<glm::vec3> globalNormals;

	std::string lineString;
	while (getline(file, lineString)) {
		while (lineString.size() > 0 && (lineString[lineString.size()-1] == '\r' || lineString[lineString.size()-1] == ' ')) {
			// Remove '\r' of Windows line ending
			lineString = lineString.substr(0, lineString.size() - 1);
		}
		std::vector<std::string> line;
		boost::algorithm::split(line, lineString, boost::is_any_of("\t "), boost::token_compress_on);

		std::string command = line.at(0);

		if (command == "o") {
			// New object
			if (globalVertices.size() != 0) {
				tempMesh.push_back(currSubmesh);
				currSubmesh = TempSubmesh();
			}
		} else if (command == "g") {
			// Groups not supported for now
		}  else if (command == "mtllib") {
            //  Load material definition file
            addMaterialsFromFile(line.at(1), objFilename, materials);
		} else if (command == "usemtl") {
            // Use new material
            auto it = materials.find(line.at(1));
            if (it != materials.end()) {
                currSubmesh.material = it->second;
            } else if (materials.size() > 0) {
                Logfile::get()->writeError(string() + "Error in parseObjMesh: Material \""
                        + line.at(1) + "\" does not exist.");
            }
		} else if (command == "s") {
			// Smooth shading is always assumed for now
			currSubmesh.smooth = true; // TODO
		} else if (command == "v") {
			// Vertex position
            globalVertices.push_back(glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
			        fromString<float>(line.at(3))));
		} else if (command == "vt") {
			// Texture coordinate
            globalTexcoords.push_back(glm::vec2(fromString<float>(line.at(1)),
			        fromString<float>(line.at(2))));
		} else if (command == "vn") {
			// Vertex normal
            globalNormals.push_back(glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)),
			        fromString<float>(line.at(3))));
		} else if (command == "f") {
			// Face indices
			std::vector<uint32_t> tempVertexIndices;
			std::vector<uint32_t> tempTecoordIndices;
			std::vector<uint32_t> tempNormalIndices;
			for (size_t i = 1; i < line.size(); i++) {
				std::vector<std::string> indices;
				boost::algorithm::split(indices, line.at(i), boost::is_any_of("/"));
				currSubmesh.vertexIndices.push_back(atoi(indices.at(0).c_str()) - indexOffset);
				if (indices.size() > 1)
					currSubmesh.tecoordIndices.push_back(atoi(indices.at(1).c_str()) - indexOffset);
				if (indices.size() > 2)
					currSubmesh.normalIndices.push_back(atoi(indices.at(2).c_str()) - indexOffset);
			}
			triangleStripToTriangles(tempVertexIndices, currSubmesh.vertexIndices);
			triangleStripToTriangles(tempTecoordIndices, currSubmesh.tecoordIndices);
			triangleStripToTriangles(tempNormalIndices, currSubmesh.normalIndices);
		} else if (boost::starts_with(command, "#") || command == "") {
			// Ignore comments and empty lines
		} else {
			Logfile::get()->writeError(string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
		}
	}

	if (globalVertices.size() != 0) {
		tempMesh.push_back(currSubmesh);
	}



	ObjMesh mesh;
	mesh.submeshes.reserve(tempMesh.size());
	for (TempSubmesh tempSubmesh : tempMesh) {
		processTempSubmesh(mesh, tempSubmesh, globalVertices, globalTexcoords, globalNormals);
	}

	file.close();

	writeMesh3D(binaryFilename, mesh);
}


