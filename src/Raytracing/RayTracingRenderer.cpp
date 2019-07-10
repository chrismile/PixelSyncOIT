#include "RaytracingRenderer.hpp"

TubePrimitives  RayTracingRenderer::loadTubeFromFile(const std::string &filename, TrajectoryType trajectoryType)
{
    std::cout << "Start loading obj file to tube primitives..." << std::endl;
    TubePrimitives Tube;
    std::ifstream infile(filename);
    std::string line;
    std::vector<int> index;
    std::vector<float> pos;
    std::vector<float> attr;
    Link tempNodeLink;
    Node tempNode;
    ospcommon::vec3f p;
    // vec4f color;
    // color.x = 1.0; color.y = 0.0; color.z = 0.0; color.w = 1.0;
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
    return Tube;

}

void RayTracingRenderer::setTransferFunction(TubePrimitives tube, TransferFunctionWindow transferFcnWindow)
{

}

void renderToRGBA32Image(uint8_t *image, int imageWidth, int imageHeight, uint32_t backgroundColor, Camera *camera)
{
    
}