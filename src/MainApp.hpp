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
#include <Math/Geometry/Point2.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Graphics/OpenGL/TimerGL.hpp>

#include "Utils/VideoWriter.hpp"
#include "Utils/MeshSerializer.hpp"
#include "OIT/OIT_Renderer.hpp"

using namespace std;
using namespace sgl;

const int NUM_OIT_MODES = 8;
const char *const OIT_MODE_NAMES[] = {
        "K-Buffer", "Linked List", "MLAB", "HT", "MBOIT", "Depth Complexity", "No OIT", "Depth Peeling"
};
enum RenderModeOIT {
        RENDER_MODE_OIT_KBUFFER = 0,
        RENDER_MODE_OIT_LINKED_LIST,
        RENDER_MODE_OIT_MLAB, // Mutli-layer Alpha Blending
        RENDER_MODE_OIT_HT, // Hybrid Transparency
        RENDER_MODE_OIT_MBOIT, // Moment-Based Order-Independent Transparency
        RENDER_MODE_OIT_DEPTH_COMPLEXITY,
        RENDER_MODE_OIT_DUMMY,
        RENDER_MODE_OIT_DEPTH_PEELING
};

const int NUM_MODELS = 6;
const char *const MODEL_FILENAMES[] = {
        "Data/Trajectories/single_streamline", "Data/Trajectories/9213_streamlines", "Data/Models/Ship_04",
        "Data/Models/Monkey", "Data/Models/Box", "Data/Models/dragon"
};
const char *const MODEL_DISPLAYNAMES[] = {
        "Single Streamline", "Streamlines", "Ship", "Monkey", "Box", "Dragon"
};

enum ShaderMode {
	SHADER_MODE_PSEUDO_PHONG, SHADER_MODE_VORTICITY, SHADER_MODE_AMBIENT_OCCLUSION
};

class PixelSyncApp : public AppLogic
{
public:
	PixelSyncApp();
	~PixelSyncApp();
	void render(); // Calls renderOIT and renderGUI
	void renderOIT(); // Uses renderScene and "oitRenderer" to render the scene
	void renderScene(); // Renders lighted scene
	void renderGUI(); // Renders GUI
	void update(float dt);
	void resolutionChanged(EventPtr event);
	void processSDLEvent(const SDL_Event &event);

	// State changes
    void setRenderMode(RenderModeOIT newMode, bool forceReset = false);
    void updateShaderMode(bool newOITRenderer);
    void loadModel(const std::string &filename);

private:
	// Lighting & rendering
	boost::shared_ptr<Camera> camera;
	ShaderProgramPtr transparencyShader;
	ShaderProgramPtr plainShader;
	ShaderProgramPtr whiteSolidShader;

	// Mode
	RenderModeOIT mode = RENDER_MODE_OIT_MLAB;
	ShaderMode shaderMode = SHADER_MODE_PSEUDO_PHONG;
	std::string modelFilenamePure;

	// Off-screen rendering
	FramebufferObjectPtr sceneFramebuffer;
	TexturePtr sceneTexture;
	RenderbufferObjectPtr sceneDepthRBO;

	// Objects in the scene
	boost::shared_ptr<OIT_Renderer> oitRenderer;
	MeshRenderer transparentObject;
	glm::mat4 rotation;
	glm::mat4 scaling;
	sgl::AABB3 boundingBox;

    // User interface
    bool showSettingsWindow = true;
    int usedModelIndex = 3; // TODO Back to 2
    Color bandingColor;
    bool cullBackface = true;

    // Continuous rendering: Re-render each frame or only when scene changes?
    bool continuousRendering = false;
    bool reRender = true;

    // Profiling events
	sgl::TimerGL timer;

	// Save video stream to file
	bool recording;
	VideoWriter *videoWriter;
};

#endif /* LOGIC_MainApp_HPP_ */
