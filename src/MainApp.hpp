/*
 * MainApp.hpp
 *
 *  Created on: 22.04.2017
 *      Author: Christoph Neuhauser
 */

#ifndef LOGIC_MainApp_HPP_
#define LOGIC_MainApp_HPP_

#include <vector>
#include <glm/glm.hpp>

#include <Utils/AppLogic.hpp>
#include <Utils/Random/Xorshift.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Math/Geometry/Point2.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Scene/Camera.hpp>

#include "Utils/VideoWriter.hpp"
#include "OIT/OIT_Renderer.hpp"

using namespace std;
using namespace sgl;

class PixelSyncApp : public AppLogic
{
public:
	PixelSyncApp();
	~PixelSyncApp();
	void render();
	void renderScene(); // Renders lighted scene
	void update(float dt);
	void resolutionChanged(EventPtr event);

private:
	// Lighting & rendering
	boost::shared_ptr<Camera> camera;
	ShaderProgramPtr plainShader;
	ShaderProgramPtr whiteSolidShader;

	// Objects in the scene
	boost::shared_ptr<OIT_Renderer> oitRenderer;
	ShaderAttributesPtr transparentObject;
	glm::mat4 rotation;
	glm::mat4 scaling;

	// Save video stream to file
	bool recording;
	VideoWriter *videoWriter;
};

#endif /* LOGIC_MainApp_HPP_ */
