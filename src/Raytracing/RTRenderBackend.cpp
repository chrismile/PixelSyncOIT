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
#include <cmath>
#include <random>

// See: https://stackoverflow.com/questions/2513505/how-to-get-available-memory-c-g
#ifdef linux
#include <unistd.h>
size_t getUsedSystemMemoryBytes()
{
    size_t totalNumPages = sysconf(_SC_PHYS_PAGES);
    size_t availablePages = sysconf(_SC_AVPHYS_PAGES);
    size_t pageSizeBytes = sysconf(_SC_PAGE_SIZE);
    return (totalNumPages - availablePages) * pageSizeBytes;
}
#endif
#ifdef windows
#include <windows.h>
size_t getUsedSystemMemoryBytes()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys - status.ullAvailPhys;
}
#endif

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
    // image.resize(width * height);
    if(framebuffer != NULL){
        ospRelease(framebuffer);
    }
    framebuffer = ospNewFrameBuffer(osp::vec2i{width, height}, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
}

void RTRenderBackend::clearData() {
    if (worldLoaded) {
        ospRelease(world);
        worldLoaded = false;
    }

    Tube.nodes.clear();
    Tube.links.clear();
    Tube.colors.clear();
    Tube.nodes.shrink_to_fit();
    Tube.links.shrink_to_fit();
    Tube.colors.shrink_to_fit();

    this->triangleMesh.indices.clear();
    this->triangleMesh.vertices.clear();
    this->triangleMesh.vertexNormals.clear();
    this->triangleMesh.vertexColors.clear();
    this->triangleMesh.indices.shrink_to_fit();
    this->triangleMesh.vertices.shrink_to_fit();
    this->triangleMesh.vertexNormals.shrink_to_fit();
    this->triangleMesh.vertexColors.shrink_to_fit();

    this->attributes.clear();
    this->attributes.shrink_to_fit();
}

