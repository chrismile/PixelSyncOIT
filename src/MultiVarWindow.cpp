//
// Created by kerninator on 8/14/20.
//

#include "MultiVarWindow.hpp"

#include <Math/Math.hpp>
#include <ImGui/imgui_custom.h>

MultiVarWindow::MultiVarWindow()
    : show(true), variableIndex(0), clearColor(sgl::Color(255, 0, 0, 1.0)),
      histogramRes(50)
{

}

void MultiVarWindow::setVariables(const std::vector<ImportanceCriterionAttribute>& _variables,
                                  const std::vector<std::string>& _names)
{
    // Set min/max and further information here
    // Maybe also KDE for Violin Plots
    // Copy Constructor
    variables = _variables;
    names = _names;
    variablesMinMax.clear();

    for (const auto& var : variables)
    {
        variablesMinMax.emplace_back(var.minAttribute, var.maxAttribute);
    }

    // Recompute histograms
    computeHistograms();
}

bool MultiVarWindow::renderGUI()
{
    if (show)
    {
        bool windowIsOpened = true;
        if (!ImGui::Begin("MultiVar Info", &windowIsOpened))
        {
            ImGui::End();
            return false;
        }

        // Render the var info chart
        renderVarChart();
        // Render the settings
        renderSettings();

        ImGui::End();
        return true;
    }

    return false;
}

void MultiVarWindow::computeHistograms()
{
    histograms.resize(variables.size());
    histogramsMax = 0;

    for (auto v = 0; v < variables.size(); ++v)
    {
        const auto& var = variables[v].attributes;
        const auto& minMax = variablesMinMax[v];
//        const auto& name = names[v];
        auto& histogram = histograms[v];
        histogram.resize(histogramRes);

        for (const auto& value : var)
        {
            int32_t index = glm::clamp(static_cast<int32_t>((value - minMax.x)
                    / (minMax.y - minMax.x) * static_cast<float>(histogramRes - 1)),
                            0, 255);
            histogram[index]++;
        }

        // Normalize values of histogram
        for (const auto& numBin : histogram)
        {
            histogramsMax = std::max(histogramsMax, numBin);
        }
//
        for (auto& numBin : histogram)
        {
            numBin /= histogramsMax;
        }
    }
}

void MultiVarWindow::renderVarChart()
{
    int regionWidth = ImGui::GetContentRegionAvailWidth();
    int graphHeight = 150;

    const auto& histogram = histograms[variableIndex];

    ImVec2 cursorPosHistogram = ImGui::GetCursorPos();
//    ImVec2 oldPadding = ImGui::GetStyle().FramePadding;
//    ImGui::GetStyle().FramePadding = ImVec2(1,1);
    ImGui::PlotHistogram("##histogram", &histogram.front(), histogram.size(),
              0,NULL, 0.0f, 1.0f,
                ImVec2(regionWidth, graphHeight));
    ImGui::SetCursorPos(cursorPosHistogram);

//    if (ImGui::ClickArea("##bararea", ImVec2(regionWidth + 2, barHeight), mouseReleased)) {
////        onColorBarClick();
//    }
    bool mouseReleased = false;
    if (ImGui::ClickArea("##grapharea", ImVec2(regionWidth, graphHeight + 2), mouseReleased))
    {

    }
}

void MultiVarWindow::renderSettings()
{
//    ImGui::NewLine();
    if (ImGui::SliderInt("Variable", &variableIndex, 0, variables.size()))
    {
    }

    if (ImGui::SliderInt("Histogram Res.", &histogramRes, 0, 255))
    {
        computeHistograms();
    }
}



