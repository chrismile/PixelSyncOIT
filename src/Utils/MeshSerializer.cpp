/*
 * MeshSerializer.cpp
 *
 *  Created on: 18.05.2018
 *      Author: christoph
 */

#include <fstream>
#include <glm/glm.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Renderer.hpp>

#include "MeshSerializer.hpp"

using namespace std;
using namespace sgl;

const uint32_t MESH_FORMAT_VERSION = 0;

void writeMesh3D(
		const std::string &filename,
		const std::vector<uint32_t> &indices,
		const std::vector<glm::vec3> &vertices,
		const std::vector<glm::vec2> &texcoords,
		const std::vector<glm::vec3> &normals) {
	std::ofstream file(filename.c_str(), std::ofstream::binary);

	sgl::BinaryWriteStream stream;
	//stream.write((uint32_t)MESH_FORMAT_VERSION);
	stream.writeArray(indices);
	stream.writeArray(vertices);
	stream.writeArray(texcoords);
	stream.writeArray(normals);

	file.write((const char*)stream.getBuffer(), stream.getSize());
	file.close();
}

void readMesh3D(
		const std::string &filename,
		std::vector<uint32_t> &indices,
		std::vector<glm::vec3> &vertices,
		std::vector<glm::vec2> &texcoords,
		std::vector<glm::vec3> &normals) {
	std::ifstream file(filename.c_str(), std::ifstream::binary);

	file.seekg(0, file.end);
	size_t size = file.tellg();
	file.seekg(0);
	char *buffer = new char[size];
	file.read(buffer, size);

	sgl::BinaryReadStream stream(buffer, size);
	stream.readArray(indices);
	stream.readArray(vertices);
	stream.readArray(texcoords);
	stream.readArray(normals);

	//delete[] buffer; // BinaryReadStream does deallocation
	file.close();
}

sgl::ShaderAttributesPtr parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader)
{
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> texcoords;
	std::vector<glm::vec3> normals;

	readMesh3D(filename, indices, vertices, texcoords, normals);

	if (!shader) {
		shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
	}

	ShaderAttributesPtr renderData = ShaderManager->createShaderAttributes(shader);
	if (vertices.size() > 0) {
		GeometryBufferPtr positionBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*vertices.size(), (void*)&vertices.front(), VERTEX_BUFFER);
		renderData->addGeometryBuffer(positionBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
	}
	if (texcoords.size() > 0) {
		GeometryBufferPtr texcoordBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec2)*texcoords.size(), (void*)&texcoords.front(), VERTEX_BUFFER);
		renderData->addGeometryBuffer(texcoordBuffer, "textureCoordinates", ATTRIB_FLOAT, 2);
	}
	if (normals.size() > 0) {
		GeometryBufferPtr normalBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*normals.size(), (void*)&normals.front(), VERTEX_BUFFER);
		renderData->addGeometryBuffer(normalBuffer, "vertexNormal", ATTRIB_FLOAT, 3);
	}
	if (indices.size() > 0) {
		GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t)*indices.size(), (void*)&indices.front(), INDEX_BUFFER);
		renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
	}

	return renderData;
}
