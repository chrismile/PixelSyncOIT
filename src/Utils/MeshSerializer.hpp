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
#include <set>

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

/**
 * Binmesh Format: A mesh consists of one or many submeshes.
 * A submesh consists of:
 *  - A material encoding ambient color, specular exponent etc. for Phong Shading.
 *    This is ignored for e.g. the aneurism dataset, as color and opacity are computed using a transfer function.
 *  - A vertex mode (points, lines, triangles, ...).
 *  - A list of indices (optional, can be empty). 32-bit indices were chosen, as all scientific datasets have more than 2^16 vertices anyways.
 *  - A list of vertex attributes.
 *  - A list of uniform attributes.
 *
 * A vertex attribute could be e.g. the vertex position, normal, color, vorticity, ... and consists of:
 *  - A name (string), which is used as the binding point for the vertex shader.
 *  - The attribute format (e.g. byte, unsigned int, float, ...).
 *  - Die number of components, e.g. 3 for a vector containing three elements or 1 for a scalar value.
 *  - The actual attribute data as an array of bytes. It is expected that the number of vertices is the same for each attribute.
 *    The number of vertices can be explicitly computed by "data.size() / numComponents / dataFormatNumBytes".
 *
 * A uniform attribute is an attribute constant over all vertices.
 */

struct BinaryMeshAttribute
{
    std::string name; // e.g. "vertexPosition"
    sgl::VertexAttributeFormat attributeFormat;
    uint32_t numComponents;
    std::vector<uint8_t> data;
};

struct BinaryMeshUniform
{
    std::string name; // e.g. "maxVorticity"
    sgl::VertexAttributeFormat attributeFormat;
    uint32_t numComponents;
    std::vector<uint8_t> data;
};

struct BinaryLineVariable
{
    std::string name; // .e.g. "temperature"
    sgl::VertexAttributeFormat attributeFormat;
    uint32_t numComponents;
    std::vector<uint8_t> data;
//    uint32_t index; // index of variable to identify variable along curve
//    float minValue; // minimum value
//    float maxValue; // maximum value
    std::vector<uint8_t> minValues;
    std::vector<uint8_t> maxValues;
    std::vector<uint8_t> lineOffsets;
    std::vector<uint8_t> varOffsets;
    std::vector<uint8_t> allMinValues;
    std::vector<uint8_t> allMaxValues;
};

struct BinaryVariableInfo
{
    std::string name;
};

struct BinarySubMesh
{
    ObjMaterial material;
    sgl::VertexMode vertexMode;
    std::vector<uint32_t> indices;
    std::vector<BinaryMeshAttribute> attributes;
    std::vector<BinaryMeshUniform> uniforms;
    std::vector<BinaryLineVariable> variables;
    std::vector<BinaryVariableInfo> varInfos;
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

struct ImportanceCriterionAttribute {
    std::string name;
    std::vector<float> attributes;
    float minAttribute;
    float maxAttribute;
};

// For programmable vertex fetching/pulling
struct SSBOEntry {
    SSBOEntry(int bindingPoint, const std::string &attributeName, sgl::GeometryBufferPtr &attributeBuffer)
        : bindingPoint(bindingPoint), attributeName(attributeName), attributeBuffer(attributeBuffer) {}
    int bindingPoint;
    std::string attributeName;
    sgl::GeometryBufferPtr attributeBuffer;
};

class MeshRenderer
{
public:
    MeshRenderer() : useProgrammableFetch(false) {}
    MeshRenderer(bool useProgrammableFetch) : useProgrammableFetch(useProgrammableFetch) {}

    // attributeIndex: For programmable vertex fetching/pulling. We need to bind the correct line attribute SSBO!
    virtual void render(sgl::ShaderProgramPtr passShader, bool isGBufferPass, int attributeIndex);
    void setNewShader(sgl::ShaderProgramPtr newShader);
    bool isLoaded() { return shaderAttributes.size() > 0; }
    bool hasAttributeWithName(const std::string &name) {
        return shaderAttributeNames.find(name) != shaderAttributeNames.end();
    }

    bool useProgrammableFetch;
    std::vector<sgl::ShaderAttributesPtr> shaderAttributes;
    std::vector<SSBOEntry> ssboEntries; // For programmable vertex fetching/pulling
    std::set<std::string> shaderAttributeNames;
    std::vector<ObjMaterial> materials;
    sgl::AABB3 boundingBox;
    sgl::Sphere boundingSphere;
    std::vector<ImportanceCriterionAttribute> importanceCriterionAttributes;
};


class LineMultiVarRenderer : public MeshRenderer
{
    explicit LineMultiVarRenderer() : MeshRenderer(false) {}

    void render(sgl::ShaderProgramPtr passShader,
            bool isGBufferPass,
            int attributeIndex) override;
};


/**
 * Uses readMesh3D to read the mesh data from a file and assigns the data to a ShaderAttributesPtr object.
 * @param shader: The shader to use for the mesh.
 * @return: The loaded mesh stored in a ShaderAttributes object.
 */
MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader, bool shuffleData = false,
        bool useProgrammableFetch = false, bool programmableFetchUseAoS = true,
        float lineRadius = 0.001f, int instancing = 0);

#endif /* UTILS_MESHSERIALIZER_HPP_ */
