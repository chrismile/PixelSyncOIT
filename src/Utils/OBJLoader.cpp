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

ShaderAttributesPtr parseObjMesh(const char *filename, ShaderProgramPtr shader)
{
	std::ifstream file(filename);

	if (!file.is_open()) {
		Logfile::get()->writeError(string() + "Error in parseObjMesh: File \"" + filename + "\" does not exist.");
		return ShaderAttributesPtr();
	}

	std::vector<glm::vec3> tempVertices;
	std::vector<glm::vec2> tempTexcoords;
	std::vector<glm::vec3> tempNormals;
	std::vector<uint32_t> vertexIndices;
	std::vector<uint32_t> tecoordIndices;
	std::vector<uint32_t> normalIndices;

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
			normals.push_back(normal);
		}
	}

	if (!shader) {
		shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
	}
	ShaderAttributesPtr renderData = ShaderManager->createShaderAttributes(shader);
	//GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t)*indices.size(), (void*)&indices.front(), INDEX_BUFFER);
	GeometryBufferPtr positionBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*vertices.size(), (void*)&vertices.front(), VERTEX_BUFFER);
	GeometryBufferPtr normalBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*normals.size(), (void*)&normals.front(), VERTEX_BUFFER);
	renderData->addGeometryBuffer(positionBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
	renderData->addGeometryBuffer(normalBuffer, "vertexNormal", ATTRIB_FLOAT, 3);
	//renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);

	return renderData;
}
