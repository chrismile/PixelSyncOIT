//
// Created by christoph on 18.12.18.
//

#include <iostream>
#include <fstream>
#include <boost/algorithm/string/predicate.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>

#include "CameraPath.hpp"
#include "Math/Math.hpp"

//#define ROTATION_AND_ZOOMING_MODE
#define ROTATION_MODE
//#define ZOOMING_MODE
//#define VIDEO_MODE

ControlPoint::ControlPoint(float time, float tx, float ty, float tz, float yaw, float pitch)
{
    this->time = time;
    this->position = glm::vec3(tx, ty, tz);

    /*glm::vec3 cameraFront, cameraRight, cameraUp;
    glm::vec3 globalUp(0.0f, 1.0f, 0.0f);
    cameraFront.x = cos(yaw) * cos(pitch);
    cameraFront.y = sin(pitch);
    cameraFront.z = sin(yaw) * cos(pitch);

    cameraFront = glm::normalize(cameraFront);
    cameraRight = glm::normalize(glm::cross(cameraFront, globalUp));
    cameraUp    = glm::normalize(glm::cross(cameraRight, cameraFront));

    this->orientation = glm::toQuat(glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp));*/
////    this->orientation = glm::toQuat(glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp));*/
//    this->orientation = glm::quat(glm::vec3(pitch, yaw, 0));
////    this->orientation = glm::toQuat(glm::orientate3(glm::vec3(yaw, pitch, 0)));
//
//    float yaw2 = glm::yaw(this->orientation);
//    float pitch2 = glm::pitch(this->orientation);
//    float test = 0;

    this->orientation = glm::angleAxis(-pitch, glm::vec3(1, 0, 0)) * glm::angleAxis(yaw + sgl::PI / 2.0f, glm::vec3(0, 1, 0));
}

void CameraPath::fromCirclePath(sgl::AABB3 &sceneBoundingBox, const std::string &modelFilenamePure)
{
    const size_t NUM_CIRCLE_POINTS = 64;
    controlPoints.clear();

    glm::vec3 centerOffset(0.0f, 0.0f, 0.0f);
    if (boost::starts_with(modelFilenamePure, "Data/Hair/ponytail")) {
        centerOffset.y += 0.1f;
    }
    float startAngle = 0.0f;
    float standardZoom = 1.0f;
    float rotateFactor = 1.0f;
    float yaw = 0;

    bool isConvectionRolls = boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output");
    bool isRings = boost::starts_with(modelFilenamePure, "Data/Rings");
    bool isHair = boost::starts_with(modelFilenamePure, "Data/Hair");
    bool isUCLA = boost::starts_with(modelFilenamePure, "Data/UCLA");

    if (isConvectionRolls)
    {
        sceneBoundingBox = sgl::AABB3();
        sceneBoundingBox.combine(glm::vec3(-1, -0.5, -1));
        sceneBoundingBox.combine(glm::vec3( 1, 0.5, 1));
    }

#ifdef VIDEO_MODE
    float pulseFactor = 0.0f;

    if (boost::starts_with(modelFilenamePure, "Data/Trajectories/9213_streamlines")) {
        startAngle += 1.2f;
        standardZoom = 1.15f;
        centerOffset.x -= 0.1f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence8000")) {
        standardZoom = 1.5f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence2000")) {
        standardZoom = 2.0f;
        startAngle += 1.58f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
        centerOffset.y -= 0.05f;
        standardZoom = 1.3f;
    }
    else if (isConvectionRolls) {
        standardZoom = 0.9f;
        startAngle += 1.58f;
        centerOffset.y += 0.5f;
//        centerOffset.z -= 1.5;
//        centerOffset.x += 0.3;
        yaw = -0.62769;

        // for zooming: 0, 1.04763, 1.76833, 0.806672, -1.56452, -1.61031
        // for rotation: 1, 1.1138, 0.395518, 2.17143, -1.62076, -0.60769
    }

    else if (isRings)
    {
        standardZoom = 1.7;
    }

    else if (isHair)
    {
        standardZoom = 0.8;
    }
#endif

#ifdef ROTATION_AND_ZOOMING_MODE
    float pulseFactor = 2.0f;



    // Zooming and pulsing
    if (boost::starts_with(modelFilenamePure, "Data/Trajectories/9213_streamlines")) {
        startAngle += 1.2f;
        standardZoom = 1.0f;
        pulseFactor = 1.1f;
        centerOffset.x -= 0.1f;
    }
    if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence8000")) {
        pulseFactor = 4.0f;
        standardZoom = 1.8f;
    }
    if (isConvectionRolls) {
        pulseFactor = 1.5f;
        standardZoom = 0.8f;
        startAngle += 0.0f;
        centerOffset.y += 0.15f;
//        centerOffset.z -= 1.5;
//        centerOffset.x += 0.3;
        yaw = -0.50769;

        // for zooming: 0, 1.04763, 1.76833, 0.806672, -1.56452, -1.61031
        // for rotation: 1, 1.1138, 0.395518, 2.17143, -1.62076, -0.60769
    }


//    if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
//        centerOffset.y -= 0.05f;
//        pulseFactor = 2.0f;
//        standardZoom = 1.3f;
//    }
#endif

