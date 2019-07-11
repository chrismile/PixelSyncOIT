/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "RTRenderBackend.hpp"
#include <Utils/File/FileUtils.hpp>
#include <Utils/AppSettings.hpp>

void RTRenderBackend::setViewportSize(int width, int height) {
    this->width = width;
    this->height = height;
    image.resize(width * height);
}

void RTRenderBackend::loadTubePrimitives(const std::string &filename){
    /**
     * TODO: Convert the trajectories to an internal representation.
     * NOTE: Trajectories can save multiple attributes (i.e., importance criteria).
     * As a simplification, we only use the first attribute for rendering and ignore everything else.
     * The data type 'Trajectories' is defined in "stc/Utils/TrajectoryFile.hpp".
     */
    std::cout << "Start loading obj file to tube primitives..." << std::endl;
    std::ifstream infile(filename);
    std::string line;
    std::vector<int> index;
    std::vector<float> pos;
    std::vector<float> attr;
    Link tempNodeLink;
    Node tempNode;
    ospcommon::vec3f p;
    ospcommon::vec4f color;
    color.x = 1.0; color.y = 0.0; color.z = 0.0; color.w = 1.0;
    while(std::getline(infile, line)){
        // std::cout << line << "\n";
        char ch = line[0];
        // char v = "v"; char g = "g"; char l = "l";
        if(ch == 'v'){
            if(line[1] == 't'){
            std::string substring = line.substr(3, line.length());
            attr.push_back(std::stof(substring));
            }else{
            std::string substring = line.substr(2, line.length());
                        std::string temp = "";
            for(int i = 0; i <= substring.length(); i++){
                // std::cout << substring[i] << " ";
                if(substring[i] >= '0' && substring[i] <= '9'){
                // std::cout << substring[i] << " ";
                temp = temp + substring[i];
                // std::cout << "temp " << temp << std::endl;
                }else if(substring[i] == '.'){
                temp = temp + substring[i];
                }
                else{
                if(temp != ""){
                    pos.push_back(std::stof(temp));
                }
                temp = "";
                }
            }
            float radius = 0.01;
            p.x = pos[0];
            p.y = pos[1];
            p.z = pos[2];
            tempNode = {p, radius};
            // colors.push_back(color);
            Tube.nodes.push_back(tempNode);
            pos.clear();
            }
        }else if(ch == 'g'){
            std::string substring = line.substr(2, line.length());
        }else if(ch == 'l'){
            std::string substring = line.substr(2, line.length());
            // std::cout << substring.length() << " " << substring << "\n";
            // std::cout << "\n";
            std::string temp = "";
            for(int i = 0; i <= substring.length(); i++){
                if(substring[i] >= '0' && substring[i] <= '9'){
                    temp = temp + substring[i];
                    // std::cout << temp << " ";
                }else{
                    if(temp != ""){
                        index.push_back(std::stoi(temp));
                        // std::cout << temp << " " << "\n";
                    }
                    temp = "";
                }
            }
            // save the index to tubes link
            int first = index[0];
            int second = first;
            tempNodeLink = {first - 1, second - 1};
            Tube.links.push_back(tempNodeLink);
            
            for(int i = 1; i < index.size(); i++){
                first = index[i];
                second = first - 1;
                tempNodeLink = {first - 1, second - 1};
                Tube.links.push_back(tempNodeLink);
            }
            index.clear();
            // std::cout << "\n";
        }
    }// end while
    Tube.worldBounds = ospcommon::empty;
    for (int i = 0; i < Tube.nodes.size(); i++) {
      Tube.worldBounds.extend(Tube.nodes[i].position - ospcommon::vec3f(Tube.nodes[i].radius));
      Tube.worldBounds.extend(Tube.nodes[i].position + ospcommon::vec3f(Tube.nodes[i].radius));
    }
    std::cout << "Data world bound lower: (" << Tube.worldBounds.lower.x << ", " << Tube.worldBounds.lower.y << ", " << Tube.worldBounds.lower.z << "), upper (" <<
                                                Tube.worldBounds.upper.x << ", " << Tube.worldBounds.upper.y << ", " << Tube.worldBounds.upper.z << ")" << "\n";
    
    std::cout << "Finish loading data " << std::endl;
    // sleep(100);
}

void RTRenderBackend::loadTriangleMesh(
        const std::string &filename,
        const std::vector<uint32_t> &indices,
        const std::vector<glm::vec3> &vertices,
        const std::vector<glm::vec3> &vertexNormals,
        const std::vector<float> &vertexAttributes) {
    // TODO: Convert the triangle mesh to an internal representation.
}

void RTRenderBackend::setTransferFunction(
        const std::vector<OpacityPoint> &opacityPoints, const std::vector<ColorPoint_sRGB> &colorPoints_sRGB) {
    // Interpolate the color points in sRGB space. Then, convert the result to linear RGB for rendering:
    //TransferFunctionWindow::sRGBToLinearRGB(glm::vec3(...));
}

void RTRenderBackend::setLineRadius(float lineRadius) {
    // TODO: Adapt the line radius (and ignore this function call for triangle meshes).
}

