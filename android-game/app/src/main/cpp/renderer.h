#pragma once

#include "game.h"

#include <GLES3/gl3.h>

#include <string>
#include <vector>

enum class ControlId {
    Gas,
    Brake,
    Left,
    Right,
    Reverse,
    Action,
    Camera,
    Horn,
    Settings,
    Pause,
    Start,
    MenuSettings,
    Resume,
    Restart,
    Replay,
    DayNight,
    Rain,
    CloseSettings,
};

struct ControlRect {
    ControlId id;
    float left;
    float top;
    float right;
    float bottom;
};

struct RenderVertex {
    float x, y, z;
    float r, g, b;
};

struct UiLayout {
    std::vector<ControlRect> controls;
    ControlRect minimap;
    ControlRect mission;
    ControlRect speed;
    ControlRect coins;
    bool portrait = false;
    float find(ControlId id) const;
    const ControlRect* control(ControlId id) const;
};

UiLayout makeUiLayout(int width, int height);

class Renderer {
public:
    bool initialize(const MapData& map);
    void release();
    void draw(const MapData& map, const GameState& game, int width, int height);
    UiLayout layout(int width, int height) const;

private:
    GLuint worldProgram_ = 0;
    GLuint hudProgram_ = 0;
    GLuint worldVbo_ = 0;
    GLuint hudVbo_ = 0;
    GLuint staticVertexCount_ = 0;
    std::vector<float> staticVertices_;
    std::vector<RenderVertex> dynamicVertices_;
    std::vector<float> packedDynamicVertices_;
    bool ready_ = false;
};
