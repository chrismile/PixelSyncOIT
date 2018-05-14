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
 * Simple .obj file parser. Not very robust, but sufficient for testing purposes.
 * @param shader: The shader to use for the mesh.
 * @return: The loaded mesh stores in a ShaderAttributes object.
 */
sgl::ShaderAttributesPtr parseObjMesh(const char *filename, sgl::ShaderProgramPtr shader = sgl::ShaderProgramPtr());

#endif /* OBJLOADER_HPP_ */
