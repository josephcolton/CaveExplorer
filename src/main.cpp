#include "core/App.h"

#include <SDL3/SDL_main.h>

#include <string>

int main(int argc, char* argv[]) {
    App::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (arg == "--area") options.startArea = next();
        else if (arg == "--spawn") options.startSpawn = next();
        else if (arg == "--shot") options.screenshotPath = next();
        else if (arg == "--ticks") options.screenshotTicks = std::stoi(next());
        else if (arg == "--script") options.script = next();
        else if (arg == "--debug") options.debug = true;
        else if (arg == "--give") options.giveItems = next();
        else if (arg == "--ui") options.openUi = next();
        else if (arg == "--hunger") options.hunger = std::stof(next());
    }

    App app;
    if (!app.init(options)) {
        app.shutdown();
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
