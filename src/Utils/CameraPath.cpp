//
// Created by christoph on 18.12.18.
//

#include "CameraPath.hpp"
#include "Math/Math.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>

ControlPoint::ControlPoint(float time, float tx, float ty, float tz, float yaw, float pitch)
{
    this->time = time;
    this->position = glm::vec3(tx, ty, tz);
    //this->orientation = glm::quat(glm::vec3(1.0f, 0.0f, 0.0f), pitch) * glm::quat(glm::vec3(0.0f, 1.0f, 0.0f), yaw);//glm::quat(glm::vec3(pitch, yaw, 0.0f));
    //this->orientation = glm::quat(glm::vec3(pitch, 0.0f, 0.0f)) * glm::quat(glm::vec3(0.0f, yaw, 0.0f));
    //this->orientation = glm::quat(glm::vec3(0.0f, yaw, 0.0f)) * glm::quat(glm::vec3(pitch, 0.0f, 0.0f));




    glm::vec3 cameraFront, cameraRight, cameraUp;
    glm::vec3 globalUp(0.0f, 1.0f, 0.0f);
    cameraFront.x = cos(yaw) * cos(pitch);
    cameraFront.y = sin(pitch);
    cameraFront.z = sin(yaw) * cos(pitch);

    cameraFront = glm::normalize(cameraFront);
    cameraRight = glm::normalize(glm::cross(cameraFront, globalUp));
    cameraUp    = glm::normalize(glm::cross(cameraRight, cameraFront));

    //this->orientation = glm::conjugate(glm::toQuat(glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp)));
    this->orientation = glm::toQuat(glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp));




    //this->orientation = glm::quat(glm::vec3(pitch, 0.0f, 0.0f)) * glm::quat(glm::vec3(0.0f, yaw + sgl::PI / 2.0f, 0.0f));


    //this->orientation = glm::conjugate(glm::toQuat(glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp) * sgl::matrixTranslation(-position)));
    this->orientation = glm::angleAxis(-pitch, glm::vec3(1, 0, 0)) * glm::angleAxis(yaw + sgl::PI / 2.0f, glm::vec3(0, 1, 0));
}

CameraPath::CameraPath(const std::vector<ControlPoint> &controlPoints) : controlPoints(controlPoints)
{
    update(0.0f);
}

void CameraPath::update(float dt)
{
    time = fmod(time + dt, controlPoints.back().time);

    // Find either exact control point or two to interpolate between
    int n = (int)controlPoints.size();
    for (int i = 0; i < n; i++) {
        // Perfect match?
        ControlPoint &currentCtrlPoint = controlPoints.at(i);
        if (i == n-1 || glm::abs(time - currentCtrlPoint.time) < 0.0001f) {
            currentTransform = toTransform(currentCtrlPoint.position, currentCtrlPoint.orientation);
            break;
        }
        ControlPoint &nextCtrlPoint = controlPoints.at(i+1);
        if (time > currentCtrlPoint.time && time < nextCtrlPoint.time) {
            float percentage = (time - currentCtrlPoint.time) / (nextCtrlPoint.time - currentCtrlPoint.time);
            glm::vec3 interpPos = glm::mix(currentCtrlPoint.position, nextCtrlPoint.position, percentage);
            glm::quat interpQuat = glm::slerp(currentCtrlPoint.orientation, nextCtrlPoint.orientation, percentage);
            currentTransform = toTransform(interpPos, interpQuat);
            break;
        }
    }
}

glm::mat4x4 CameraPath::toTransform(const glm::vec3 &position, const glm::quat &orientation)
{
    return glm::toMat4(orientation) * sgl::matrixTranslation(-position);
}

