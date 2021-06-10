//============================================================================
// Name        : PixelSync.cpp
// Author      : Christoph Neuhauser
// Copyright   : GPLv3
//============================================================================

#include <iostream>
#include <SDL2/SDL.h>
#include <Utils/File/FileUtils.hpp>
#include <Utils/AppSettings.hpp>
#include <Graphics/Window.hpp>

#include "MainApp.hpp"

using namespace std;
using namespace sgl;

int main(int argc, char *argv[]) {
    // Initialize the filesystem utilities
    FileUtils::get()->initialize("pixel-sync-oit", argc, argv);

    // Load the file containing the app settings
    string settingsFile = FileUtils::get()->getConfigDirectory() + "settings.txt";
    AppSettings::get()->loadSettings(settingsFile.c_str());
    AppSettings::get()->getSettings().addKeyValue("window-multisamples", 0);
    AppSettings::get()->getSettings().addKeyValue("window-debugContext", true);
    AppSettings::get()->getSettings().addKeyValue("window-vSync", true);
    AppSettings::get()->getSettings().addKeyValue("window-resizable", true);
    AppSettings::get()->setLoadGUI();

    AppSettings::get()->createWindow();
    AppSettings::get()->initializeSubsystems();

    AppLogic *app = new PixelSyncApp();
    app->run();
    delete app;

    AppSettings::get()->release();

    return 0;
}
