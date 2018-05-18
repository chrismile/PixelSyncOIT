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

#include <Graphics/Shader/ShaderAttributes.hpp>

/**
 * Parsing text-based mesh files, like .obj files, is really slow compared to binary formats.
 * The utility functions below serialize 3D mesh data to a file/read the data back from such a file.
 */

/**
 * Writes a mesh to a binary file. The mesh data vectors may also be empty (i.e. size 0).
 * @param indices, vertices, texcoords, normals: The mesh data.
 */
void writeMesh3D(
		const std::string &filename,
		const std::vector<uint32_t> &indices,
		const std::vector<glm::vec3> &vertices,
		const std::vector<glm::vec2> &texcoords,
		const std::vector<glm::vec3> &normals);

/**
 * Reads a mesh from a binary file. The mesh data vectors may also be empty (i.e. size 0).
 * @param indices, vertices, texcoords, normals: The mesh data.
 */
void readMesh3D(
		const std::string &filename,
		std::vector<uint32_t> &indices,
		std::vector<glm::vec3> &vertices,
		std::vector<glm::vec2> &texcoords,
		std::vector<glm::vec3> &normals);

/**
 * Uses readMesh3D to read the mesh data from a file and assigns the data to a ShaderAttributesPtr object.
 * @param shader: The shader to use for the mesh.
 * @return: The loaded mesh stored in a ShaderAttributes object.
 */
sgl::ShaderAttributesPtr parseMesh3D(const std::string &filename,
		sgl::ShaderProgramPtr shader = sgl::ShaderProgramPtr());

#endif /* UTILS_MESHSERIALIZER_HPP_ */
