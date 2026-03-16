#include <sgg/graphics.h>

#include <string>

namespace {

struct AppState {
    float elapsed_ms = 0.0f;
};

void draw() {
    auto* app = static_cast<AppState*>(graphics::getUserData());

    graphics::Brush rect;
    rect.fill_color[0] = 0.2f;
    rect.fill_color[1] = 0.6f;
    rect.fill_color[2] = 0.9f;
    rect.fill_opacity = 0.8f;
    rect.outline_opacity = 0.0f;

    graphics::drawRect(50.0f, 35.0f, 70.0f, 30.0f, rect);

    graphics::Brush text;
    text.fill_color[0] = 1.0f;
    text.fill_color[1] = 1.0f;
    text.fill_color[2] = 1.0f;

    const std::string msg = "SGG smoke test (ESC to quit) | t=" + std::to_string(static_cast<int>(app ? app->elapsed_ms : 0.0f));
    graphics::drawText(2.0f, 6.0f, 4.0f, msg, text);
}

void update(float ms) {
    auto* app = static_cast<AppState*>(graphics::getUserData());
    if (app) app->elapsed_ms += ms;
}

} 

int main() {
    AppState app;

    graphics::createWindow(900, 700, "GridWorld - SGG smoke");

    graphics::Brush bg;
    bg.fill_color[0] = 0.08f;
    bg.fill_color[1] = 0.08f;
    bg.fill_color[2] = 0.10f;
    graphics::setWindowBackground(bg);

    graphics::setCanvasSize(100.0f, 75.0f);
    graphics::setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);

    graphics::setUserData(&app);
    graphics::setDrawFunction(draw);
    graphics::setUpdateFunction(update);

    graphics::startMessageLoop();

    graphics::setUserData(nullptr);
    graphics::destroyWindow();
    return 0;
}
