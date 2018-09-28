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
void setNewTilingMode(int newTileWidth, int newTileHeight);

#endif //PIXELSYNCOIT_TILINGMODE_HPP
