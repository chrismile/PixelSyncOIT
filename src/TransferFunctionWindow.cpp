//
// Created by christoph on 09.10.18.
//

#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <GL/glew.h>

#include <Utils/File/Logfile.hpp>
#include <Math/Math.hpp>
#include <Input/Mouse.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include "TransferFunctionWindow.hpp"

TransferFunctionWindow::TransferFunctionWindow()
{
    colorPoints = { ColorPoint(sgl::Color(255, 255, 255), 0.0f), ColorPoint(sgl::Color(255, 0, 0), 1.0f) };
    /*colorPoints = { ColorPoint(sgl::Color(255, 255, 255), 0.0f),
                    ColorPoint(sgl::Color(255, 255, 0), 0.5f),
                    ColorPoint(sgl::Color(255, 0, 0), 1.0f)};*/
    opacityPoints = { OpacityPoint(0.0f, 0.0f), OpacityPoint(1.0f, 1.0f) };
    transferFunctionMap.resize(256);
    tfMapBuffer = sgl::Renderer->createGeometryBuffer(sizeof(uint32_t) * 256, sgl::UNIFORM_BUFFER);
    rebuildTransferFunctionMap();
}

void TransferFunctionWindow::setClearColor(const sgl::Color &clearColor)
{
    this->clearColor = clearColor;
}

bool TransferFunctionWindow::renderGUI()
{
    ImGui::Begin("Transfer Function", &showTransferFunctionWindow); // , ImGuiWindowFlags_AlwaysAutoResize);

    renderOpacityGraph();
    renderColorBar();

    if (selectedPointType == SELECTED_POINT_TYPE_OPACITY) {
        if (ImGui::DragFloat("Opacity", &opacitySelection, 0.01f, 0.0f, 1.0f)) {
            opacityPoints.at(currentSelectionIndex).opacity = opacitySelection;
            rebuildTransferFunctionMap();
            reRender = true;
        }
    } else if (selectedPointType == SELECTED_POINT_TYPE_COLOR) {
        if (ImGui::ColorEdit3("Color", (float*)&colorSelection)) {
            colorPoints.at(currentSelectionIndex).color = sgl::colorFromFloat(
                    colorSelection.x, colorSelection.y, colorSelection.z, colorSelection.w);
            rebuildTransferFunctionMap();
            reRender = true;
        }
    }

    ImGui::End();

    if (reRender) {
        reRender = false;
        return true;
    }
    return false;
}

// Code adapted from ImGui's InvisibleButton
bool ImGuiClickButton(const char *str_id, const ImVec2 &size_arg, bool &mouseReleased)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiID id = window->GetID(str_id);
    ImVec2 size = ImGui::CalcItemSize(size_arg, 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = ImGui::ItemHoverable(bb, id);
    bool clicked = ImGui::GetIO().MouseClicked[0] || ImGui::GetIO().MouseClicked[1] || ImGui::GetIO().MouseClicked[2];
    mouseReleased = ImGui::GetIO().MouseReleased[0];

    return hovered && clicked;
}

void TransferFunctionWindow::renderOpacityGraph()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    int regionWidth = ImGui::GetContentRegionAvailWidth();
    int graphHeight = 300;
    int border = 1;
    opacityGraphBox.min = glm::vec2(ImGui::GetCursorScreenPos().x + border, ImGui::GetCursorScreenPos().y + border);
    opacityGraphBox.max = opacityGraphBox.min + glm::vec2(regionWidth - 2.0f*border, graphHeight - 2.0f*border);

    ImColor backgroundColor(clearColor.getFloatR(), clearColor.getFloatG(), clearColor.getFloatB());
    ImColor borderColor(1.0f - clearColor.getFloatR(), 1.0f - clearColor.getFloatG(), 1.0f - clearColor.getFloatB());

    // First render the graph box
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(ImVec2(startPos.x, startPos.y),
                            ImVec2(startPos.x + regionWidth, startPos.y + graphHeight),
                            borderColor,
                            ImGui::GetStyle().FrameRounding);
    drawList->AddRectFilled(ImVec2(startPos.x + border, startPos.y + border),
                            ImVec2(startPos.x + regionWidth - border, startPos.y + graphHeight - border),
                            backgroundColor,
                            ImGui::GetStyle().FrameRounding);

    // Then render the graph itself
    for (int i = 0; i < (int)opacityPoints.size()-1; i++) {
        float positionX0 = opacityPoints.at(i).position * regionWidth;
        float positionX1 = opacityPoints.at(i+1).position * regionWidth;
        float positionY0 = (1.0f - opacityPoints.at(i).opacity) * graphHeight;
        float positionY1 = (1.0f - opacityPoints.at(i+1).opacity) * graphHeight;
        drawList->AddLine(
                ImVec2(startPos.x + positionX0, startPos.y + positionY0),
                ImVec2(startPos.x + positionX1, startPos.y + positionY1),
                borderColor, 2.0f);
    }

    // Finally, render the points
    for (int i = 0; i < (int)opacityPoints.size(); i++) {
        ImVec2 centerPt = ImVec2(startPos.x + border + opacityPoints.at(i).position * (regionWidth - border),
                startPos.y + border + (1.0f - opacityPoints.at(i).opacity) * (graphHeight - border));
        drawList->AddCircleFilled(centerPt, 6, backgroundColor, 24);
        drawList->AddCircle(centerPt, 6, borderColor, 24, 1.5f);
    }


    if (ImGuiClickButton("##grapharea", ImVec2(regionWidth, graphHeight + 2), mouseReleased)) {
        onOpacityGraphClick();
    }
}

