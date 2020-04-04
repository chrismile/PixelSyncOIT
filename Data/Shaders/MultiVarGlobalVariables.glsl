// Number of variables to display
uniform uint numVariables = 6;
// Number of instances for rendering
uniform uint numInstances;
// Radius of tubes
uniform float radius = 0.001f;

// Structs for SSBOs
struct VarData
{
    float value;
};

struct LineDescData
{
    float startIndex;
};

struct VarDescData
{
//    float startIndex;
//    vec2 minMax;
//    float dummy;
    vec4 info;
};

struct LineVarDescData
{
    vec4 minMax;
};

// SSBOs which contain all data for all variables per trajectory
layout (std430, binding = 2) buffer VariableArray
{
    float varArray[];
};

layout (std430, binding = 3) buffer LineDescArray
{
    float lineDescs[];
};

layout (std430, binding = 4) buffer VarDescArray
{
    VarDescData varDescs[];
};

layout (std430, binding = 5) buffer LineVarDescArray
{
    LineVarDescData lineVarDescs[];
};

// Function to sample from SSBOs
void sampleVariableFromLineSSBO(in uint lineID, in uint varID, in uint elementID,
                                out float value, out vec2 minMax)
{
    uint startIndex = uint(lineDescs[lineID]);
    VarDescData varDesc = varDescs[numVariables * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    // Output
    minMax = varDesc.info.gb;
    value = varArray[startIndex + varOffset + elementID];
}

// Function to sample distribution from SSBO
void sampleVariableDistributionFromLineSSBO(in uint lineID, in uint varID, out vec2 minMax)
{
    LineVarDescData lineVarDesc = lineVarDescs[numVariables * lineID + varID];
    minMax = lineVarDesc.minMax.xy;
}