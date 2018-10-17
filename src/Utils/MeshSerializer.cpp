/*
 * MeshSerializer.cpp
 *
 *  Created on: 18.05.2018
 *      Author: christoph
 */

#include <cmath>
#include <fstream>
#include <algorithm>
#include <random>
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

const uint32_t MESH_FORMAT_VERSION = 3u;

void writeMesh3D(const std::string &filename, const BinaryMesh &mesh) {
	std::ofstream file(filename.c_str(), std::ofstream::binary);
	if (!file.is_open()) {
		Logfile::get()->writeError(std::string() + "Error in writeMesh3D: File \"" + filename + "\" not found.");
		return;
	}

	sgl::BinaryWriteStream stream;
	stream.write((uint32_t)MESH_FORMAT_VERSION);
	stream.write((uint32_t)mesh.submeshes.size());

	for (const BinarySubMesh &submesh : mesh.submeshes) {
		stream.write(submesh.material);
		stream.write((uint32_t)submesh.vertexMode);
		stream.writeArray(submesh.indices);
		stream.write((uint32_t)submesh.attributes.size());

		for (const BinaryMeshAttribute &attribute : submesh.attributes) {
			stream.write(attribute.name);
			stream.write((uint32_t)attribute.attributeFormat);
			stream.write((uint32_t)attribute.numComponents);
			stream.writeArray(attribute.data);
		}
	}

	file.write((const char*)stream.getBuffer(), stream.getSize());
	file.close();
}

void readMesh3D(const std::string &filename, BinaryMesh &mesh) {
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
		BinarySubMesh &submesh = mesh.submeshes.at(i);
		stream.read(submesh.material);
		uint32_t vertexMode;
		stream.read(vertexMode);
		submesh.vertexMode = (sgl::VertexMode)vertexMode;
		stream.readArray(mesh.submeshes.at(i).indices);

		uint32_t numAttributes;
		stream.read(numAttributes);
		submesh.attributes.resize(numAttributes);

		for (uint32_t j = 0; j < numAttributes; j++) {
			BinaryMeshAttribute &attribute = submesh.attributes.at(j);
			stream.read(attribute.name);
			uint32_t format;
			stream.read(format);
			attribute.attributeFormat = (sgl::VertexAttributeFormat)format;
			stream.read(attribute.numComponents);
			stream.readArray(attribute.data);
		}
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

std::vector<uint32_t> shuffleIndicesLines(const std::vector<uint32_t> &indices) {
	size_t numSegments = indices.size() / 2;
	std::vector<size_t> shuffleOffsets;
	for (size_t i = 0; i < numSegments; i++) {
		shuffleOffsets.push_back(i);
	}
	auto rng = std::default_random_engine{};
	std::shuffle(std::begin(shuffleOffsets), std::end(shuffleOffsets), rng);

	std::vector<uint32_t> shuffledIndices;
	shuffledIndices.reserve(numSegments*2);
	for (size_t i = 0; i < numSegments; i++) {
	    size_t lineIndex = shuffleOffsets.at(i);
		shuffledIndices.push_back(indices.at(lineIndex*2));
		shuffledIndices.push_back(indices.at(lineIndex*2+1));
	}

	return shuffledIndices;
}

std::vector<uint32_t> shuffleIndicesTriangles(const std::vector<uint32_t> &indices) {
	size_t numSegments = indices.size() / 3;
	std::vector<size_t> shuffleOffsets;
	for (size_t i = 0; i < numSegments; numSegments++) {
		shuffleOffsets.push_back(i);
	}
	auto rng = std::default_random_engine{};
	std::shuffle(std::begin(shuffleOffsets), std::end(shuffleOffsets), rng);

	std::vector<uint32_t> shuffledIndices;
	shuffledIndices.reserve(numSegments*3);
	for (size_t i = 0; i < numSegments; i++) {
		shuffledIndices.push_back(indices.at(i*3));
		shuffledIndices.push_back(indices.at(i*3+1));
		shuffledIndices.push_back(indices.at(i*3+2));
	}

	return shuffledIndices;
}

MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader,
		float *maxVorticity, bool shuffleData)
{
	MeshRenderer meshRenderer;
	BinaryMesh mesh;
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
		BinarySubMesh &submesh = mesh.submeshes.at(i);
		ShaderAttributesPtr renderData = ShaderManager->createShaderAttributes(shader);
		renderData->setVertexMode(submesh.vertexMode);
		if (submesh.indices.size() > 0) {
			if (shuffleData && submesh.vertexMode == VERTEX_MODE_LINES) {
				std::vector<uint32_t> shuffledIndices;
				if (submesh.vertexMode == VERTEX_MODE_LINES) {
					shuffledIndices = shuffleIndicesLines(submesh.indices);
				} else if (submesh.vertexMode == VERTEX_MODE_TRIANGLES) {
					shuffledIndices = shuffleIndicesTriangles(submesh.indices);
				} else {
					Logfile::get()->writeError("ERROR in parseMesh3D: shuffleData and unsupported vertex mode!");
					shuffledIndices = submesh.indices;
				}
				GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(
						sizeof(uint32_t)*shuffledIndices.size(), (void*)&shuffledIndices.front(), INDEX_BUFFER);
				renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
			} else {
				GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(
						sizeof(uint32_t)*submesh.indices.size(), (void*)&submesh.indices.front(), INDEX_BUFFER);
				renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
			}
		}

		for (size_t j = 0; j < submesh.attributes.size(); j++) {
			BinaryMeshAttribute &meshAttribute = submesh.attributes.at(j);
			GeometryBufferPtr attributeBuffer = Renderer->createGeometryBuffer(
					meshAttribute.data.size(), (void*)&meshAttribute.data.front(), VERTEX_BUFFER);
			renderData->addGeometryBuffer(attributeBuffer, meshAttribute.name.c_str(), meshAttribute.attributeFormat,
					meshAttribute.numComponents);

			if (meshAttribute.name == "vertexPosition") {
				std::vector<glm::vec3> vertices;
				vertices.resize(meshAttribute.data.size() / sizeof(glm::vec3));
				memcpy(&vertices.front(), &meshAttribute.data.front(), meshAttribute.data.size());
				totalBoundingBox.combine(computeAABB(vertices));
			}
			if (maxVorticity != NULL && meshAttribute.name == "vorticity") {
				float *vorticities = (float*)&meshAttribute.data.front();
				size_t numVorticityValues = meshAttribute.data.size() / sizeof(float);
				*maxVorticity = 0.0f;
				for (size_t k = 0; k < numVorticityValues; k++) {
					*maxVorticity = std::max(*maxVorticity, vorticities[k]);
				}
			}
		}

		shaderAttributes.push_back(renderData);
		materials.push_back(mesh.submeshes.at(i).material);
	}

	meshRenderer.boundingBox = totalBoundingBox;
	meshRenderer.boundingSphere = sgl::Sphere(totalBoundingBox.getCenter(), glm::length(totalBoundingBox.getExtent()));

	return meshRenderer;
}
