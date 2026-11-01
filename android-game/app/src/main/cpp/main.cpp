#include "audio.h"
#include "game.h"
#include "renderer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr const char* kLogTag = "IMISUBEBE";
constexpr int kControlCount = 18;
constexpr int kMaxPointers = 10;

struct TouchPoint {
    int32_t id = -1;
    float x = 0.0f;
    float y = 0.0f;
    bool active = false;
};

struct Engine {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    int width = 0;
    int height = 0;
    bool animating = false;
    bool mapLoaded = false;
    std::string mapError;
    Game game;
    Renderer renderer;
    AudioEngine audio;
    std::array<TouchPoint, kMaxPointers> touches{};
    std::array<bool, kControlCount> previousControlDown{};
    bool keyGas = false;
    bool keyBrake = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyReverse = false;
    bool keyHorn = false;
    bool keyStart = false;
    bool keyPause = false;
    bool keyCamera = false;
    bool keySettings = false;
    float freeCameraDx = 0.0f;
    float freeCameraDy = 0.0f;
    int savedCoins = 0;
    std::chrono::steady_clock::time_point previousFrame{};
};

int controlIndex(ControlId id) {
    return static_cast<int>(id);
}

void logError(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", message);
}

bool loadBundledMap(Engine& engine) {
    AAsset* asset = AAssetManager_open(engine.app->activity->assetManager,
                                       "osm_cebu.osm", AASSET_MODE_BUFFER);
    if (!asset) {
        engine.mapError = "The offline OpenStreetMap asset is missing.";
        return false;
    }
    const off_t length = AAsset_getLength(asset);
    if (length <= 0 || length > 4 * 1024 * 1024) {
        AAsset_close(asset);
        engine.mapError = "The offline OpenStreetMap file has an invalid size.";
        return false;
    }
    std::string xml(static_cast<std::size_t>(length), '\0');
    const int read = AAsset_read(asset, xml.data(), static_cast<size_t>(length));
    AAsset_close(asset);
    if (read != length) {
        engine.mapError = "The offline OpenStreetMap file could not be read.";
        return false;
    }
    return engine.game.loadMap(xml, &engine.mapError);
}

int loadCoinSave(android_app* app) {
    const std::string path =
        std::string(app->activity->internalDataPath) + "/coins.dat";
    std::ifstream input(path);
    int coins = 0;
    input >> coins;
    return std::max(0, coins);
}

void saveCoinCount(android_app* app, int coins) {
    const std::string path =
        std::string(app->activity->internalDataPath) + "/coins.dat";
    const std::string temporary = path + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) return;
        output << std::max(0, coins) << '\n';
        output.flush();
        if (!output) return;
    }
    std::remove(path.c_str());
    std::rename(temporary.c_str(), path.c_str());
}

void clearEgl(Engine& engine) {
    if (engine.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine.surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine.display, engine.surface);
        }
        if (engine.context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine.display, engine.context);
        }
        eglTerminate(engine.display);
    }
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;
    engine.width = 0;
    engine.height = 0;
    engine.animating = false;
}

bool initializeEgl(Engine& engine) {
    if (!engine.app->window) return false;
    engine.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (engine.display == EGL_NO_DISPLAY) {
        logError("eglGetDisplay failed.");
        clearEgl(engine);
        return false;
    }
    if (eglInitialize(engine.display, nullptr, nullptr) != EGL_TRUE) {
        logError("eglInitialize failed.");
        clearEgl(engine);
        return false;
    }
    const EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE,
    };
    EGLint configCount = 0;
    if (eglChooseConfig(engine.display, attributes, &engine.config, 1,
                        &configCount) != EGL_TRUE || configCount < 1) {
        logError("No OpenGL ES 3 EGL configuration was found.");
        clearEgl(engine);
        return false;
    }
    EGLint format = 0;
    eglGetConfigAttrib(engine.display, engine.config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine.app->window, 0, 0, format);
    engine.surface = eglCreateWindowSurface(engine.display, engine.config,
                                            engine.app->window, nullptr);
    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    engine.context =
        eglCreateContext(engine.display, engine.config, EGL_NO_CONTEXT,
                         contextAttributes);
    if (engine.surface == EGL_NO_SURFACE || engine.context == EGL_NO_CONTEXT ||
        eglMakeCurrent(engine.display, engine.surface, engine.surface,
                       engine.context) != EGL_TRUE) {
        logError("Could not create the OpenGL ES 3 window context.");
        clearEgl(engine);
        return false;
    }
    eglQuerySurface(engine.display, engine.surface, EGL_WIDTH, &engine.width);
    eglQuerySurface(engine.display, engine.surface, EGL_HEIGHT, &engine.height);
    engine.animating = true;
    if (!engine.renderer.initialize(engine.game.map())) {
        logError("The OpenGL ES renderer failed to initialize.");
    }
    return true;
}

