//
// Created by christoph on 18.12.18.
//

#ifndef PIXELSYNCOIT_CAMERAPATH_HPP
#define PIXELSYNCOIT_CAMERAPATH_HPP

#include <vector>
#include <glm/gtc/quaternion.hpp>

#include <Math/Geometry/MatrixUtil.hpp>
#include <Math/Geometry/AABB3.hpp>

class ControlPoint
{
public:
    ControlPoint() {}
    ControlPoint(float time, float tx, float ty, float tz, float yaw, float pitch);

    float time = 0.0f;
    glm::vec3 position;
    glm::quat orientation;
};

class CameraPath
{
public:
    CameraPath() {}
    void fromCirclePath(sgl::AABB3 &sceneBoundingBox, const std::string &modelFilenamePure);
    void fromControlPoints(const std::vector<ControlPoint> &controlPoints);
    bool fromBinaryFile(const std::string &filename);
    bool saveToBinaryFile(const std::string &filename);
    void normalizeToTotalTime(float totalTime);
    void update(float currentTime);
    inline const glm::mat4x4 &getViewMatrix() const { return currentTransform; }
    inline float getEndTime() { return controlPoints.back().time; }

private:
    glm::mat4x4 toTransform(const glm::vec3 &position, const glm::quat &orientation);
    glm::mat4x4 currentTransform;
    std::vector<ControlPoint> controlPoints;
    float time = 0.0f;
};

#endif //PIXELSYNCOIT_CAMERAPATH_HPP
