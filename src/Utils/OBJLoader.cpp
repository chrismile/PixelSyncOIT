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

void convertObjMeshToBinary(
		const std::string &objFilename,
		const std::string &binaryFilename)
{
	std::ifstream file(objFilename.c_str());

	if (!file.is_open()) {
		Logfile::get()->writeError(string() + "Error in parseObjMesh: File \"" + objFilename + "\" does not exist.");
		return;
	}

	std::vector<glm::vec3> tempVertices;
	std::vector<glm::vec2> tempTexcoords;
	std::vector<glm::vec3> tempNormals;
	std::vector<uint32_t> vertexIndices;
	std::vector<uint32_t> tecoordIndices;
	std::vector<uint32_t> normalIndices;

	bool smooth = false;
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
		} else if (command == "g") {
			// Groups not supported for now
		}  else if (command == "mtllib") {
			// Materials not supported for now
		} else if (command == "usemtl") {
			//  Materials not supported for now
		} else if (command == "s") {
			// Smooth shading is always assumed for now
			smooth = true; // TODO
		} else if (command == "v") {
			// Vertex position
			tempVertices.push_back(glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)), fromString<float>(line.at(3))));
		} else if (command == "vt") {
			// Texture coordinate
			tempTexcoords.push_back(glm::vec2(fromString<float>(line.at(1)), fromString<float>(line.at(2))));
		} else if (command == "vn") {
			// Vertex normal
			tempNormals.push_back(glm::vec3(fromString<float>(line.at(1)), fromString<float>(line.at(2)), fromString<float>(line.at(3))));
		} else if (command == "f") {
			// Face indices
			std::vector<uint32_t> tempVertexIndices;
			std::vector<uint32_t> tempTecoordIndices;
			std::vector<uint32_t> tempNormalIndices;
			for (size_t i = 1; i < line.size(); i++) {
				std::vector<std::string> indices;
				boost::algorithm::split(indices, line.at(i), boost::is_any_of("/"));
				vertexIndices.push_back(atof(indices.at(0).c_str()));
				if (indices.size() > 1)
					tecoordIndices.push_back(atof(indices.at(1).c_str()));
				if (indices.size() > 2)
					normalIndices.push_back(atof(indices.at(2).c_str()));
			}
			triangleStripToTriangles(tempVertexIndices, vertexIndices);
			triangleStripToTriangles(tempTecoordIndices, tecoordIndices);
			triangleStripToTriangles(tempNormalIndices, normalIndices);
		} else if (boost::starts_with(command, "#") || command == "") {
			// Ignore comments and empty lines
		} else {
			Logfile::get()->writeError(string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
		}
	}


	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> texcoords;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;


	if (!smooth) {
		for (size_t i = 0; i < vertexIndices.size(); i++) {
			vertices.push_back(tempVertices.at(vertexIndices.at(i)-1));
			//if (tecoordIndices.size() > 0)
			//	texcoords.push_back(tempTexcoords.at(tecoordIndices.at(i)-1));
			if (normalIndices.size() > 0)
				normals.push_back(tempNormals.at(normalIndices.at(i)-1));
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
		for (size_t i = 0; i < vertexIndices.size(); i++) {
			indices.push_back(vertexIndices.at(i)-1);
		}

		// For finding all triangles with a specific index. Maps vertex index -> first triangle index.
		std::multimap<size_t, size_t> indexMap;
		for (size_t j = 0; j < indices.size(); j++) {
			indexMap.insert(make_pair(indices.at(j), (j%3)*3));
		}

		vertices = tempVertices;
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
			}
			normals.push_back(normal / (float)numTrianglesSharedBy);
		}
	}

	writeMesh3D(binaryFilename, indices, vertices, texcoords, normals);
}