TouchPoint* findTouch(Engine& engine, int32_t id) {
    for (TouchPoint& touch : engine.touches) {
        if (touch.active && touch.id == id) return &touch;
    }
    return nullptr;
}

TouchPoint* allocateTouch(Engine& engine, int32_t id) {
    if (TouchPoint* existing = findTouch(engine, id)) return existing;
    for (TouchPoint& touch : engine.touches) {
        if (!touch.active) {
            touch.id = id;
            touch.active = true;
            return &touch;
        }
    }
    return nullptr;
}

int32_t eventPointerId(const AInputEvent* event, size_t index) {
    return AMotionEvent_getPointerId(event, index);
}

void setTouchFromEvent(Engine& engine, const AInputEvent* event, size_t index,
                       bool trackCameraMotion = false) {
    TouchPoint* touch = allocateTouch(engine, eventPointerId(event, index));
    if (!touch) return;
    const float x = AMotionEvent_getX(event, index);
    const float y = AMotionEvent_getY(event, index);
    if (trackCameraMotion && engine.game.state().cameraMode == 3 &&
        engine.width > 0 && engine.height > 0) {
        const float dx = x - touch->x;
        const float dy = y - touch->y;
        if (std::abs(dx) + std::abs(dy) > 1.0f) {
            engine.freeCameraDx += dx;
            engine.freeCameraDy += dy;
        }
    }
    touch->x = x;
    touch->y = y;
}

int32_t handleInput(android_app* app, AInputEvent* event) {
    auto* engine = static_cast<Engine*>(app->userData);
    if (!engine) return 0;
    const int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int action = AMotionEvent_getAction(event);
        const int masked = action & AMOTION_EVENT_ACTION_MASK;
        const size_t pointerIndex =
            static_cast<size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        const size_t count = AMotionEvent_getPointerCount(event);
        if (masked == AMOTION_EVENT_ACTION_DOWN ||
            masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            setTouchFromEvent(*engine, event, std::min(pointerIndex, count - 1));
        } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
            for (size_t i = 0; i < count; ++i) {
                setTouchFromEvent(*engine, event, i, true);
            }
        } else if (masked == AMOTION_EVENT_ACTION_UP ||
                   masked == AMOTION_EVENT_ACTION_POINTER_UP ||
                   masked == AMOTION_EVENT_ACTION_CANCEL) {
            if (masked == AMOTION_EVENT_ACTION_CANCEL) {
                for (TouchPoint& touch : engine->touches) touch.active = false;
            } else if (count > 0) {
                TouchPoint* touch =
                    findTouch(*engine, eventPointerId(event, std::min(pointerIndex, count - 1)));
                if (touch) touch->active = false;
            }
        }
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t code = AKeyEvent_getKeyCode(event);
        const bool down = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN;
        switch (code) {
            case AKEYCODE_W:
            case AKEYCODE_DPAD_UP: engine->keyGas = down; return 1;
            case AKEYCODE_S:
            case AKEYCODE_DPAD_DOWN: engine->keyBrake = down; return 1;
            case AKEYCODE_A:
            case AKEYCODE_DPAD_LEFT: engine->keyLeft = down; return 1;
            case AKEYCODE_D:
            case AKEYCODE_DPAD_RIGHT: engine->keyRight = down; return 1;
            case AKEYCODE_R:
                if (down && !engine->keyReverse) engine->keyReverse = true;
                return 1;
            case AKEYCODE_SPACE: engine->keyHorn = down; return 1;
            case AKEYCODE_C:
                if (down) engine->keyCamera = true;
                return 1;
            case AKEYCODE_M:
                if (down) engine->keySettings = true;
                return 1;
            case AKEYCODE_ENTER:
            case AKEYCODE_NUMPAD_ENTER:
                if (down) engine->keyStart = true;
                return 1;
            case AKEYCODE_BACK:
            case AKEYCODE_ESCAPE:
                if (down) engine->keyPause = true;
                return 1;
            default: return 0;
        }
    }
    return 0;
}

