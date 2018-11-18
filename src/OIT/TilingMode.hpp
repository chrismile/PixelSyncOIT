//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_TILINGMODE_HPP
#define PIXELSYNCOIT_TILINGMODE_HPP

/**
 * Uses ImGui to render a tiling mode selection window.
 * @return True if a new tiling mode was selected (shaders need to be reloaded in this case).
 */
bool selectTilingModeUI();

/// For e.g. the automatic performance measuring tool.
/// Morton code for now only supported for 8x8 tile size.
void setNewTilingMode(int newTileWidth, int newTileHeight, bool useMortonCode = false);

/// Returns screen width and screen height padded for tile size
void getScreenSizeWithTiling(int &screenWidth, int &screenHeight);

#endif //PIXELSYNCOIT_TILINGMODE_HPP
