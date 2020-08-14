//
// Created by kerninator on 8/14/20.
//

#ifndef PIXELSYNCOIT_MULTIVARWINDOW_HPP
#define PIXELSYNCOIT_MULTIVARWINDOW_HPP


#include <string>
#include <vector>

#include <Utils/MeshSerializer.hpp>
#include <glm/glm.hpp>
#include <Graphics/Color.hpp>
#include <ImGui/ImGuiWrapper.hpp>

class MultiVarWindow
{
public:
    explicit MultiVarWindow();
    // Render GUI elements
    bool renderGUI();

    void setClearColor(const sgl::Color& _clearColor) {clearColor = _clearColor; }
    void setShow(const bool _show) { show = _show; }
    void setVariables(const std::vector<ImportanceCriterionAttribute>& _variables,
                       const std::vector<std::string>& _names);

protected:
    bool show;
    int32_t variableIndex;
    sgl::Color clearColor;
    int32_t histogramRes;
    std::vector<ImportanceCriterionAttribute> variables;
    std::vector<std::string> names;
    std::vector<std::vector<float>> histograms;
    float histogramsMax;
    std::vector<glm::vec2> variablesMinMax;

    void computeHistograms();
    // Render a VIS graph for the currently selected variable
    void renderVarChart();
    // Render the settings to control the information plots
    void renderSettings();
};


#endif //PIXELSYNCOIT_MULTIVARWINDOW_HPP
