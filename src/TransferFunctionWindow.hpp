//
// Created by christoph on 09.10.18.
//

#ifndef PIXELSYNCOIT_TRANSFERFUNCTIONWINDOW_HPP
#define PIXELSYNCOIT_TRANSFERFUNCTIONWINDOW_HPP

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <Math/Geometry/AABB2.hpp>
#include <Graphics/Color.hpp>
#include <Graphics/Buffers/GeometryBuffer.hpp>
#include <ImGui/ImGuiWrapper.hpp>

struct ColorPoint
{
    ColorPoint(const sgl::Color &color, float position) : color(color), position(position) {}
    sgl::Color color;
    float position;
};

struct OpacityPoint
{
    OpacityPoint(float opacity, float position) : opacity(opacity), position(position) {}
    float opacity;
    float position;
};

enum SelectedPointType {
    SELECTED_POINT_TYPE_NONE, SELECTED_POINT_TYPE_OPACITY, SELECTED_POINT_TYPE_COLOR
};

/** Stores color and opacity points and renders the GUI.
 *
 */
class TransferFunctionWindow
{
public:
    TransferFunctionWindow();
    bool saveFunctionToFile(const std::string &filename);
    bool loadFunctionFromFile(const std::string &filename);
    void updateAvailableFiles();

    bool renderGUI();
    void update(float dt);

    void setClearColor(const sgl::Color &clearColor);
    inline bool &getShowTransferFunctionWindow() { return showTransferFunctionWindow; }

    // For OpenGL: Has 256 entries. Get mapped color for normalized attribute by accessing entry at "attr*255".
    std::vector<sgl::Color> getTransferFunctionMap();
    sgl::GeometryBufferPtr &getTransferFunctionMapUBO();

private:
    void renderFileDialog();
    void renderOpacityGraph();
    void renderColorBar();

    // Drag-and-drop functions
    void onOpacityGraphClick();
    void onColorBarClick();
    void dragPoint();
    bool selectNearestOpacityPoint(int &currentSelectionIndex, const glm::vec2 &mousePosWidget);
    bool selectNearestColorPoint(int &currentSelectionIndex, const glm::vec2 &mousePosWidget);

    // Drag-and-drop data
    SelectedPointType selectedPointType = SELECTED_POINT_TYPE_NONE;
    bool dragging = false;
    bool mouseReleased = false;
    int currentSelectionIndex = 0;
    sgl::AABB2 opacityGraphBox, colorBarBox;
    glm::vec2 oldMousePosWidget;

    // GUI
    bool reRender = false;
    bool showTransferFunctionWindow = true;
    float opacitySelection = 1.0f;
    ImVec4 colorSelection = ImColor(255, 255, 255, 255);
    sgl::Color clearColor;
    std::string saveDirectory = "Data/TransferFunctions/";
    std::string saveFileString = "WhiteRed.xml";
    std::vector<std::string> availableFiles;
    int selectedFileIndex = -1;

    void rebuildTransferFunctionMap();
    std::vector<sgl::Color> transferFunctionMap;
    sgl::GeometryBufferPtr tfMapBuffer;

    std::vector<OpacityPoint> opacityPoints;
    std::vector<ColorPoint> colorPoints;
};


#endif //PIXELSYNCOIT_TRANSFERFUNCTIONWINDOW_HPP
