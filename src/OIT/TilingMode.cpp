//
// Created by christoph on 27.09.18.
//

#include "TilingMode.hpp"
#include <Utils/Convert.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <ImGui/ImGuiWrapper.hpp>

static int modeIndex = 2;
static int tileWidth = 2;
static int tileHeight = 8;

bool selectTilingModeUI()
{
    const char *indexingModeNames[] = { "1x1", "2x2", "2x8", "8x2", "4x4", "8x8", "8x8 Morton Code" };
    if (ImGui::Combo("Tiling Mode", (int*)&modeIndex, indexingModeNames, IM_ARRAYSIZE(indexingModeNames))) {
        // Reset mode
        sgl::ShaderManager->invalidateShaderCache();
        sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_2x2");
        sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_2x8");
        sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_NxM");
        sgl::ShaderManager->removePreprocessorDefine("ADRESSING_MORTON_CODE_8x8");

        // Select new mode
        if (modeIndex == 0) {
            // No tiling
            tileWidth = 1;
            tileHeight = 1;
        } else if (modeIndex == 1) {
            tileWidth = 2;
            tileHeight = 2;
            sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_2x2", "");
        } else if (modeIndex == 2) {
            tileWidth = 2;
            tileHeight = 8;
            sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_2x8", "");
        } else if (modeIndex == 3) {
            tileWidth = 8;
            tileHeight = 2;
            sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
        } else if (modeIndex == 4) {
            tileWidth = 4;
            tileHeight = 4;
            sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
        } else if (modeIndex == 5) {
            tileWidth = 8;
            tileHeight = 8;
            sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
        } else if (modeIndex == 6) {
            tileWidth = 8;
            tileHeight = 8;
            sgl::ShaderManager->addPreprocessorDefine("ADRESSING_MORTON_CODE_8x8", "");
        }

        sgl::ShaderManager->addPreprocessorDefine("TILE_N", sgl::toString(tileWidth));
        sgl::ShaderManager->addPreprocessorDefine("TILE_M", sgl::toString(tileHeight));

        return true;
    }
    return false;
}

void setNewTilingMode(int newTileWidth, int newTileHeight, bool useMortonCode /* = false */)
{
    tileWidth = newTileWidth;
    tileHeight = newTileHeight;

    // Reset mode
    sgl::ShaderManager->invalidateShaderCache();
    sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_2x2");
    sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_2x8");
    sgl::ShaderManager->removePreprocessorDefine("ADDRESSING_TILED_NxM");
    sgl::ShaderManager->removePreprocessorDefine("ADRESSING_MORTON_CODE_8x8");

    // Select new mode
    if (tileWidth == 1 && tileHeight == 1) {
        // No tiling
        modeIndex = 0;
    } else if (tileWidth == 2 && tileHeight == 2) {
        modeIndex = 1;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_2x2", "");
    } else if (tileWidth == 2 && tileHeight == 8) {
        modeIndex = 2;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_2x8", "");
    } else if (tileWidth == 8 && tileHeight == 2) {
        modeIndex = 3;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
    } else if (tileWidth == 4 && tileHeight == 4) {
        modeIndex = 4;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
    } else if (tileWidth == 8 && tileHeight == 8 && !useMortonCode) {
        modeIndex = 5;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
    } else if (tileWidth == 8 && tileHeight == 8 && useMortonCode) {
        modeIndex = 6;
        sgl::ShaderManager->addPreprocessorDefine("ADRESSING_MORTON_CODE_8x8", "");
    } else {
        // Invalid mode, just set to mode 5, too.
        modeIndex = 5;
        sgl::ShaderManager->addPreprocessorDefine("ADDRESSING_TILED_NxM", "");
    }

    sgl::ShaderManager->addPreprocessorDefine("TILE_N", sgl::toString(tileWidth));
    sgl::ShaderManager->addPreprocessorDefine("TILE_M", sgl::toString(tileHeight));
}

void getScreenSizeWithTiling(int &screenWidth, int &screenHeight)
{
    if (screenWidth % tileWidth != 0) {
        screenWidth = (screenWidth / tileWidth + 1) * tileWidth;
    }
    if (screenHeight % tileHeight != 0) {
        screenHeight = (screenHeight / tileHeight + 1) * tileHeight;
    }
}
