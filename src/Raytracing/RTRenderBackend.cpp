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
#include "helper.h"

#include <iterator>
#include <algorithm>

void writePPM(const char *fileName,
              const int x,
              const int y,
              const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if (!file) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", x, y);
  unsigned char *out = (unsigned char *)alloca(3 * x);
  for (int iy = 0; iy < y; iy++) {
    const unsigned char *in = (const unsigned char *)&pixel[(y-1-iy)*x];
    for (int ix = 0; ix < x; ix++) {
      out[3*ix + 0] = in[4*ix + 0];
      out[3*ix + 1] = in[4*ix + 1];
      out[3*ix + 2] = in[4*ix + 2];
    }
    fwrite(out, 3*x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}

void RTRenderBackend::setViewportSize(int width, int height) {
    this->width = width;
    this->height = height;
    image.resize(width * height);
}

void RTRenderBackend::loadTrajectories(const std::string &filename, const Trajectories &trajectories)
{
    // convert trajectories to tubes
    Tube.nodes.clear();
    Tube.links.clear();
    Tube.colors.clear();
    std::cout << "Start convert trajectories to tube primitives..." << std::endl;
    // std::cout << "node size = " << trajectories.positions.size() << std::endl;
    // std::cout << "line size = " << trajectories.size() << std::endl;
    int length = 0;
    for(int i = 0; i < trajectories.size(); i++){
        // one line 
        Trajectory trajectory = trajectories[i];
        // positions and attributes for one line
        std::vector<glm::vec3> positions = trajectory.positions;
        std::vector<std::vector<float>> attributes = trajectory.attributes;
        // std::cout << "node size " << trajectory.positions.size() << std::endl;
        ospcommon::vec4f color;
        // color.x = 1.0; color.y = 0.0; color.z = 0.0; color.w = 1.0;

        for(int j = 0; j < attributes.size(); j++){
            // std::cout << "point size of line #" << i << " is " << positions.size() << std::endl; 
            std::vector<float> attr = attributes[j];
            // std::cout << "attribute size " << attr.size() << std::endl;
            for(int k = 0; k < attr.size(); k++){
                float r = attr[k];
                glm::vec3 pos = positions[k];
                // std::cout << "index " << i * length + k << " ";
                ospcommon::vec3f p(pos.x, pos.y, pos.z);
                Node node;
                node.radius = r; node.position = p;
                Tube.nodes.push_back(node);
                // Tube.colors.push_back(color);
                if(k == 0){
                    int first = length + k;
                    int second = first;
                    // std::cout << second << " ";
                    Link link;
                    link.first = first; link.second = second;
                    Tube.links.push_back(link);
                    // std::cout << "length " << length << " first " << first << " second " << second << std::endl;
                }else{
                    int first = length + k;
                    int second = first - 1;
                    // std::cout << second << " ";
                    Link link;
                    link.first = first; link.second = second;
                    Tube.links.push_back(link);
                }
            }
        }
        std::cout << std::endl;
        length = length + positions.size();
    }
            // sleep(100);
    
    // Tube.worldBounds = ospcommon::empty;
    // for (int i = 0; i < Tube.nodes.size(); i++) {
    //   Tube.worldBounds.extend(Tube.nodes[i].position - ospcommon::vec3f(Tube.nodes[i].radius));
    //   Tube.worldBounds.extend(Tube.nodes[i].position + ospcommon::vec3f(Tube.nodes[i].radius));
    // }
    // std::cout << "Data world bound lower: (" << Tube.worldBounds.lower.x << ", " << Tube.worldBounds.lower.y << ", " << Tube.worldBounds.lower.z << "), upper (" <<
    //                                             Tube.worldBounds.upper.x << ", " << Tube.worldBounds.upper.y << ", " << Tube.worldBounds.upper.z << ")" << "\n";
    
}

void RTRenderBackend::loadTubePrimitives(const std::string &filename){
    /**
     * TODO: Convert the trajectories to an internal representation.
     * NOTE: Trajectories can save multiple attributes (i.e., importance criteria).
     * As a simplification, we only use the first attribute for rendering and ignore everything else.
     * The data type 'Trajectories' is defined in "stc/Utils/TrajectoryFile.hpp".
     */
    
    Tube.nodes.clear();
    Tube.links.clear();
    Tube.colors.clear();
    std::cout << "Start loading obj file to tube primitives..." << std::endl;
    std::ifstream infile(filename);
    std::string line;
    std::vector<int> index;
    std::vector<float> pos;
    // std::vector<float> attr;
    Link tempNodeLink;
    Node tempNode;
    ospcommon::vec3f p;
    ospcommon::vec4f color;
    float radius;
    color.x = 1.0; color.y = 0.0; color.z = 0.0; color.w = 1.0;
    while(std::getline(infile, line)){
        // std::cout << line << "\n";
        char ch = line[0];
        // char v = "v"; char g = "g"; char l = "l";
        if(ch == 'v'){
            if(line[1] == 't'){
            std::string substring = line.substr(3, line.length());
            radius = std::stof(substring);
            // attr.push_back(std::stof(substring));
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
            // float radius = 0.01;
            p.x = pos[0];
            p.y = pos[1];
            p.z = pos[2];
            tempNode = {p, radius};
            // Tube.colors.push_back(color);
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
            tempNodeLink = {first -1, second -1};
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
    for(int i = 0; i < opacityPoints.size(); i++){
        std::cout << "opacity #" << i << " opacity " << opacityPoints[i].opacity << " position " << opacityPoints[i].position << "\n";
    }
    for(int i = 0; i < colorPoints_sRGB.size(); i++){
        std::cout << "color points #" << i << " R " << (int)colorPoints_sRGB[i].color.getR() 
                                            << " G " << (int)colorPoints_sRGB[i].color.getG()
                                            << " B " << (int)colorPoints_sRGB[i].color.getB()
                                            << " A " << (int)colorPoints_sRGB[i].color.getA()
                                            << " position " << colorPoints_sRGB[i].position << "\n";
    }
    //! find the min and max
    std::vector<float> attr;
    for(int i = 0; i < Tube.nodes.size(); i++){
        attr.push_back(Tube.nodes[i].radius);
    }
    const float amin = *std::min_element(attr.begin(), attr.end());
    const float amax = *std::max_element(attr.begin(), attr.end());
    // std::cout << "Attribute min " << amin << " max " << amax << std::endl;
    const float dis = 1.f / (amax - amin);
    // map attributes to the color 
    for(int i = 0; i < attr.size(); ++i){
        float a = attr[i];
        const float ratio = (a - amin) * dis;
        // find TFN index and the fraction
        const int N = int(ratio * (colorPoints_sRGB.size() - 1));
        const float R = ratio * (colorPoints_sRGB.size() - 1) - N;
        // std::cout << "a = " << a << " N = " << N << " R " << R << std::endl;
        if(N < 0){
            glm::vec3 color_sRGB = colorPoints_sRGB.front().color.getFloatColorRGB();
            float opacity = opacityPoints.front().opacity;
            ospcommon::vec4f color(color_sRGB.x, color_sRGB.y , color_sRGB.z, opacity);
            Tube.colors.push_back(color);
        }else if(N >= colorPoints_sRGB.size() - 1){
            glm::vec3 color_sRGB = colorPoints_sRGB.back().color.getFloatColorRGB();
            float opacity = opacityPoints.back().opacity;
            ospcommon::vec4f color(color_sRGB.x, color_sRGB.y , color_sRGB.z, opacity);
            Tube.colors.push_back(color);
        }else{
            glm::vec3 color_sRGB_lo = colorPoints_sRGB[N].color.getFloatColorRGB();
            glm::vec3 color_sRGB_hi = colorPoints_sRGB[N+1].color.getFloatColorRGB();
            float opacity_lo = opacityPoints[N].opacity;
            float opacity_hi = opacityPoints[N+1].opacity;
            // convert sRGB to linear RGB
            glm::vec3 color_linear_lo = TransferFunctionWindow::sRGBToLinearRGB(color_sRGB_lo);
            glm::vec3 color_linear_hi = TransferFunctionWindow::sRGBToLinearRGB(color_sRGB_hi);
            ospcommon::vec4f color_lo(color_linear_lo.x, color_linear_lo.y, color_linear_lo.z, opacity_lo);
            ospcommon::vec4f color_hi(color_linear_hi.x, color_linear_hi.y, color_linear_hi.z, opacity_hi);
            ospcommon::vec4f color = lerp(color_lo, color_hi, R);
            Tube.colors.push_back(color);
        }
    }
    // sleep(100);
}

void RTRenderBackend::setLineRadius(float lineRadius) {
    // TODO: Adapt the line radius (and ignore this function call for triangle meshes).
    std::cout << "nodes size " << Tube.nodes.size() << "\n";
    std::cout << "link size " << Tube.links.size() << "\n";
    for(int i = 0; i < Tube.nodes.size(); ++i){
        Tube.nodes[i].radius = lineRadius;
    }
    Tube.worldBounds = ospcommon::empty;
    for (int i = 0; i < Tube.nodes.size(); i++) {
      Tube.worldBounds.extend(Tube.nodes[i].position - ospcommon::vec3f(Tube.nodes[i].radius));
      Tube.worldBounds.extend(Tube.nodes[i].position + ospcommon::vec3f(Tube.nodes[i].radius));
    }
    std::cout << "Data world bound lower: (" << Tube.worldBounds.lower.x << ", " << Tube.worldBounds.lower.y << ", " << Tube.worldBounds.lower.z << "), upper (" <<
                                                Tube.worldBounds.upper.x << ", " << Tube.worldBounds.upper.y << ", " << Tube.worldBounds.upper.z << ")" << "\n";
    
    std::cout << "Finish loading data " << std::endl;
}

void RTRenderBackend::commitToOSPRay(const glm::vec3 &pos, const glm::vec3 &dir, const glm::vec3 &up, const float fovy)
{
    // // ! Initialize OSPRay
    // int argc = sgl::FileUtils::get()->get_argc();
    // char **argv = sgl::FileUtils::get()->get_argv();
    // const char **av = const_cast<const char**>(argv);
    // OSPError init_error = ospInit(&argc, av);
    // if (init_error != OSP_NO_ERROR){
    //     std::cout << "== ospray initialization fail" << std::endl;
    //     exit (EXIT_FAILURE);
    // }else{
    //     std::cout << "== ospray initialized successfully" << std::endl;
    // }
    // ! Load tube module
    // ospLoadModule("tubes");
    // std::cout << "camera pos " << pos.x << " " << pos.y << " " << pos.z << std::endl;
    // std::cout << "camera dir " << dir.x << " " << dir.y << " " << dir.z << std::endl;
    // std::cout << "camera pos " << up.x << " " << up.y << " " << up.z << std::endl;
    // std::cout << "fovy " << fovy << std::endl;
    // // ! Camera
    // glm::vec3 focus = glm::vec3((Tube.worldBounds.upper.x - Tube.worldBounds.lower.x) / 2.f  + Tube.worldBounds.lower.x, 
    //                     (Tube.worldBounds.upper.y - Tube.worldBounds.lower.y) / 2.f  + Tube.worldBounds.lower.y, 
    //                     (Tube.worldBounds.upper.z - Tube.worldBounds.lower.z) / 2.f  + Tube.worldBounds.lower.z);
    // glm::vec3 Pos;
    // Pos.x = pos.x + Tube.worldBounds.upper.x;
    // Pos.y = pos.y + pos.y + Tube.worldBounds.upper.y;
    // Pos.z = pos.z + Tube.worldBounds.upper.z;
    // glm::vec3 Dir = focus - pos;
    // OSPCamera camera = ospNewCamera("perspective");
    // float camPos[] = {Pos.x, Pos.y, Pos.z};
    // float camDir[] = {Dir.x, Dir.y, Dir.z};
    // float camUp[] = {up.x, up.y, up.z};
    // ospSetf(camera, "aspect", width/(float)height);
    // ospSet1f(camera, "fovy", fovy);
    // ospSet3fv(camera, "pos", camPos);
    // ospSet3fv(camera, "dir", camDir);
    // ospSet3fv(camera, "up", camUp);
    // ospCommit(camera);
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

    renderer = ospNewRenderer("scivis");
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
    // ospSetObject(renderer, "camera", camera);
    ospSet1i(renderer, "shadowsEnabled", 1);
    ospSet1f(renderer, "epsilon", 0.1f);
    ospSet1i(renderer, "aoSamples", 0);
    ospSet1f(renderer, "aoDistance", 100.f);
    ospSet1i(renderer, "spp", 1);
    ospSet1i(renderer, "oneSidedLighting", 0);
    ospCommit(renderer);
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
    std::cout << "camera pos " << pos.x << " " << pos.y << " " << pos.z << std::endl;
    std::cout << "camera dir " << dir.x << " " << dir.y << " " << dir.z << std::endl;
    std::cout << "camera pos " << up.x << " " << up.y << " " << up.z << std::endl;
    std::cout << "fovy " << fovy << std::endl;
    // // ! Initialize OSPRay
    // int argc = sgl::FileUtils::get()->get_argc();
    // char **argv = sgl::FileUtils::get()->get_argv();
    // const char **av = const_cast<const char**>(argv);
    // OSPError init_error = ospInit(&argc, av);
    // if (init_error != OSP_NO_ERROR){
    //     std::cout << "== ospray initialization fail" << std::endl;
    //     exit (EXIT_FAILURE);
    // }else{
    //     std::cout << "== ospray initialized successfully" << std::endl;
    // }
    // // ! Load tube module
    // ospLoadModule("tubes");

    glm::vec3 focus = glm::vec3((Tube.worldBounds.upper.x - Tube.worldBounds.lower.x) / 2.f  + Tube.worldBounds.lower.x, 
                        (Tube.worldBounds.upper.y - Tube.worldBounds.lower.y) / 2.f  + Tube.worldBounds.lower.y, 
                        (Tube.worldBounds.upper.z - Tube.worldBounds.lower.z) / 2.f  + Tube.worldBounds.lower.z);
    glm::vec3 Pos;
    Pos.x = pos.x;
    Pos.y = pos.y + Tube.worldBounds.upper.y;
    Pos.z = pos.z + Tube.worldBounds.upper.z;
    glm::vec3 Dir = focus - Pos;
    glm::vec3 Up;
    Up.x = 0.f; Up.y = 1.0f; Up.z = 0.f;
    OSPCamera camera = ospNewCamera("perspective");
    float camPos[] = {Pos.x, Pos.y, Pos.z};
    float camDir[] = {Dir.x, Dir.y, Dir.z};
    float camUp[] = {Up.x, Up.y, Up.z};
    ospSetf(camera, "aspect", width/(float)height);
    ospSet1f(camera, "fovy", 30);
    ospSet3fv(camera, "pos", camPos);
    ospSet3fv(camera, "dir", camDir);
    ospSet3fv(camera, "up", camUp);
    ospCommit(camera);

    ospSetObject(renderer, "camera", camera);
    ospCommit(renderer);

    framebuffer = ospNewFrameBuffer(osp::vec2i{width, height}, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);

    ospFrameBufferClear(framebuffer, OSP_FB_COLOR| OSP_FB_ACCUM);
    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR| OSP_FB_ACCUM);
    
    uint32_t * fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    // std::string filename = "test_fb.ppm";
    // writePPM(filename.c_str(), width, height, fb);

    return fb;
}