uint32_t *RTRenderBackend::renderToImage(
        const glm::vec3 &pos, const glm::vec3 &dir, const glm::vec3 &up, const float fovy) {
    /**
     * TODO: Write to the image (RGBA32, pre-multiplied alpha, sRGB).
     * For internal computations, linear RGB is used. The values need to be converted to sRGB before writing to the
     * image. For this, the following function can be used:
     * TransferFunctionWindow::linearRGBTosRGB(glm::vec3(...))
     */
    std::cout << "Start Rendering " << std::endl;
    // ! Initialize OSPRay
    int argc = sgl::FileUtils::get()->get_argc();
    char **argv = sgl::FileUtils::get()->get_argv();
    const char **av = const_cast<const char**>(argv);
    OSPError init_error = ospInit(&argc, av);
    if (init_error != OSP_NO_ERROR){
        exit (EXIT_FAILURE);
    }else{
        std::cout << "== ospray initialized successfully" << std::endl;
    }
    // ! Load tube module
    ospLoadModule("tubes");
    // ! Camera
    // glm::vec3 focus = glm::vec3((worldBounds.upper.x - worldBounds.lower.x) / 2.f  + worldBounds.lower.x, 
    //                     (worldBounds.upper.y - worldBounds.lower.y) / 2.f  + worldBounds.lower.y, 
    //                     (worldBounds.upper.z - worldBounds.lower.z) / 2.f  + worldBounds.lower.z);
    OSPCamera camera = ospNewCamera("perspective");
    float camPos[] = {pos.x, pos.y, pos.z};
    float camDir[] = {dir.x, dir.y, dir.z};
    float camUp[] = {up.x, up.y, up.z};
    ospSetf(camera, "aspect", width/(float)height);
    ospSet1f(camera, "fovy", fovy);
    ospSet3fv(camera, "pos", camPos);
    ospSet3fv(camera, "dir", camDir);
    ospSet3fv(camera, "up", camUp);
    ospCommit(camera);
    // ! tubeGeo
    OSPGeometry tubeGeo = ospNewGeometry("tubes");
    OSPData nodeData = ospNewData(Tube.nodes.size() * sizeof(Node), OSP_RAW, Tube.nodes.data());
    OSPData linkData   = ospNewData(Tube.links.size() * sizeof(Link), OSP_RAW, Tube.links.data());
    OSPData colorData   = ospNewData(Tube.colors.size() * sizeof(ospcommon::vec4f), OSP_RAW, Tube.colors.data());
    ospCommit(nodeData);
    ospCommit(linkData);
    ospCommit(colorData);
    
    ospSetData(tubeGeo, "nodeData", nodeData);
    ospSetData(tubeGeo, "linkData", linkData);
    ospSetData(tubeGeo, "colorData", colorData);
    ospCommit(tubeGeo);

    // ! Add tubeGeo to the world
    OSPModel world = ospNewModel();
    ospAddGeometry(world, tubeGeo);
    ospRelease(tubeGeo); // we are done using this handle
    ospCommit(world);

    OSPFrameBuffer  framebuffer = ospNewFrameBuffer(osp::vec2i{width, height}, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);

    OSPRenderer renderer = ospNewRenderer("scivis");
    //! lighting
    OSPLight ambient_light = ospNewLight(renderer, "AmbientLight");
    ospSet1f(ambient_light, "intensity", 0.8f);
    ospSetVec3f(ambient_light, "color", osp::vec3f{174.f / 255.0f, 218.0f / 255.f, 1.0f});
    ospCommit(ambient_light);
    OSPLight directional_light0 = ospNewLight(renderer, "DirectionalLight");
    ospSet1f(directional_light0, "intensity", 0.7f);
    ospSetVec3f(directional_light0, "direction", osp::vec3f{80.f, 25.f, 35.f});
    ospSetVec3f(directional_light0, "color", osp::vec3f{1.0f, 232.f / 255.0f, 166.0f / 255.f});
    ospCommit(directional_light0);
    OSPLight directional_light1 = ospNewLight(renderer, "DirectionalLight");
    ospSet1f(directional_light1, "intensity", 0.7f);
    ospSetVec3f(directional_light1, "direction", osp::vec3f{0.f, -1.f, 0.f});
    ospSetVec3f(directional_light1, "color", osp::vec3f{1.0f, 1.0f, 1.0f});
    ospCommit(directional_light1);
    std::vector<OSPLight> light_list {ambient_light, directional_light0} ;
    OSPData lights = ospNewData(light_list.size(), OSP_OBJECT, light_list.data());
    ospCommit(lights);

    //! renderer
    ospSetVec4f(renderer, "bgColor", osp::vec4f{255.f / 255.f, 255.f/ 255.f, 255.f/ 255.f, 1.0f});
    ospSetData(renderer, "lights", lights);
    ospSetObject(renderer, "model", world);
    ospSetObject(renderer, "camera", camera);
    ospSet1i(renderer, "shadowsEnabled", 1);
    ospSet1f(renderer, "epsilon", 0.1f);
    ospSet1i(renderer, "aoSamples", 0);
    ospSet1f(renderer, "aoDistance", 100.f);
    ospSet1i(renderer, "spp", 1);
    ospSet1i(renderer, "oneSidedLighting", 0);
    ospCommit(renderer);

    ospFrameBufferClear(framebuffer, OSP_FB_COLOR| OSP_FB_ACCUM);
    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR| OSP_FB_ACCUM);
    
    const uint32_t * fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    std::vector<uint32_t> im(fb, fb + width * height * 4);
    image = im;

    sleep(100);

    return &image.front();
}
