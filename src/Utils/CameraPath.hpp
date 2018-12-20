//
// Created by christoph on 18.12.18.
//

#ifndef PIXELSYNCOIT_CAMERAPATH_HPP
#define PIXELSYNCOIT_CAMERAPATH_HPP

#include <vector>
#include <glm/gtc/quaternion.hpp>

#include <Math/Geometry/MatrixUtil.hpp>

class ControlPoint
{
public:
    ControlPoint(float time, float tx, float ty, float tz, float yaw, float pitch);

    float time;
    glm::vec3 position;
    glm::quat orientation;
};

class CameraPath
{
public:
    CameraPath(const std::vector<ControlPoint> &controlPoints);
    void update(float dt);
    inline const glm::mat4x4 &getViewMatrix() const { return currentTransform; }

private:
    glm::mat4x4 toTransform(const glm::vec3 &position, const glm::quat &orientation);
    glm::mat4x4 currentTransform;
    std::vector<ControlPoint> controlPoints;
    float time = 0.0f;
};

#endif //PIXELSYNCOIT_CAMERAPATH_HPP
