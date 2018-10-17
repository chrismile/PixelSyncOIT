/*
 * MeshSerializer.hpp
 *
 *  Created on: 18.05.2018
 *      Author: christoph
 */

#ifndef UTILS_MESHSERIALIZER_HPP_
#define UTILS_MESHSERIALIZER_HPP_

#include <glm/glm.hpp>
#include <vector>

#include <Math/Geometry/AABB3.hpp>
#include <Math/Geometry/Sphere.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>

/**
 * Parsing text-based mesh files, like .obj files, is really slow compared to binary formats.
 * The utility functions below serialize 3D mesh data to a file/read the data back from such a file.
 */

 /**
  * A simple material definition. For now, no textures are supported.
  */
struct ObjMaterial
{
	ObjMaterial() : ambientColor(0.75f, 0.75f, 0.75f), diffuseColor(0.75f, 0.75f, 0.75f),
					specularColor(0.0f, 0.0f, 0.0f), specularExponent(1.0f), opacity(1.0f) {}

	glm::vec3 ambientColor;
	glm::vec3 diffuseColor;
	glm::vec3 specularColor;
	float specularExponent;
	float opacity;
};

struct ObjSubmesh
{
	ObjMaterial material;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> texcoords;
	std::vector<glm::vec3> normals;
};

struct ObjMesh
{
	std::vector<ObjSubmesh> submeshes;
};

struct BinaryMeshAttribute
{
    std::string name; // e.g. "vertexPosition"
    sgl::VertexAttributeFormat attributeFormat;
    uint32_t numComponents;
    std::vector<uint8_t> data;
};

struct BinarySubMesh
{
    ObjMaterial material;
    sgl::VertexMode vertexMode;
    std::vector<uint32_t> indices;
    std::vector<BinaryMeshAttribute> attributes;
};

struct BinaryMesh
{
    std::vector<BinarySubMesh> submeshes;
};

/**
 * Writes a mesh to a binary file. The mesh data vectors may also be empty (i.e. size 0).
 * @param indices, vertices, texcoords, normals: The mesh data.
 */
void writeMesh3D(const std::string &filename, const BinaryMesh &mesh);

/**
 * Reads a mesh from a binary file. The mesh data vectors may also be empty (i.e. size 0).
 * @param indices, vertices, texcoords, normals: The mesh data.
 */
void readMesh3D(const std::string &filename, BinaryMesh &mesh);


class MeshRenderer
{
public:
	void render(sgl::ShaderProgramPtr passShader, bool isGBufferPass = false);
	void setNewShader(sgl::ShaderProgramPtr newShader);
	bool isLoaded() { return shaderAttributes.size() > 0; }

	std::vector<sgl::ShaderAttributesPtr> shaderAttributes;
	std::vector<ObjMaterial> materials;
	sgl::AABB3 boundingBox;
	sgl::Sphere boundingSphere;
};


/**
 * Uses readMesh3D to read the mesh data from a file and assigns the data to a ShaderAttributesPtr object.
 * @param shader: The shader to use for the mesh.
 * @return: The loaded mesh stored in a ShaderAttributes object.
 */
MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader = sgl::ShaderProgramPtr(),
		float *maxVorticity = NULL, bool shuffleData = false);

#endif /* UTILS_MESHSERIALIZER_HPP_ */