void TransferFunctionWindow::renderColorBar()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    int regionWidth = ImGui::GetContentRegionAvailWidth() - 2;
    int barHeight = 30;
    colorBarBox.min = glm::vec2(ImGui::GetCursorScreenPos().x + 1, ImGui::GetCursorScreenPos().y + 1);
    colorBarBox.max = colorBarBox.min + glm::vec2(regionWidth - 2, barHeight - 2);

    // Draw bar
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImVec2 pos = ImVec2(startPos.x + 1, startPos.y + 1);
    for (int i = 0; i < 256; i++) {
        sgl::Color color = transferFunctionMap[i];
        ImU32 colorImgui = ImColor(color.getR(), color.getG(), color.getB());
        drawList->AddLine(ImVec2(pos.x, pos.y), ImVec2(pos.x, pos.y + barHeight), colorImgui, 2.0f * regionWidth / 255.0f);
        pos.x += regionWidth / 255.0f;

    }

    // Draw points
    pos = ImVec2(startPos.x + 2, startPos.y + 2);
    for (int i = 0; i < (int)colorPoints.size(); i++) {
        sgl::Color color = colorPoints.at(i).color;
        ImU32 colorImgui = ImColor(color.getR(), color.getG(), color.getB());
        ImU32 colorInvertedImgui = ImColor(1.0f - color.getFloatR(), 1.0f - color.getFloatG(), 1.0f - color.getFloatB());
        ImVec2 centerPt = ImVec2(pos.x + colorPoints.at(i).position * regionWidth, pos.y + barHeight/2);
        drawList->AddCircleFilled(centerPt, 6, colorImgui, 24);
        drawList->AddCircle(centerPt, 6, colorInvertedImgui, 24);
    }

    if (ImGuiClickButton("##bararea", ImVec2(regionWidth + 2, barHeight), mouseReleased)) {
        onColorBarClick();
    }
}


// For OpenGL: Has 256 entries. Get mapped color for normalized attribute by accessing entry at "attr*255".
std::vector<sgl::Color> TransferFunctionWindow::getTransferFunctionMap()
{
    return transferFunctionMap;
}

sgl::GeometryBufferPtr &TransferFunctionWindow::getTransferFunctionMapUBO()
{
    return tfMapBuffer;
}

// For OpenGL: Has 256 entries. Get mapped color for normalized attribute by accessing entry at "attr*255".
void TransferFunctionWindow::rebuildTransferFunctionMap()
{
    int colorPointsIdx = 0;
    int opacityPointsIdx = 0;
    for (int i = 0; i < 256; i++) {
        sgl::Color colorAtIdx;
        float currentPosition = static_cast<float>(i) / 255.0f;

        // colorPoints.at(colorPointsIdx) should be to the right of/equal to currentPosition
        while (colorPoints.at(colorPointsIdx).position < currentPosition) {
            colorPointsIdx++;
        }
        while (opacityPoints.at(opacityPointsIdx).position < currentPosition) {
            opacityPointsIdx++;
        }

        // Now compute the color...
        if (colorPoints.at(colorPointsIdx).position == currentPosition) {
            colorAtIdx = colorPoints.at(colorPointsIdx).color;
        } else {
            sgl::Color color0 = colorPoints.at(colorPointsIdx-1).color;
            sgl::Color color1 = colorPoints.at(colorPointsIdx).color;
            float pos0 = colorPoints.at(colorPointsIdx-1).position;
            float pos1 = colorPoints.at(colorPointsIdx).position;
            float factor = 1.0 - (pos1 - currentPosition) / (pos1 - pos0);
            colorAtIdx = sgl::colorLerp(color0, color1, factor);
        }

        // ... and the opacity.
        if (opacityPoints.at(opacityPointsIdx).position == currentPosition) {
            colorAtIdx.setFloatA(opacityPoints.at(opacityPointsIdx).opacity);
        } else {
            float opacity0 = opacityPoints.at(opacityPointsIdx-1).opacity;
            float opacity1 = opacityPoints.at(opacityPointsIdx).opacity;
            float pos0 = opacityPoints.at(opacityPointsIdx-1).position;
            float pos1 = opacityPoints.at(opacityPointsIdx).position;
            float factor = 1.0 - (pos1 - currentPosition) / (pos1 - pos0);
            colorAtIdx.setFloatA(sgl::interpolateLinear(opacity0, opacity1, factor));
        }
        //colorAtIdx = sgl::Color(255, 255, 255);

        transferFunctionMap.at(i) = colorAtIdx;
    }

    tfMapBuffer->subData(0, sizeof(uint32_t) * 256, &transferFunctionMap.front());
}


