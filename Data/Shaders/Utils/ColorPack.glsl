uint packColor(vec4 vecColor)
{
    uint packedColor;
    packedColor = uint(vecColor.r * 255.0) & 0xFFu;
    packedColor |= (uint(vecColor.g * 255.0) & 0xFFu) << 8;
    packedColor |= (uint(vecColor.b * 255.0) & 0xFFu) << 16;
    packedColor |= (uint(vecColor.a * 255.0) & 0xFFu) << 24;
    return packedColor;
}

vec4 unpackColor(uint packedColor)
{
    vec4 vecColor;
    vecColor.r = float(packedColor & 0xFFu) / 255.0;
    vecColor.g = float((packedColor >> 8)  & 0xFFu) / 255.0;
    vecColor.b = float((packedColor >> 16) & 0xFFu) / 255.0;
    vecColor.a = float((packedColor >> 24) & 0xFFu) / 255.0;
    return vecColor;
}
