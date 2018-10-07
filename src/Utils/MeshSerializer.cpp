/*
 * MeshSerializer.cpp
 *
 *  Created on: 18.05.2018
 *      Author: christoph
 */

#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Renderer.hpp>

#include "MeshSerializer.hpp"

using namespace std;
using namespace sgl;

const uint32_t MESH_FORMAT_VERSION = 2u;

void writeMesh3D(const std::string &filename, const ObjMesh &mesh) {
	std::ofstream file(filename.c_str(), std::ofstream::binary);
	if (!file.is_open()) {
		Logfile::get()->writeError(std::string() + "Error in writeMesh3D: File \"" + filename + "\" not found.");
		return;
	}

	sgl::BinaryWriteStream stream;
	stream.write((uint32_t)MESH_FORMAT_VERSION);
	stream.write((uint32_t)mesh.submeshes.size());

	for (const ObjSubmesh &submesh : mesh.submeshes) {
        stream.write(submesh.material);
        stream.writeArray(submesh.indices);
		stream.writeArray(submesh.vertices);
		stream.writeArray(submesh.texcoords);
        stream.writeArray(submesh.normals);
	}


	file.write((const char*)stream.getBuffer(), stream.getSize());
	file.close();
}

void readMesh3D(const std::string &filename, ObjMesh &mesh) {
	std::ifstream file(filename.c_str(), std::ifstream::binary);
	if (!file.is_open()) {
		Logfile::get()->writeError(std::string() + "Error in readMesh3D: File \"" + filename + "\" not found.");
		return;
	}

	file.seekg(0, file.end);
	size_t size = file.tellg();
	file.seekg(0);
	char *buffer = new char[size];
	file.read(buffer, size);

	sgl::BinaryReadStream stream(buffer, size);
	uint32_t version;
	stream.read(version);
	if (version != MESH_FORMAT_VERSION) {
		Logfile::get()->writeError(std::string() + "Error in readMesh3D: Invalid version in file \""
				+ filename + "\".");
		return;
	}

	uint32_t numSubmeshes;
	stream.read(numSubmeshes);
	mesh.submeshes.resize(numSubmeshes);

	for (uint32_t i = 0; i < numSubmeshes; i++) {
        stream.read(mesh.submeshes.at(i).material);
        stream.readArray(mesh.submeshes.at(i).indices);
		stream.readArray(mesh.submeshes.at(i).vertices);
		stream.readArray(mesh.submeshes.at(i).texcoords);
		stream.readArray(mesh.submeshes.at(i).normals);
	}

	//delete[] buffer; // BinaryReadStream does deallocation
	file.close();
}





void MeshRenderer::render(sgl::ShaderProgramPtr passShader, bool isGBufferPass)
{
	for (size_t i = 0; i < shaderAttributes.size(); i++) {
		//ShaderProgram *shader = shaderAttributes.at(i)->getShaderProgram();
		if (!boost::starts_with(passShader->getShaderList().front()->getFileID(), "PseudoPhongVorticity")
				&& !boost::starts_with(passShader->getShaderList().front()->getFileID(), "DepthPeelingGatherDepthComplexity")
				&& !isGBufferPass) {
			passShader->setUniform("ambientColor", materials.at(i).ambientColor);
			passShader->setUniform("diffuseColor", materials.at(i).diffuseColor);
			passShader->setUniform("specularColor", materials.at(i).specularColor);
			passShader->setUniform("specularExponent", materials.at(i).specularExponent);
			passShader->setUniform("opacity", materials.at(i).opacity);
		}
		Renderer->render(shaderAttributes.at(i), passShader);
	}
}

void MeshRenderer::setNewShader(sgl::ShaderProgramPtr newShader)
{
	for (size_t i = 0; i < shaderAttributes.size(); i++) {
		shaderAttributes.at(i) = shaderAttributes.at(i)->copy(newShader);
	}
}


sgl::AABB3 computeAABB(const std::vector<glm::vec3> &vertices)
{
	if (vertices.size() < 1) {
		Logfile::get()->writeError("computeAABB: vertices.size() < 1");
		return sgl::AABB3();
	}

	glm::vec3 minV = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	glm::vec3 maxV = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (const glm::vec3 &pt : vertices) {
		minV.x = std::min(minV.x, pt.x);
		minV.y = std::min(minV.y, pt.y);
		minV.z = std::min(minV.z, pt.z);
		maxV.x = std::max(maxV.x, pt.x);
		maxV.y = std::max(maxV.y, pt.y);
		maxV.z = std::max(maxV.z, pt.z);
	}

	return sgl::AABB3(minV, maxV);
}

MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader, float *maxVorticity)
{
	//std::vector<uint32_t> indices;
	//std::vector<glm::vec3> vertices;
	//std::vector<glm::vec2> texcoords;
	//std::vector<glm::vec3> normals;

	MeshRenderer meshRenderer;
	ObjMesh mesh;
	readMesh3D(filename, mesh);

	if (!shader) {
		shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
	}

	std::vector<sgl::ShaderAttributesPtr> &shaderAttributes = meshRenderer.shaderAttributes;
	std::vector<ObjMaterial> &materials = meshRenderer.materials;
	shaderAttributes.reserve(mesh.submeshes.size());
	materials.reserve(mesh.submeshes.size());

	// Bounding box of all submeshes combined
	AABB3 totalBoundingBox(glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX), glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX));

	for (size_t i = 0; i < mesh.submeshes.size(); i++) {
		std::vector<uint32_t> &indices = mesh.submeshes.at(i).indices;
		std::vector<glm::vec3> &vertices = mesh.submeshes.at(i).vertices;
		std::vector<glm::vec2> &texcoords = mesh.submeshes.at(i).texcoords;
		std::vector<glm::vec3> &normals = mesh.submeshes.at(i).normals;
		totalBoundingBox.combine(computeAABB(vertices));

		ShaderAttributesPtr renderData = ShaderManager->createShaderAttributes(shader);
		if (vertices.size() > 0) {
			GeometryBufferPtr positionBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*vertices.size(),
					(void*)&vertices.front(), VERTEX_BUFFER);
			renderData->addGeometryBuffer(positionBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
		}
		if (texcoords.size() > 0) {
			GeometryBufferPtr texcoordBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec2)*texcoords.size(),
					(void*)&texcoords.front(), VERTEX_BUFFER);
			renderData->addGeometryBuffer(texcoordBuffer, "textureCoordinates", ATTRIB_FLOAT, 2);

			if (maxVorticity != NULL) {
				*maxVorticity = 0.0f;
				for (size_t i = 0; i < texcoords.size(); i++) {
					*maxVorticity = std::max(*maxVorticity, texcoords.at(i).x);
				}
			}
		}
		if (normals.size() > 0) {
			GeometryBufferPtr normalBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*normals.size(),
					(void*)&normals.front(), VERTEX_BUFFER);
			renderData->addGeometryBuffer(normalBuffer, "vertexNormal", ATTRIB_FLOAT, 3);
		}
		if (indices.size() > 0) {
			GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t)*indices.size(),
					(void*)&indices.front(), INDEX_BUFFER);
			renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
		}

		shaderAttributes.push_back(renderData);
		materials.push_back(mesh.submeshes.at(i).material);
	}

	meshRenderer.boundingBox = totalBoundingBox;
	meshRenderer.boundingSphere = sgl::Sphere(totalBoundingBox.getCenter(), glm::length(totalBoundingBox.getExtent()));

	return meshRenderer;
}