void TransferFunctionWindow::update(float dt)
{
    dragPoint();
}

void TransferFunctionWindow::onOpacityGraphClick()
{
    glm::vec2 mousePosWidget = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y) - opacityGraphBox.min;
    glm::vec2 normalizedPosition = mousePosWidget / opacityGraphBox.getDimensions();
    normalizedPosition.y = 1.0f - normalizedPosition.y;
    dragging = false;

    if (selectNearestOpacityPoint(currentSelectionIndex, mousePosWidget)) {
        // A) Point near to normalized position
        if (ImGui::GetIO().MouseClicked[0]) {
            // A.1 Left clicked? Select/drag-and-drop
            opacitySelection = opacityPoints.at(currentSelectionIndex).opacity;
            selectedPointType = SELECTED_POINT_TYPE_OPACITY;
            if (currentSelectionIndex != 0 && currentSelectionIndex != opacityPoints.size()-1) {
                dragging = true;
            }
        } else if (ImGui::GetIO().MouseClicked[1]) {
            // A.2 Middle clicked? Delete point
            opacityPoints.erase(opacityPoints.begin() + currentSelectionIndex);
            selectedPointType = SELECTED_POINT_TYPE_NONE;
            reRender = true;
        }
    } else {
        // B) If no point near and left clicked: Create new point at position
        if (ImGui::GetIO().MouseClicked[0]) {
            // Compute insert position for new point
            int insertPosition = 0;
            for (insertPosition = 0; insertPosition < (int)opacityPoints.size(); insertPosition++) {
                if (normalizedPosition.x < opacityPoints.at(insertPosition).position
                        || insertPosition == opacityPoints.size()-1) {
                    break;
                }
            }

            // Add new opacity point
            glm::vec2 newPosition = normalizedPosition;
            float newOpacity = newPosition.y;
            opacityPoints.insert(opacityPoints.begin() + insertPosition, OpacityPoint(newOpacity, newPosition.x));
            currentSelectionIndex = insertPosition;
            opacitySelection = opacityPoints.at(currentSelectionIndex).opacity;
            selectedPointType = SELECTED_POINT_TYPE_OPACITY;
            dragging = true;
            reRender = true;
        }
    }

    rebuildTransferFunctionMap();
}

void TransferFunctionWindow::onColorBarClick()
{
    glm::vec2 mousePosWidget = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y) - colorBarBox.min;
    float normalizedPosition = mousePosWidget.x / colorBarBox.getWidth();
    dragging = false;

    if (selectNearestColorPoint(currentSelectionIndex, mousePosWidget)) {
        // A) Point near to normalized position
        if (ImGui::GetIO().MouseClicked[0]) {
            // A.1 Left clicked? Select/drag-and-drop
            colorSelection = ImColor(colorPoints.at(currentSelectionIndex).color.getColorRGB());
            selectedPointType = SELECTED_POINT_TYPE_COLOR;
            if (currentSelectionIndex != 0 && currentSelectionIndex != colorPoints.size()-1) {
                dragging = true;
            }
        } else if (ImGui::GetIO().MouseClicked[1]) {
            // A.2 Middle clicked? Delete point
            colorPoints.erase(colorPoints.begin() + currentSelectionIndex);
            selectedPointType = SELECTED_POINT_TYPE_NONE;
            reRender = true;
        }
    } else {
        // B) If no point near and left clicked: Create new point at position
        if (ImGui::GetIO().MouseClicked[0]) {
            // Compute insert position for new point
            int insertPosition = 0;
            for (insertPosition = 0; insertPosition < (int)colorPoints.size(); insertPosition++) {
                if (normalizedPosition < colorPoints.at(insertPosition).position
                    || insertPosition == colorPoints.size()-1) {
                    break;
                }
            }

            // Add new color point
            float newPosition = normalizedPosition;
            sgl::Color newColor = sgl::colorLerp(
                    colorPoints.at(insertPosition-1).color,
                    colorPoints.at(insertPosition).color,
                    1.0 - (colorPoints.at(insertPosition).position - newPosition)
                          / (colorPoints.at(insertPosition).position - colorPoints.at(insertPosition-1).position));
            colorPoints.insert(colorPoints.begin() + insertPosition, ColorPoint(newColor, newPosition));
            currentSelectionIndex = insertPosition;
            colorSelection = ImColor(colorPoints.at(currentSelectionIndex).color.getColorRGB());
            selectedPointType = SELECTED_POINT_TYPE_COLOR;
            reRender = true;
        }
    }

    rebuildTransferFunctionMap();
}

