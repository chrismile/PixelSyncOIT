#ifndef PIXELSYNCOIT_HELPER_HPP
#define PIXELSYNCOIT_HELPER_HPP

template<typename T>
T lerp(const T& a, const T& b, const float r)
{
    return (a * (1.0 - r) + b * r);
}

template<>
ospcommon::vec4f lerp<ospcommon::vec4f>(const ospcommon::vec4f& a, const ospcommon::vec4f& b, const float r)
{
    return ospcommon::vec4f{
        lerp<float>(a.x, b.x, r),
        lerp<float>(a.y, b.y, r),
        lerp<float>(a.z, b.z, r),
        lerp<float>(a.w, b.w, r),
    };
}

#endif