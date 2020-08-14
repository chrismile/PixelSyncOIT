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
#include <Graphics/Texture/Texture.hpp>
#include <ImGui/ImGuiWrapper.hpp>

enum ColorSpace {
    COLOR_SPACE_SRGB, COLOR_SPACE_LINEAR_RGB
};
const char *const COLOR_SPACE_NAMES[] {
    "sRGB", "Linear RGB"
};

/**
 * A color point stores sRGB color values.
 */
struct ColorPoint_sRGB
{
    ColorPoint_sRGB(const sgl::Color &color, float position) : color(color), position(position) {}
    sgl::Color color;
    float position;
};

struct ColorPoint_LinearRGB
{
    ColorPoint_LinearRGB(const glm::vec3 &color, float position) : color(color), position(position) {}
    glm::vec3 color;
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
    void setMultiVarMode(bool enabled) { multiVarMode = enabled; }

    bool renderGUI();
//    bool renderMultiVarGUI(const std::string& varNames);
    void update(float dt);

    void setClearColor(const sgl::Color &clearColor);
    void setShow(const bool showWindow) { showTransferFunctionWindow = showWindow; }
    inline bool &getShowTransferFunctionWindow() { return showTransferFunctionWindow; }
    void computeHistogram(const std::vector<float> &attributes, float minAttr, float maxAttr);
    void setUseLinearRGB(bool useLinearRGB);

    // For querying transfer function in application
    float getOpacityAtAttribute(float attribute); // attribute: Between 0 and 1
    const std::vector<sgl::Color> &getTransferFunctionMap_sRGB() { return transferFunctionMap_sRGB; }

    // For OpenGL: Has 256 entries. Get mapped color for normalized attribute by accessing entry at "attr*255".
    sgl::TexturePtr &getTransferFunctionMapTexture();
    bool getTransferFunctionMapRebuilt();

    // For ray tracing interface
    inline const std::vector<OpacityPoint> &getOpacityPoints() { return opacityPoints; }
    inline const std::vector<ColorPoint_sRGB> &getColorPoints_sRGB() { return colorPoints; }
    inline const std::vector<ColorPoint_LinearRGB> &getColorPoints_LinearRGB() { return colorPoints_LinearRGB; }

    // sRGB and linear RGB conversion
    static glm::vec3 sRGBToLinearRGB(const glm::vec3 &color_LinearRGB);
    static glm::vec3 linearRGBTosRGB(const glm::vec3 &color_sRGB);

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
    ColorSpace interpolationColorSpace;

    std::string saveDirectory = "Data/TransferFunctions/";
    std::string saveFileString = "WhiteRed.xml";
    std::vector<std::string> availableFiles;
    int selectedFileIndex = -1;
    std::vector<float> histogram;
    void rebuildTransferFunctionMap();
    void rebuildTransferFunctionMap_sRGB();
    void rebuildTransferFunctionMap_LinearRGB();
    std::vector<sgl::Color> transferFunctionMap_sRGB;
    std::vector<sgl::Color> transferFunctionMap_linearRGB;
    sgl::TexturePtr tfMapTexture;
    sgl::TextureSettings tfMapTextureSettings;

    std::vector<OpacityPoint> opacityPoints;
    std::vector<ColorPoint_sRGB> colorPoints;
    std::vector<ColorPoint_LinearRGB> colorPoints_LinearRGB;
    bool useLinearRGB = true;
    bool transferFunctionMapRebuilt = true;
    bool multiVarMode = false;
};

extern TransferFunctionWindow *g_TransferFunctionWindowHandle;


#endif //PIXELSYNCOIT_TRANSFERFUNCTIONWINDOW_HPP
