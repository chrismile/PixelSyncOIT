#ifndef TRANSFER_FUNCTION_GLSL
#define TRANSFER_FUNCTION_GLSL

// Transfer function color lookup table
uniform sampler1D transferFunctionTexture;

vec4 transferFunction(uint attr)
{
    // Transfer to range [0,1]
    float posFloat = clamp(float(attr) / 255.0, 0.0, 1.0);
    // Look up the color value
    return texture(transferFunctionTexture, posFloat);
}

#endif