#ifdef ROTATION_MODE
    float pulseFactor = 0.0f;

    if (boost::starts_with(modelFilenamePure, "Data/Trajectories/9213_streamlines")) {
        startAngle += 1.2f;
        standardZoom = 1.1f;
        centerOffset.x -= 0.1f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence8000")) {
        standardZoom = 1.3f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence2000")) {
        standardZoom = 2.0f;
        startAngle += 1.58f;
    }
    else if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
        centerOffset.y -= 0.05f;
        standardZoom = 1.3f;
    }
    else if (isConvectionRolls) {
        standardZoom = 0.8f;
        startAngle += 1.58f;
        centerOffset.y += 0.4f;
//        centerOffset.z -= 1.5;
//        centerOffset.x += 0.3;
        yaw = -0.60769;

        // for zooming: 0, 1.04763, 1.76833, 0.806672, -1.56452, -1.61031
        // for rotation: 1, 1.1138, 0.395518, 2.17143, -1.62076, -0.60769
    }
    else if (isUCLA)
    {
        standardZoom = 1.0f;
        yaw = -0.20;
    }

    else if (isRings)
    {
        standardZoom = 1.3;
    }

    else if (isHair)
    {
        standardZoom = 0.8;
    }

#endif

#ifdef ZOOMING_MODE
    float pulseFactor = 0.0f;
    rotateFactor = 0.0f;

    if (boost::starts_with(modelFilenamePure, "Data/Trajectories/9213_streamlines")) {
        startAngle += 1.2f;
        standardZoom = 0.6f;
        centerOffset.x -= 0.1f;
        pulseFactor = 2.5f;
    }
    if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence8000")) {
        standardZoom = 1.2f;
        pulseFactor = 2.5f;
    }
    if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence2000")) {
        standardZoom = 1.0f;
        startAngle += 1.58f;
        pulseFactor = 2.5f;
    }
    if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
        centerOffset.y -= 0.05f;
        standardZoom = 0.8f;
        pulseFactor = 2.5f;
    }

    standardZoom += pulseFactor / 2.0f;

#endif

    std::ofstream testFile("Data/ControlPoints.txt");

    for (size_t i = 0; i <= NUM_CIRCLE_POINTS; i++) {
        float time = float(i) * 0.5f;//float(i)/NUM_CIRCLE_POINTS;
        float angleTrans = float(i)/NUM_CIRCLE_POINTS*sgl::TWO_PI*rotateFactor;

        float angle = angleTrans + startAngle;
//        float pulseRadius = (cos(2.0f*angle)-1.0f)/(8.0f/pulseFactor)+standardZoom;
#ifdef ZOOMING_MODE
        float pulseRadius = cos(float(i) / NUM_CIRCLE_POINTS * sgl::PI * 2) * pulseFactor / 2.0 + standardZoom;
#else
        float pulseRadius = (cos(2.0f*angle)-1.0f)/(8.0f/pulseFactor)+standardZoom;
#endif

//                (pulseFactor * float(i)/ NUM_CIRCLE_POINTS) / 2.0f - pulseFactor + standardZoom;
        glm::vec3 cameraPos = glm::vec3(cos(angle)*pulseRadius, 0.0f, sin(angle)*pulseRadius)
                * glm::length(sceneBoundingBox.getExtent()) + sceneBoundingBox.getCenter() + centerOffset;
        controlPoints.push_back(ControlPoint(time, cameraPos.x, cameraPos.y, cameraPos.z, sgl::PI + angle, yaw));

        testFile << "time: " << time << "(" << cameraPos.x << "," << cameraPos.y << "," << cameraPos.z << ")\n" << std::endl;
    }
    testFile.close();

    update(0.0f);
}

void CameraPath::fromControlPoints(const std::vector<ControlPoint> &controlPoints)
{
    this->controlPoints = controlPoints;
    update(0.0f);
}

const uint32_t CAMERA_PATH_FORMAT_VERSION = 1u;

bool CameraPath::fromBinaryFile(const std::string &filename)
{
    std::ifstream file(filename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in CameraPath::fromBinaryFile: File \""
                + filename + "\" not found.");
        return false;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);
    file.close();

    sgl::BinaryReadStream stream(buffer, size);
    uint32_t version;
    stream.read(version);
    if (version != CAMERA_PATH_FORMAT_VERSION) {
        sgl::Logfile::get()->writeError(std::string() + "Error in CameraPath::fromBinaryFile: "
                + "Invalid version in file \"" + filename + "\".");
        return false;
    }

    controlPoints.clear();
    stream.readArray(controlPoints);
    update(0.0f);

    return true;
}

bool CameraPath::saveToBinaryFile(const std::string &filename)
{
    std::ofstream file(filename.c_str(), std::ofstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in CameraPath::saveToBinaryFile: File \""
                + filename + "\" not found.");
        return false;
    }

    sgl::BinaryWriteStream stream;
    stream.write((uint32_t)CAMERA_PATH_FORMAT_VERSION);
    stream.writeArray(controlPoints);
    file.write((const char*)stream.getBuffer(), stream.getSize());
    file.close();

    return true;
}


void CameraPath::normalizeToTotalTime(float totalTime)
{
    float factor = totalTime / getEndTime();
    int n = (int)controlPoints.size();
    for (int i = 0; i < n; i++) {
        controlPoints.at(i).time *= factor;
    }
}

void CameraPath::update(float currentTime)
{
    time = fmod(currentTime, controlPoints.back().time);

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