bool isTouching(const Engine& engine, const UiLayout& layout, ControlId id) {
    const ControlRect* rectangle = layout.control(id);
    if (!rectangle) return false;
    for (const TouchPoint& touch : engine.touches) {
        if (touch.active && touch.x >= rectangle->left && touch.x <= rectangle->right &&
            touch.y >= rectangle->top && touch.y <= rectangle->bottom) {
            return true;
        }
    }
    return false;
}

bool pressEdge(Engine& engine, ControlId id, bool down) {
    const int index = controlIndex(id);
    if (index < 0 || index >= kControlCount) return false;
    const bool pressed = down && !engine.previousControlDown[index];
    engine.previousControlDown[index] = down;
    return pressed;
}

InputState collectInput(Engine& engine) {
    InputState input;
    const GameState& state = engine.game.state();
    const UiLayout layout = engine.renderer.layout(engine.width, engine.height);
    const auto hit = [&](ControlId id) { return isTouching(engine, layout, id); };
    if (!state.mapReady) return input;

    if (state.settingsOpen) {
        input.dayNightPressed = pressEdge(engine, ControlId::DayNight,
                                          hit(ControlId::DayNight));
        input.rainPressed =
            pressEdge(engine, ControlId::Rain, hit(ControlId::Rain));
        input.closeSettingsPressed =
            pressEdge(engine, ControlId::CloseSettings, hit(ControlId::CloseSettings));
    } else if (!state.started) {
        input.startPressed = pressEdge(engine, ControlId::Start, hit(ControlId::Start)) ||
                             engine.keyStart;
        input.settingsPressed =
            pressEdge(engine, ControlId::MenuSettings, hit(ControlId::MenuSettings)) ||
            engine.keySettings;
    } else if (state.paused) {
        input.resumePressed = pressEdge(engine, ControlId::Resume, hit(ControlId::Resume));
        input.restartPressed =
            pressEdge(engine, ControlId::Restart, hit(ControlId::Restart));
        input.settingsPressed =
            pressEdge(engine, ControlId::MenuSettings, hit(ControlId::MenuSettings)) ||
            engine.keySettings;
    } else if (state.phase == MissionPhase::Complete) {
        input.replayPressed = pressEdge(engine, ControlId::Replay, hit(ControlId::Replay));
        input.settingsPressed =
            pressEdge(engine, ControlId::Settings, hit(ControlId::Settings)) ||
            engine.keySettings;
        input.pausePressed = pressEdge(engine, ControlId::Pause, hit(ControlId::Pause)) ||
                             engine.keyPause;
    } else {
        input.gas = hit(ControlId::Gas) || engine.keyGas;
        input.brake = hit(ControlId::Brake) || engine.keyBrake;
        input.left = hit(ControlId::Left) || engine.keyLeft;
        input.right = hit(ControlId::Right) || engine.keyRight;
        input.reversePressed =
            pressEdge(engine, ControlId::Reverse, hit(ControlId::Reverse)) ||
            engine.keyReverse;
        input.horn = hit(ControlId::Horn) || engine.keyHorn;
        input.actionPressed = pressEdge(engine, ControlId::Action, hit(ControlId::Action));
        input.cameraPressed =
            pressEdge(engine, ControlId::Camera, hit(ControlId::Camera)) ||
            engine.keyCamera;
        input.pausePressed = pressEdge(engine, ControlId::Pause, hit(ControlId::Pause)) ||
                             engine.keyPause;
        input.settingsPressed =
            pressEdge(engine, ControlId::Settings, hit(ControlId::Settings)) ||
            engine.keySettings;
        if (state.cameraMode == 3) {
            input.freeCameraDeltaX = engine.freeCameraDx;
            input.freeCameraDeltaY = engine.freeCameraDy;
        }
    }
    engine.keyStart = false;
    engine.keyPause = false;
    engine.keyCamera = false;
    engine.keySettings = false;
    engine.keyReverse = false;
    engine.freeCameraDx = 0.0f;
    engine.freeCameraDy = 0.0f;
    return input;
}

