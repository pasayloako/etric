#pragma once

#include "map_data.h"

#include <string>
#include <vector>

enum class MissionPhase {
    ToPickup,
    ToDestination,
    Complete,
};

struct InputState {
    bool gas = false;
    bool brake = false;
    bool left = false;
    bool right = false;
    bool reverse = false;
    bool reversePressed = false;
    bool horn = false;
    bool actionPressed = false;
    bool cameraPressed = false;
    bool pausePressed = false;
    bool settingsPressed = false;
    bool startPressed = false;
    bool resumePressed = false;
    bool restartPressed = false;
    bool replayPressed = false;
    bool dayNightPressed = false;
    bool rainPressed = false;
    bool closeSettingsPressed = false;
    float freeCameraDeltaX = 0.0f;
    float freeCameraDeltaY = 0.0f;
};

struct TrafficActor {
    Vec2 position;
    float heading = 0.0f;
    float progress = 0.0f;
    float speed = 0.0f;
    float sideOffset = 0.0f;
    float collisionRadius = 1.2f;
    std::size_t roadIndex = 0;
    unsigned kind = 0;
    unsigned colorIndex = 0;
    int direction = 1;
};

struct GameState {
    bool mapReady = false;
    bool started = false;
    bool paused = false;
    bool settingsOpen = false;
    bool rain = false;
    bool passengerOnboard = false;
    bool reverseGear = false;
    float elapsedSeconds = 0.0f;
    float timeOfDay = 14.0f;
    float speed = 0.0f;
    float steering = 0.0f;
    float suspension = 0.0f;
    float bodyRoll = 0.0f;
    float wheelRotation = 0.0f;
    float throttle = 0.0f;
    float heading = 0.0f;
    float freeCameraYaw = 0.0f;
    float freeCameraPitch = 0.35f;
    float distanceToGoal = 0.0f;
    float distanceToPickup = 0.0f;
    float distanceToDestination = 0.0f;
    float toastSeconds = 0.0f;
    Vec2 vehiclePosition;
    Vec2 passengerPosition;
    Vec2 pickupPosition;
    Vec2 destinationPosition;
    MissionPhase phase = MissionPhase::ToPickup;
    int coins = 0;
    int cameraMode = 0;  // rear, front, driver, free
    std::string missionText;
    std::string passengerText = "WAITING";
    std::string navigationText;
    std::string toast;
    std::vector<Vec2> route;
    std::vector<TrafficActor> traffic;
};

class Game {
public:
    bool loadMap(const std::string& osmXml, std::string* error = nullptr);
    void setCoins(int coins);
    void update(const InputState& input, float deltaSeconds);

    const GameState& state() const { return state_; }
    const MapData& map() const { return map_; }
    bool mapReady() const { return state_.mapReady; }

private:
    void startMission();
    void resetMission();
    void doMissionAction();
    void showToast(const std::string& message, float seconds = 2.5f);
    void refreshNavigation();
    void updateTraffic(float deltaSeconds);
    void updateVehicle(const InputState& input, float deltaSeconds);
    void updateMissionHud();
    void seedTraffic();
    void advanceTrafficActor(TrafficActor& actor, float deltaSeconds);

    MapData map_;
    GameState state_;
    Vec2 startPosition_;
    float startHeading_ = 0.0f;
    float navigationTimer_ = 0.0f;
    float simulationTime_ = 0.0f;
    float suspensionVelocity_ = 0.0f;
    float rollVelocity_ = 0.0f;
    bool nightPreference_ = false;
};
