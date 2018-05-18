/*
 * OBJLoader.hpp
 *
 *  Created on: 13.05.2018
 *      Author: Christoph Neuhauser
 */

#ifndef OBJLOADER_HPP_
#define OBJLOADER_HPP_

#include <Graphics/Shader/ShaderAttributes.hpp>

/**
 * Simple .obj file parser converting a mesh to a binary format.
 * Not very robust, but sufficient for testing purposes.
 * Stores the parsed .obj mesh to "binaryFilename" using "writeMesh3D" (see MeshSerializer.hpp).
 *
 * @param objFilename: The input .obj file.
 * @param binaryFilename: The filename of the binary output file output.
 * @return: The loaded mesh stores in a ShaderAttributes object.
 */
void convertObjMeshToBinary(
		const std::string &objFilename,
		const std::string &binaryFilename);

#endif /* OBJLOADER_HPP_ */