void RTRenderBackend::loadTrajectories(const std::string &filename, const Trajectories &trajectories)
{
    isTriangles = false;
    clearData();

    // convert trajectories to tubes
    std::cout << "Start convert trajectories to tube primitives..." << std::endl;
    // std::cout << "node size = " << trajectories.positions.size() << std::endl;
    // std::cout << "line size = " << trajectories.size() << std::endl;
    int length = 0;
    numOfLines = trajectories.size();
    if(numOfLines > threshold) needSplit = true;
    for(int i = 0; i < trajectories.size(); i++){
        // one line 
        Trajectory trajectory = trajectories[i];
        // positions and attributes for one line
        std::vector<glm::vec3> &positions = trajectory.positions;
        std::vector<std::vector<float>> &attributes = trajectory.attributes;
        // std::cout << "node size " << trajectory.positions.size() << std::endl;
        ospcommon::vec4f color;
        // color.x = 1.0; color.y = 0.0; color.z = 0.0; color.w = 1.0;

        // std::cout << "point size of line #" << i << " is " << positions.size() << std::endl;
        std::vector<float> &attr = attributes.at(0);
        // std::cout << "attribute size " << attr.size() << std::endl;
        for(int k = 0; k < attr.size(); k++){
            float r = attr[k];
            this->attributes.push_back(r);
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
        length = length + positions.size();
    }
}

void RTRenderBackend::loadTriangleMesh(
        const std::string &filename,
        std::vector<uint32_t> &indices,
        std::vector<glm::vec3> &vertices,
        std::vector<glm::vec3> &vertexNormals,
        std::vector<float> &vertexAttributes)
{
    isTriangles = true;
    clearData();

    this->triangleMesh.vertices = vertices;
    vertices.clear();
    vertices.shrink_to_fit();
    this->triangleMesh.vertexNormals = vertexNormals;
    vertexNormals.clear();
    vertexNormals.shrink_to_fit();
    this->attributes = vertexAttributes;
    vertexAttributes.clear();
    vertexAttributes.shrink_to_fit();

    this->triangleMesh.indices.resize(indices.size());
    for (size_t i = 0; i < indices.size(); i++) {
        this->triangleMesh.indices.at(i) = indices.at(i);
    }
    indices.clear();
    indices.shrink_to_fit();
}

void RTRenderBackend::setTransferFunction(const std::vector<sgl::Color> &tfLookupTable) {
    // Interpolate the color points in sRGB space. Then, convert the result to linear RGB for rendering:
    //TransferFunctionWindow::sRGBToLinearRGB(glm::vec3(...));
    //! find the min and max
    const float amin = *std::min_element(this->attributes.begin(), this->attributes.end());
    const float amax = *std::max_element(this->attributes.begin(), this->attributes.end());
    // std::cout << "Attribute min " << amin << " max " << amax << std::endl;
    const float dis = 1.f / (amax - amin);
    // map attributes to the color

    std::vector<ospcommon::vec4f> newColors;
    newColors.reserve(this->attributes.size());

    for(int i = 0; i < this->attributes.size(); ++i){
        float a = this->attributes[i];
        const float ratio = (a - amin) * dis;
        // find TFN index and the fraction
        const int N = int(ratio * (tfLookupTable.size() - 1));
        const float R = ratio * (tfLookupTable.size() - 1) - N;
        // std::cout << "a = " << a << " N = " << N << " R " << R << std::endl;
        if(N < 0){
            glm::vec4 colorRGBA = tfLookupTable.front().getFloatColorRGBA();
            glm::vec3 color_sRGB(colorRGBA);
            glm::vec3 color_linear_rgb = TransferFunctionWindow::sRGBToLinearRGB(color_sRGB);
            float opacity = colorRGBA.a;
            ospcommon::vec4f color(color_linear_rgb.r, color_linear_rgb.g , color_linear_rgb.b, colorRGBA.a);
            newColors.push_back(color);
        }else if(N >= tfLookupTable.size() - 1){
            glm::vec4 colorRGBA = tfLookupTable.back().getFloatColorRGBA();
            glm::vec3 color_sRGB(colorRGBA);
            glm::vec3 color_linear_rgb = TransferFunctionWindow::sRGBToLinearRGB(color_sRGB);
            float opacity = colorRGBA.a;
            ospcommon::vec4f color(color_linear_rgb.r, color_linear_rgb.g , color_linear_rgb.b, opacity);
            newColors.push_back(color);
        }else{
            glm::vec4 colorRGBA_lo = tfLookupTable.at(N).getFloatColorRGBA();
            glm::vec4 colorRGBA_hi = tfLookupTable.at(N+1).getFloatColorRGBA();
            glm::vec3 color_sRGB_lo(colorRGBA_lo);
            glm::vec3 color_sRGB_hi(colorRGBA_hi);
            glm::vec3 color_sRGB = glm::mix(color_sRGB_lo, color_sRGB_hi, R);
            float opacity_lo = colorRGBA_lo.a;
            float opacity_hi = colorRGBA_hi.a;
            float opacity = glm::mix(opacity_lo, opacity_hi, R);
            // convert sRGB to linear RGB
            glm::vec3 color_linear = TransferFunctionWindow::sRGBToLinearRGB(color_sRGB);
            ospcommon::vec4f color(color_linear.x, color_linear.y, color_linear.z, opacity);
            newColors.push_back(color);
        }
    }

    if (isTriangles) {
        this->triangleMesh.vertexColors = newColors;
        colorNeedsRecommit = true;
    } else {
        Tube.colors = newColors;
    }
}

void RTRenderBackend::setLineRadius(float lineRadius) {
    // TODO: Adapt the line radius (and ignore this function call for triangle meshes).
    // std::cout << "nodes size " << Tube.nodes.size() << "\n";
    // std::cout << "link size " << Tube.links.size() << "\n";
    for(int i = 0; i < Tube.nodes.size(); ++i){
        Tube.nodes[i].radius = lineRadius;
    }
}

void RTRenderBackend::setUseEmbreeCurves(bool useEmbreeCurves) {
    use_Embree = useEmbreeCurves;
}

void RTRenderBackend::setCameraInitialize(glm::vec3 dir)
{
    camera_dir = dir;
}

void RTRenderBackend::recommitRadius()
{
    if (isTriangles) {
        return;
    }

    if (use_Embree){
        std::vector<ospcommon::vec4f> points;

        for(int i = 0; i < Tube.nodes.size(); i++){
            ospcommon::vec4f p;
            p.x = Tube.nodes[i].position.x;
            p.y = Tube.nodes[i].position.y;
            p.z = Tube.nodes[i].position.z;
            p.w = Tube.nodes[i].radius;
            points.push_back(p);
        }
        OSPData pointsData  = ospNewData(points.size(), OSP_FLOAT4, points.data());
        ospSetData(tubeGeo, "vertex", pointsData);
        ospCommit(tubeGeo);
        ospCommit(world);
    } else {
        if (needSplit) {
            for (int i = 0; i < index.size(); i++) {
                int start,end;
                if (i == index.size() - 1) {
                    start = index[i];
                    end = Tube.links.size() - 1;
                } else {
                    start = index[i];
                    end = index[i+1] - 1;
                }
                auto first_node = Tube.nodes.begin() + start;
                auto last_node = Tube.nodes.begin() + end + 1;
                std::vector<Node> sub_nodes(first_node, last_node);  
                OSPData nodeData   = ospNewData(sub_nodes.size() * sizeof(Node), OSP_RAW, sub_nodes.data());
                ospCommit(nodeData);

                if (i == 0) {
                    ospSetData(tubeGeo, "nodeData", nodeData);
                    ospCommit(tubeGeo);
                } else {
                    ospSetData(tubeGeo1, "nodeData", nodeData);
                    ospCommit(tubeGeo1);
                }            
            }
            ospCommit(world);
        } else {
            OSPData nodeData = ospNewData(Tube.nodes.size() * sizeof(Node), OSP_RAW, Tube.nodes.data());
            ospSetData(tubeGeo, "nodeData", nodeData);
            ospCommit(tubeGeo);
            ospCommit(world);
        }
    }
}

void RTRenderBackend::recommitColor()
{
    if (isTriangles) {
        if (colorNeedsRecommit) {
            OSPData colorsData = ospNewData(triangleMesh.vertexColors.size(), OSP_FLOAT4, triangleMesh.vertexColors.data());
            triangleMesh.vertexColors.clear();
            triangleMesh.vertexColors.shrink_to_fit();
            colorNeedsRecommit = false;
            ospSetData(triangleGeo, "vertex.color", colorsData);
            ospCommit(triangleGeo);
            ospCommit(world);
        }
    } else {
        if(use_Embree){
            std::vector<ospcommon::vec4f> colors;
            for(int i = 0; i < Tube.nodes.size(); i++){
                ospcommon::vec4f c;
                c.x = Tube.colors[i].x;
                c.y = Tube.colors[i].y;
                c.z = Tube.colors[i].z;
                c.w = Tube.colors[i].w;
                colors.push_back(c);
            }
            OSPData colorsData  = ospNewData(colors.size(), OSP_FLOAT4, colors.data());
            ospSetData(tubeGeo, "vertex.color", colorsData);
            ospCommit(tubeGeo);
            ospCommit(world);
        } else {
            if (needSplit) {
                for (int i = 0; i < index.size(); i++) {
                    int start,end;
                    if (i == index.size() - 1) {
                        start = index[i];
                        end = Tube.links.size() - 1;
                    } else {
                        start = index[i];
                        end = index[i+1] - 1;
                    }
                    auto first_color = Tube.colors.begin() + start;
                    auto last_color = Tube.colors.begin() + end + 1;
                    std::vector<ospcommon::vec4f> sub_colors(first_color, last_color);
                    OSPData colorData   = ospNewData(sub_colors.size() * sizeof(ospcommon::vec4f), OSP_RAW, sub_colors.data());
                    ospCommit(colorData);

                    if (i == 0) {
                        ospSetData(tubeGeo, "colorData", colorData);
                        ospCommit(tubeGeo);
                    } else {
                        ospSetData(tubeGeo1, "colorData", colorData);
                        ospCommit(tubeGeo1);
                    }
                }
                ospCommit(world);
            } else {
                OSPData colorData   = ospNewData(Tube.colors.size() * sizeof(ospcommon::vec4f), OSP_RAW, Tube.colors.data());
                ospSetData(tubeGeo, "colorData", colorData);
                ospCommit(tubeGeo);
                ospCommit(world);
            }
        }
    }
}

void RTRenderBackend::commitToOSPRay(const glm::vec3 &pos, const glm::vec3 &dir, const glm::vec3 &up, const float fovy)
{
    world = ospNewModel();
    worldLoaded = true;

    if (isTriangles) {
        std::cout << "Using triangle mesh" << "\n";
        triangleGeo = ospNewGeometry("triangles");
        std::vector<ospcommon::vec4f> points;
        std::vector<int> indices;
        std::vector<ospcommon::vec4f> colors;

        OSPData vertexData = ospNewData(triangleMesh.vertices.size(), OSP_FLOAT3, triangleMesh.vertices.data());
        triangleMesh.vertices.clear();
        triangleMesh.vertices.shrink_to_fit();
        OSPData normalData = ospNewData(triangleMesh.vertexNormals.size(), OSP_FLOAT3, triangleMesh.vertexNormals.data());
        triangleMesh.vertexNormals.clear();
        triangleMesh.vertexNormals.shrink_to_fit();
        OSPData colorData = ospNewData(triangleMesh.vertexColors.size(), OSP_FLOAT4, triangleMesh.vertexColors.data());
        triangleMesh.vertexColors.clear();
        triangleMesh.vertexColors.shrink_to_fit();
        colorNeedsRecommit = false;
        OSPData indexData = ospNewData(triangleMesh.indices.size(), OSP_INT, triangleMesh.indices.data());
        triangleMesh.indices.clear();
        triangleMesh.indices.shrink_to_fit();
        ospSetData(triangleGeo, "vertex", vertexData);
        ospSetData(triangleGeo, "vertex.normal", normalData);
        ospSetData(triangleGeo, "vertex.color", colorData);
        ospSetData(triangleGeo, "index", indexData);
        ospCommit(triangleGeo);

        ospAddGeometry(world, triangleGeo);
        ospRelease(triangleGeo); // we are done using this handle
        size_t before = getUsedSystemMemoryBytes();
        ospCommit(world);
        size_t after = getUsedSystemMemoryBytes();
        std::cout << "====================================== " << "\n";
        std::cout << "              MEMORY USAGE             " << std::endl;
        std::cout << " Before : " << before << " After: " << after << std::endl;
        std::cout << " Used: " << after - before << std::endl;
        std::cout << "====================================== " << "\n";
    } else {
        if (!use_Embree) {
            if (needSplit) {
                // separate into two geometris
                // Now is hardcoded for turbulence data set
                // ! tubeGeo
                tubeGeo = ospNewGeometry("tubes");
                tubeGeo1 = ospNewGeometry("tubes");
                //! Construct Links first
                int count = 0;
                for(int i = 0; i < Tube.links.size(); i++){
                    int first = Tube.links[i].first;
                    int second = Tube.links[i].second;
                    if(first == second){
                        count++;
                    }
                    if(count == threshold + 1){
                        index.push_back(i);
                        count = 1;
                    }
                }
                for(int i = 0; i < index.size(); i++){
                    int start,end;
                    if(i == index.size() - 1){
                        start = index[i];
                        end = Tube.links.size() - 1;
                    }else{
                        start = index[i];
                        end = index[i+1] - 1;
                    }

                    auto first_link = Tube.links.begin() + start;
                    auto last_link = Tube.links.begin() + end + 1;
                    std::vector<Link> sub_links(first_link, last_link);
                    for(int j = 0; j < sub_links.size(); j++){
                        int first = sub_links[j].first;
                        int second = sub_links[j].second;
                        if(first == second){
                            sub_links[j].first = j;
                            sub_links[j].second = j;
                        }else{
                            sub_links[j].first = j;
                            sub_links[j].second = j - 1;
                        }
                    }

                    auto first_node = Tube.nodes.begin() + start;
                    auto last_node = Tube.nodes.begin() + end + 1;
                    std::vector<Node> sub_nodes(first_node, last_node);
                    auto first_color = Tube.colors.begin() + start;
                    auto last_color = Tube.colors.begin() + end + 1;
                    std::vector<ospcommon::vec4f> sub_colors(first_color, last_color);
                    OSPData nodeData = ospNewData(sub_nodes.size() * sizeof(Node), OSP_RAW, sub_nodes.data());
                    OSPData linkData   = ospNewData(sub_links.size() * sizeof(Link), OSP_RAW, sub_links.data());
                    OSPData colorData   = ospNewData(sub_colors.size() * sizeof(ospcommon::vec4f), OSP_RAW, sub_colors.data());
                    ospCommit(nodeData);
                    ospCommit(linkData);
                    ospCommit(colorData);

                    // OSPMaterial materialList = ospNewMaterial2("scivis", "OBJMaterial");
                    // ospCommit(materialList);

                    if(i == 0){
                        ospSetData(tubeGeo, "nodeData", nodeData);
                        ospSetData(tubeGeo, "linkData", linkData);
                        ospSetData(tubeGeo, "colorData", colorData);
                        // ospSetMaterial(tubeGeo, materialList);
                        ospCommit(tubeGeo);
                    }else{
                        ospSetData(tubeGeo1, "nodeData", nodeData);
                        ospSetData(tubeGeo1, "linkData", linkData);
                        ospSetData(tubeGeo1, "colorData", colorData);
                        // ospSetMaterial(tubeGeo1, materialList);
                        ospCommit(tubeGeo1);
                    }
                }
                ospAddGeometry(world, tubeGeo);
                ospAddGeometry(world, tubeGeo1);
                ospRelease(tubeGeo); // we are done using this handle
                ospRelease(tubeGeo1);
                size_t before = getUsedSystemMemoryBytes();
                ospCommit(world);
                size_t after = getUsedSystemMemoryBytes();
                std::cout << "====================================== " << "\n";
                std::cout << "              MEMORY USAGE             " << std::endl;
                std::cout << " Before : " << before << " After: " << after << std::endl;
                std::cout << " Used: " << after - before << std::endl;
                std::cout << "====================================== " << "\n";

            } else {
                tubeGeo = ospNewGeometry("tubes");
                OSPData nodeData = ospNewData(Tube.nodes.size() * sizeof(Node), OSP_RAW, Tube.nodes.data());
                OSPData linkData   = ospNewData(Tube.links.size() * sizeof(Link), OSP_RAW, Tube.links.data());
                OSPData colorData   = ospNewData(Tube.colors.size() * sizeof(ospcommon::vec4f), OSP_RAW, Tube.colors.data());
                ospCommit(nodeData);
                ospCommit(linkData);
                ospCommit(colorData);

                // initializedColorData = true;

                OSPMaterial materialList = ospNewMaterial2("scivis", "OBJMaterial");
                ospCommit(materialList);

                ospSetData(tubeGeo, "nodeData", nodeData);
                ospSetData(tubeGeo, "linkData", linkData);
                ospSetData(tubeGeo, "colorData", colorData);
                // ospSetMaterial(tubeGeo, materialList);
                ospCommit(tubeGeo);

                // ! Add tubeGeo to the world
                ospAddGeometry(world, tubeGeo);
                ospRelease(tubeGeo); // we are done using this handle

                size_t before = getUsedSystemMemoryBytes();
                ospCommit(world);
                size_t after = getUsedSystemMemoryBytes();
                std::cout << "====================================== " << "\n";
                std::cout << "              MEMORY USAGE             " << std::endl;
                std::cout << " Before : " << before << " After: " << after << std::endl;
                std::cout << " Used: " << after - before << std::endl;
                std::cout << "====================================== " << "\n";

            }
        } else {
            std::cout << "Using Embree Streamlines" << "\n";
            tubeGeo = ospNewGeometry("streamlines");
            std::vector<ospcommon::vec4f> points;
            std::vector<int> indices;
            std::vector<ospcommon::vec4f> colors;

            for (int i = 0; i < Tube.nodes.size(); i++) {
                ospcommon::vec4f p;
                p.x = Tube.nodes[i].position.x;
                p.y = Tube.nodes[i].position.y;
                p.z = Tube.nodes[i].position.z;
                p.w = Tube.nodes[i].radius;
                points.push_back(p);
                ospcommon::vec4f c;
                c.x = Tube.colors[i].x;
                c.y = Tube.colors[i].y;
                c.z = Tube.colors[i].z;
                c.w = Tube.colors[i].w;
                colors.push_back(c);
            }
            for (int i = 0; i < Tube.links.size() - 1; i++) {
                int first = Tube.links[i].first;
                int second = Tube.links[i].second;
                if(first == second && i != 0){
                    indices.pop_back();
                }
                indices.push_back(first);
            }

            OSPData pointsData  = ospNewData(points.size(), OSP_FLOAT4, points.data());
            OSPData indicesData = ospNewData(indices.size(), OSP_INT, indices.data());
            OSPData colorsData  = ospNewData(colors.size(), OSP_FLOAT4, colors.data());
            ospSetData(tubeGeo, "vertex", pointsData);
            ospSetData(tubeGeo, "index", indicesData);
            ospSetData(tubeGeo, "vertex.color", colorsData);
            ospCommit(tubeGeo);

            ospAddGeometry(world, tubeGeo);
            ospRelease(tubeGeo); // we are done using this handle
            size_t before = getUsedSystemMemoryBytes();
            ospCommit(world);
            size_t after = getUsedSystemMemoryBytes();
            std::cout << "====================================== " << "\n";
            std::cout << "              MEMORY USAGE             " << std::endl;
            std::cout << " Before : " << before << " After: " << after << std::endl;
            std::cout << " Used: " << after - before << std::endl;
            std::cout << "====================================== " << "\n";
        }
    }


    renderer = ospNewRenderer("scivis");
    //! lighting
    OSPLight ambient_light = ospNewLight(renderer, "AmbientLight");
    ospSet1f(ambient_light, "intensity", 0.4f);
    ospSetVec3f(ambient_light, "color", osp::vec3f{255.f / 255.0f, 255.0f / 255.f, 1.0f});
    ospCommit(ambient_light);
    OSPLight directional_light0 = ospNewLight(renderer, "DirectionalLight");
    ospSet1f(directional_light0, "intensity", 0.9f);
    ospSetVec3f(directional_light0, "direction", osp::vec3f{-.5f, -1.f, -.2f});
    // ospSetVec3f(directional_light0, "color", osp::vec3f{1.0f, 255.f / 255.0f, 255.0f / 255.f});
    ospCommit(directional_light0);
    OSPLight directional_light1 = ospNewLight(renderer, "DirectionalLight");
    ospSet1f(directional_light1, "intensity", 0.9f);
    ospSetVec3f(directional_light1, "direction", osp::vec3f{.5f, 1.f, .2f});
    // ospSetVec3f(directional_light0, "color", osp::vec3f{1.0f, 255.f / 255.0f, 255.0f / 255.f});
    ospCommit(directional_light1);
    std::vector<OSPLight> light_list {ambient_light, directional_light0, directional_light1} ;
    OSPData lights = ospNewData(light_list.size(), OSP_OBJECT, light_list.data());
    ospCommit(lights);

    camera = ospNewCamera("perspective");
    float camPos[] = {pos.x, pos.y, pos.z};
    float camDir[] = {dir.x, dir.y, dir.z};
    float camUp[] = {up.x, up.y, up.z};
    ospSetf(camera, "aspect", width/(float)height);
    ospSet1f(camera, "fovy", glm::degrees(fovy));
    ospSet3fv(camera, "pos", camPos);
    ospSet3fv(camera, "dir", camDir);
    ospSet3fv(camera, "up", camUp);
    ospCommit(camera);

    ospSetObject(renderer, "camera", camera);
    //! renderer
    ospSetVec4f(renderer, "bgColor", osp::vec4f{255.f / 255.f, 255.f/ 255.f, 255.f/ 255.f, 1.0f});
    ospSetData(renderer, "lights", lights);
    ospSetObject(renderer, "model", world);
    ospSet1i(renderer, "shadowsEnabled", 0);
    ospSet1f(renderer, "epsilon", 0.1f);
    ospSet1i(renderer, "aoSamples", 0);
    ospSet1f(renderer, "aoDistance", 1000.f);
    ospSet1i(renderer, "spp", 1);
    ospSet1i(renderer, "oneSidedLighting", 0);
    ospCommit(renderer);
}

uint32_t *RTRenderBackend::renderToImage(
        const glm::vec3 &pos, const glm::vec3 &dir, const glm::vec3 &up, const float fovy, float radius, bool changeTFN) {
    /**
     * TODO: Write to the image (RGBA32, pre-multiplied alpha, sRGB).
     * For internal computations, linear RGB is used. The values need to be converted to sRGB before writing to the
     * image. For this, the following function can be used:
     * TransferFunctionWindow::linearRGBTosRGB(glm::vec3(...))
     */
    //  if(changeTFN){
    //     std::cout << "!!!! Here Here Here Transfer Function Changed !!!! " << "\n";
    //  }

    if(changeTFN){
        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
        recommitColor();
    }



    if(camera_dir != dir || camera_pos != pos || camera_up != up){
        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
        float camPos[] = {pos.x, pos.y, pos.z};
        float camDir[] = {dir.x, dir.y, dir.z};
        float camUp[] = {up.x, up.y, up.z};
        ospSetf(camera, "aspect", width/(float)height);
        ospSet1f(camera, "fovy", glm::degrees(fovy));
        ospSet3fv(camera, "pos", camPos);
        ospSet3fv(camera, "dir", camDir);
        ospSet3fv(camera, "up", camUp);
        ospCommit(camera);
    }
    if(lineRadius != radius){
        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
        recommitRadius();
    }

    // auto t1 = std::chrono::high_resolution_clock::now();

    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR| OSP_FB_ACCUM);
    // auto t2 = std::chrono::high_resolution_clock::now();
    // auto dur = std::chrono::duration<double>(t2 - t1);
    // std::cout << "fps =  " <<  1 / dur.count()<< " fps" << std::endl;

    uint32_t * fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);

    // image.insert(image.begin(), fb, fb + width * height * 4);

    ospUnmapFrameBuffer(fb, framebuffer);

    camera_dir = dir;
    camera_up = up;
    camera_pos = pos;
    lineRadius = radius;


    return fb;
}
