//
// Created by kerninator on 19.03.20.
//
#ifndef PIXELSYNCOIT_BEZIERCURVE_HPP
#define PIXELSYNCOIT_BEZIERCURVE_HPP

#include <array>
#include <glm/glm.hpp>

class BezierCurve
{
public:
    explicit BezierCurve(const std::array<glm::vec3, 4>& points, const float _minT,
                         const float _maxT);

    std::array<glm::vec3, 4> controlPoints;
    float totalArcLength;
    float minT;
    float maxT;

    bool isInterval(const float t) { return (minT <= t && t <= maxT); }
    void evaluate(const float t, glm::vec3& P, glm::vec3& dt) const;
    glm::vec3 derivative(const float t) const;
    glm::vec3 curvature(const float t) const;
    float evalArcLength(const float _minT, const float _maxT, const uint16_t numSteps) const;
    float solveTForArcLength(const float _arcLength) const;
    float normalizeT(const float t) const { return (t - minT) / (maxT - minT); }
private:
    float denormalizeT(const float t) const { return t * (maxT - minT) + minT; }
};


#endif //PIXELSYNCOIT_BEZIERCURVE_HPP