void TransferFunctionWindow::dragPoint()
{
    if (mouseReleased) {
        dragging = false;
    }

    glm::vec2 mousePosWidget = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y) - opacityGraphBox.min;
    if (!dragging || mousePosWidget == oldMousePosWidget) {
        oldMousePosWidget = mousePosWidget;
        return;
    }
    oldMousePosWidget = mousePosWidget;

    if (selectedPointType == SELECTED_POINT_TYPE_OPACITY) {
        glm::vec2 normalizedPosition = mousePosWidget / opacityGraphBox.getDimensions();
        normalizedPosition.y = 1.0f - normalizedPosition.y;
        normalizedPosition = glm::clamp(normalizedPosition, 0.0f, 1.0f);
        // Clip to neighbors!
        if (normalizedPosition.x < opacityPoints.at(currentSelectionIndex-1).position) {
            normalizedPosition.x = opacityPoints.at(currentSelectionIndex-1).position;
        }
        if (normalizedPosition.x > opacityPoints.at(currentSelectionIndex+1).position) {
            normalizedPosition.x = opacityPoints.at(currentSelectionIndex+1).position;
        }
        opacityPoints.at(currentSelectionIndex).position = normalizedPosition.x;
        opacityPoints.at(currentSelectionIndex).opacity = normalizedPosition.y;
    }

    if (selectedPointType == SELECTED_POINT_TYPE_COLOR) {
        float normalizedPosition = mousePosWidget.x / opacityGraphBox.getWidth();
        normalizedPosition = glm::clamp(normalizedPosition, 0.0f, 1.0f);
        // Sort if necessary
        while (normalizedPosition < colorPoints.at(currentSelectionIndex-1).position) {
            ColorPoint tmp = colorPoints.at(currentSelectionIndex-1);
            colorPoints.at(currentSelectionIndex-1) = colorPoints.at(currentSelectionIndex);
            colorPoints.at(currentSelectionIndex) = tmp;
            currentSelectionIndex -= 1;
        }
        while (normalizedPosition > colorPoints.at(currentSelectionIndex+1).position) {
            ColorPoint tmp = colorPoints.at(currentSelectionIndex+1);
            colorPoints.at(currentSelectionIndex+1) = colorPoints.at(currentSelectionIndex);
            colorPoints.at(currentSelectionIndex) = tmp;
            currentSelectionIndex += 1;
        }
        colorPoints.at(currentSelectionIndex).position = normalizedPosition;
    }

    rebuildTransferFunctionMap();
    reRender = true;
}

bool TransferFunctionWindow::selectNearestOpacityPoint(int &currentSelectionIndex, const glm::vec2 &mousePosWidget)
{
    for (int i = 0; i < (int)opacityPoints.size(); i++) {
        glm::vec2 centerPt = glm::vec2(opacityPoints.at(i).position * opacityGraphBox.getWidth(),
                (1.0f - opacityPoints.at(i).opacity) * opacityGraphBox.getHeight());
        if (glm::length(centerPt - mousePosWidget) < 10.0f) {
            currentSelectionIndex = i;
            return true;
        }
    }
    return false;
}

bool TransferFunctionWindow::selectNearestColorPoint(int &currentSelectionIndex, const glm::vec2 &mousePosWidget)
{
    for (int i = 0; i < (int)colorPoints.size(); i++) {
        ImVec2 centerPt = ImVec2(colorPoints.at(i).position * colorBarBox.getWidth(),
                colorBarBox.getHeight()/2);
        if (glm::abs(centerPt.x - mousePosWidget.x) < 10.0f) {
            currentSelectionIndex = i;
            return true;
        }
    }
    return false;
}