void handleCommand(android_app* app, int32_t command) {
    auto* engine = static_cast<Engine*>(app->userData);
    if (!engine) return;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) initializeEgl(*engine);
            break;
        case APP_CMD_TERM_WINDOW:
            if (engine->display != EGL_NO_DISPLAY &&
                engine->context != EGL_NO_CONTEXT) {
                eglMakeCurrent(engine->display, engine->surface, engine->surface,
                               engine->context);
                engine->renderer.release();
            }
            clearEgl(*engine);
            break;
        case APP_CMD_PAUSE:
            {
                InputState input;
                input.pausePressed = true;
                engine->game.update(input, 0.016f);
            }
            engine->audio.update(0.0f, 0.0f, false);
            break;
        case APP_CMD_LOST_FOCUS:
            engine->audio.update(0.0f, 0.0f, false);
            break;
        case APP_CMD_RESUME:
            break;
        case APP_CMD_DESTROY:
            engine->audio.stop();
            break;
        default:
            break;
    }
}
}  // namespace

void android_main(android_app* app) {
    Engine engine;
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = handleCommand;
    app->onInputEvent = handleInput;

    engine.mapLoaded = loadBundledMap(engine);
    if (!engine.mapLoaded) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "Offline map load failed: %s", engine.mapError.c_str());
    } else {
        engine.savedCoins = loadCoinSave(app);
        engine.game.setCoins(engine.savedCoins);
        engine.audio.start();
    }

    engine.previousFrame = std::chrono::steady_clock::now();
    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        const int timeout = engine.animating ? 12 : -1;
        const int result = ALooper_pollOnce(timeout, nullptr, &events,
                                            reinterpret_cast<void**>(&source));
        if (result == ALOOPER_POLL_ERROR) break;
        if (source) source->process(app, source);
        if (app->destroyRequested != 0) break;

        if (!engine.animating || engine.display == EGL_NO_DISPLAY ||
            engine.surface == EGL_NO_SURFACE) {
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - engine.previousFrame).count();
        engine.previousFrame = now;
        const InputState input = collectInput(engine);
        engine.game.update(input, dt);
        const int coins = engine.game.state().coins;
        if (coins != engine.savedCoins) {
            saveCoinCount(app, coins);
            engine.savedCoins = coins;
        }
        const GameState& state = engine.game.state();
        engine.audio.update(state.speed, state.throttle, input.horn && state.started);
        engine.renderer.draw(engine.game.map(), state, engine.width, engine.height);
        if (eglSwapBuffers(engine.display, engine.surface) != EGL_TRUE) {
            const EGLint error = eglGetError();
            if (error == EGL_CONTEXT_LOST) {
                engine.renderer.release();
                clearEgl(engine);
            }
        }
    }
    engine.audio.stop();
    if (engine.display != EGL_NO_DISPLAY &&
        engine.context != EGL_NO_CONTEXT) {
        eglMakeCurrent(engine.display, engine.surface, engine.surface, engine.context);
        engine.renderer.release();
    }
    clearEgl(engine);
}
