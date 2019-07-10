#ifndef PIXELSYNCOIT_RAYTRACINGRENDERER_HPP
#define PIXELSYNCOIT_RAYTRACINGRENDERER_HPP

#include <iostream>
#include <vector>

#include "../Utils/TrajectoryFile.hpp"
#include "../TransferFunctionWindow.hpp"
#include "ospray/ospray.h"
#include "ospray/ospcommon/vec.h"

struct Node
{
    ospcommon::vec3f position;
    float radius;
};

struct Link
{
    int first;
    int second;
};

struct TubePrimitives
{
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::vector<ospcommon::vec4f> colors;
};

class RayTracingRenderer{
	/**
	 * The filename of the trajectories is passed and not the trajectories themselves, as they will probably need to be converted to an internal representation
	 * that we might want to cache.
	 * If no internal representation was cached, the function "Trajectories loadTrajectoriesFromFile(const std::string &filename, TrajectoryType trajectoryType);"
	 * from https://github.com/chrismile/PixelSyncOIT/blob/master/src/Utils/TrajectoryFile.hpp may be used to load the raw trajectories.
	 * The internal representation is then created from the trajectories, and stored to e.g. the filename plus ".rtr" (Ray-Tracing representation).
	 * The passed value of trajectoryType is just used for passing it on to "loadTrajectoriesFromFile" (see above).
	 */
	TubePrimitives loadTubeFromFile(const std::string &filename, TrajectoryType trajectoryType);
	
	/**
	 * Depending on how you want to internally represent the transfer function, the arguments can be either something like:
	 * "std::vector<uint32_t> transferFunctionMap_LinearRGB" (where we have a look-up array with e.g. 256 values) or
	 * "std::vector<OpacityPoint> opacityPoints, std::vector<ColorPoint_LinearRGB> colorPoints_LinearRGB" where
	 * the opacity and color control points are given explicitly.
	 * (For more details of the classes OpacityPoint and ColorPoint_LinearRGB see
	 * https://github.com/chrismile/PixelSyncOIT/blob/master/src/TransferFunctionWindow.hpp)
	//  */
	// [virtual] void setTransferFunction(...);
    // @ Mengjiao 
    // This function will be called in the loadTubeFromFile() to convert attr to color and opacity. 
    // TransferFunction should have been loaded before call this setTransferFunction()
    // The color will be saved with the tube primitive and commit to ispc side before start rendering
    void setTransferFunction(float attr, TransferFunctionWindow transferFcnWindow);

	// /**
	//  * I guess this might be the easiest interface to pass the rendered image to our program.
	//  * Feel free to change this to RGB24 or something similar depending on the internal format you use.
	//  * You can find the interface of the class Camera here:
	//  * https://github.com/chrismile/sgl/blob/master/src/Graphics/Scene/Camera.hpp
	//  */
	void renderToRGBA32Image(uint8_t *image, int imageWidth, int imageHeight, uint32_t backgroundColor, Camera *camera);

};

#endif