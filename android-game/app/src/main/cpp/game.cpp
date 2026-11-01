#include "game.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265358979323846f;

float wrapAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

Vec2 pointOnWay(const RoadWay& road, float distance, float* heading) {
    if (road.points.size() < 2) return {};
    float remaining = distance;
    for (std::size_t i = 1; i < road.points.size(); ++i) {
        const Vec2 a = road.points[i - 1];
        const Vec2 b = road.points[i];
        const float length = distanceBetween(a, b);
        if (remaining <= length || i + 1 == road.points.size()) {
            const float t = length > 0.01f ? std::clamp(remaining / length, 0.0f, 1.0f) : 0.0f;
            if (heading) *heading = std::atan2(b.x - a.x, b.z - a.z);
            return {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
        }
        remaining -= length;
    }
    return road.points.back();
}

float wayLength(const RoadWay& road) {
    float length = 0.0f;
    for (std::size_t i = 1; i < road.points.size(); ++i) {
        length += distanceBetween(road.points[i - 1], road.points[i]);
    }
    return length;
}

}  // namespace

bool Game::loadMap(const std::string& osmXml, std::string* error) {
    state_.mapReady = false;
    if (!map_.loadFromOsmXml(osmXml, error)) return false;

    const NearestRoadPoint pickup = map_.nearestRoadPoint(map_.pickupPoint());
    const RoadSegment& segment = map_.segments()[pickup.segmentIndex];
    const RoadWay& road = map_.roads()[segment.wayIndex];
    float heading = pickup.heading;
    if (road.reverseOneWay) heading = wrapAngle(heading + kPi);

    const float startOffset = std::min(18.0f, std::max(8.0f, distanceBetween(segment.a, segment.b) * 0.28f));
    startHeading_ = heading;
    startPosition_ = {
        pickup.point.x - std::sin(heading) * startOffset,
        pickup.point.z - std::cos(heading) * startOffset,
    };
    if (!map_.isOnRoad(startPosition_, 0.4f)) {
        startPosition_ = pickup.point;
    }

    const Vec2 side{-std::cos(heading), std::sin(heading)};
    state_.pickupPosition = pickup.point;
    state_.destinationPosition =
        map_.nearestRoadPoint(map_.destinationPoint()).point;
    state_.passengerPosition = {
        pickup.point.x + side.x * (segment.halfWidth + 1.0f),
        pickup.point.z + side.z * (segment.halfWidth + 1.0f),
    };
    state_.mapReady = true;
    resetMission();
    seedTraffic();
    refreshNavigation();
    updateMissionHud();
    return true;
}

void Game::setCoins(int coins) {
    state_.coins = std::max(0, coins);
}

void Game::startMission() {
    if (!state_.mapReady) return;
    state_.started = true;
    state_.paused = false;
    state_.settingsOpen = false;
    if (state_.phase == MissionPhase::Complete) resetMission();
    showToast("Mission started. Drive to the waiting passenger.");
}

void Game::resetMission() {
    state_.phase = MissionPhase::ToPickup;
    state_.started = false;
    state_.paused = false;
    state_.settingsOpen = false;
    state_.passengerOnboard = false;
    state_.reverseGear = false;
    state_.speed = 0.0f;
    state_.steering = 0.0f;
    state_.suspension = 0.0f;
    state_.bodyRoll = 0.0f;
    state_.wheelRotation = 0.0f;
    state_.throttle = 0.0f;
    state_.vehiclePosition = startPosition_;
    state_.heading = startHeading_;
    state_.route.clear();
    navigationTimer_ = 0.0f;
    suspensionVelocity_ = 0.0f;
    rollVelocity_ = 0.0f;
    updateMissionHud();
}

void Game::showToast(const std::string& message, float seconds) {
    state_.toast = message;
    state_.toastSeconds = seconds;
}

void Game::doMissionAction() {
    if (!state_.started || state_.paused || state_.settingsOpen) return;
    const float speed = std::abs(state_.speed);
    if (state_.phase == MissionPhase::ToPickup) {
        const float distance = distanceBetween(state_.vehiclePosition, state_.passengerPosition);
        if (distance > 10.5f) {
            showToast("Drive closer to the waiting passenger.");
        } else if (speed > 0.8f) {
            showToast("Stop the tricycle before pickup.");
        } else {
            state_.passengerOnboard = true;
            state_.phase = MissionPhase::ToDestination;
            showToast("Passenger onboard. Follow the real-road GPS route.");
            navigationTimer_ = 0.0f;
        }
    } else if (state_.phase == MissionPhase::ToDestination) {
        const float distance =
            distanceBetween(state_.vehiclePosition, state_.destinationPosition);
        if (distance > 12.5f) {
            showToast("Follow the GPS route to the drop-off marker.");
        } else if (speed > 0.8f) {
            showToast("Stop the tricycle before drop-off.");
        } else {
            state_.phase = MissionPhase::Complete;
            state_.passengerOnboard = false;
            state_.coins += 250;
            state_.route.clear();
            showToast("Passenger delivered. Mission reward: 250 coins.", 4.0f);
        }
    }
    updateMissionHud();
}

void Game::updateVehicle(const InputState& input, float deltaSeconds) {
    const float previousSpeed = state_.speed;
    const bool accelerating = input.gas && !input.brake;
    const float driveDirection = state_.reverseGear ? -1.0f : 1.0f;
    if (input.brake) {
        const float deceleration = 7.5f * deltaSeconds;
        if (state_.speed > 0.0f) {
            state_.speed = std::max(0.0f, state_.speed - deceleration);
        } else {
            state_.speed = std::min(0.0f, state_.speed + deceleration);
        }
    } else if (accelerating) {
        const float maxSpeed = state_.reverseGear ? 3.0f : 9.2f;
        if (state_.speed * driveDirection < -0.05f) {
            state_.speed += driveDirection * 6.0f * deltaSeconds;
        } else {
            const float ratio = std::clamp(std::abs(state_.speed) / maxSpeed, 0.0f, 1.0f);
            state_.speed += driveDirection * (2.55f * (1.0f - ratio) + 0.14f) *
                            deltaSeconds;
        }
    }

    const float drag = (0.34f + 0.026f * state_.speed * state_.speed) * deltaSeconds;
    if (!accelerating && !input.brake) {
        if (std::abs(state_.speed) <= drag) state_.speed = 0.0f;
        else state_.speed -= std::copysign(drag, state_.speed);
    }
    state_.speed = std::clamp(state_.speed, -3.0f, 9.2f);

    const float steerInput = (input.right ? 1.0f : 0.0f) -
                             (input.left ? 1.0f : 0.0f);
    const float targetSteering = steerInput * 0.46f;
    state_.steering +=
        (targetSteering - state_.steering) * std::min(1.0f, deltaSeconds * 5.5f);
    const float yawRate = state_.speed * std::tan(state_.steering) / 2.15f;
    state_.heading = wrapAngle(state_.heading + yawRate * deltaSeconds);

    const Vec2 forward{std::sin(state_.heading), std::cos(state_.heading)};
    const Vec2 right{std::cos(state_.heading), -std::sin(state_.heading)};
    const Vec2 next{
        state_.vehiclePosition.x + forward.x * state_.speed * deltaSeconds,
        state_.vehiclePosition.z + forward.z * state_.speed * deltaSeconds,
    };
    bool collision = false;
    for (const float forwardOffset : {-1.25f, 1.2f}) {
        for (const float sideOffset : {-0.78f, 0.78f}) {
            const Vec2 corner{
                next.x + forward.x * forwardOffset + right.x * sideOffset,
                next.z + forward.z * forwardOffset + right.z * sideOffset,
            };
            if (!map_.isOnRoad(corner, 0.45f)) collision = true;
        }
    }
    for (const TrafficActor& actor : state_.traffic) {
        if (actor.kind == 3) continue;
        if (distanceBetween(next, actor.position) <
            actor.collisionRadius + 0.9f) {
            collision = true;
            break;
        }
    }
    if (collision) {
        state_.speed *= -0.06f;
        state_.heading = wrapAngle(state_.heading - yawRate * deltaSeconds);
        showToast("Collision detected. Stay on the road and watch traffic.", 1.6f);
    } else {
        state_.vehiclePosition = next;
    }

    const float acceleration = (state_.speed - previousSpeed) / deltaSeconds;
    const float lateralAcceleration = state_.speed * yawRate;
    suspensionVelocity_ +=
        ((-acceleration * 0.010f - state_.suspension) * 65.0f -
         suspensionVelocity_ * 9.0f) *
        deltaSeconds;
    state_.suspension += suspensionVelocity_ * deltaSeconds;
    rollVelocity_ +=
        ((lateralAcceleration * 0.012f - state_.bodyRoll) * 58.0f -
         rollVelocity_ * 8.0f) *
        deltaSeconds;
    state_.bodyRoll += rollVelocity_ * deltaSeconds;
    state_.wheelRotation += state_.speed * deltaSeconds / 0.34f;
    state_.throttle = accelerating ? std::clamp(std::abs(state_.speed) / 7.0f, 0.12f, 1.0f) : 0.0f;
}

void Game::advanceTrafficActor(TrafficActor& actor, float deltaSeconds) {
    const RoadWay& road = map_.roads()[actor.roadIndex];
    const float length = wayLength(road);
    if (length < 1.0f) return;
    actor.progress += actor.direction * actor.speed * deltaSeconds;
    if (actor.progress > length) {
        if (road.oneWay) {
            actor.progress = 0.0f;
        } else {
            actor.progress = length;
            actor.direction = -1;
        }
    } else if (actor.progress < 0.0f) {
        actor.progress = 0.0f;
        actor.direction = 1;
    }
    float heading = 0.0f;
    const Vec2 roadPoint = pointOnWay(road, actor.progress, &heading);
    if (actor.direction < 0) heading = wrapAngle(heading + 3.14159265f);
    const Vec2 side{std::cos(heading), -std::sin(heading)};
    actor.position = {
        roadPoint.x + side.x * actor.sideOffset,
        roadPoint.z + side.z * actor.sideOffset,
    };
    actor.heading = heading;
}

void Game::updateTraffic(float deltaSeconds) {
    for (TrafficActor& actor : state_.traffic) {
        advanceTrafficActor(actor, deltaSeconds);
    }
}

void Game::seedTraffic() {
    state_.traffic.clear();
    if (map_.roads().empty()) return;
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < map_.roads().size(); ++i) {
        const RoadWay& road = map_.roads()[i];
        if (wayLength(road) < 55.0f || road.points.empty()) continue;
        const Vec2 middle = road.points[road.points.size() / 2];
        if (distanceBetween(middle, state_.pickupPosition) < 420.0f ||
            distanceBetween(middle, state_.destinationPosition) < 420.0f) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) {
        for (std::size_t i = 0; i < map_.roads().size(); ++i) {
            if (wayLength(map_.roads()[i]) >= 55.0f) candidates.push_back(i);
        }
    }
    if (candidates.empty()) return;

    const unsigned vehicleCount = std::min<unsigned>(6, candidates.size());
    for (unsigned i = 0; i < vehicleCount; ++i) {
        const std::size_t roadIndex = candidates[(i * 37U + 5U) % candidates.size()];
        const RoadWay& road = map_.roads()[roadIndex];
        const float length = wayLength(road);
        TrafficActor actor;
        actor.roadIndex = roadIndex;
        actor.kind = i % 3;
        actor.speed = actor.kind == 1 ? 4.2f : (actor.kind == 2 ? 5.6f : 4.8f);
        actor.progress = std::fmod(19.0f + static_cast<float>(i) * 37.0f, length);
        actor.sideOffset = actor.kind == 2 ? 0.0f : std::min(0.65f, road.halfWidth * 0.18f);
        actor.collisionRadius = actor.kind == 2 ? 0.85f : 1.5f;
        actor.colorIndex = (i * 3U + 1U) % 7U;
        actor.direction = road.reverseOneWay ? -1 : 1;
        advanceTrafficActor(actor, 0.0f);
        if (distanceBetween(actor.position, startPosition_) < 35.0f) continue;
        state_.traffic.push_back(actor);
    }

    const unsigned pedestrianCount = std::min<unsigned>(7, candidates.size());
    for (unsigned i = 0; i < pedestrianCount; ++i) {
        const std::size_t roadIndex = candidates[(i * 19U + 2U) % candidates.size()];
        const RoadWay& road = map_.roads()[roadIndex];
        const float length = wayLength(road);
        TrafficActor person;
        person.roadIndex = roadIndex;
        person.kind = 3;
        person.speed = 0.45f + 0.06f * static_cast<float>(i % 3);
        person.progress = std::fmod(13.0f + static_cast<float>(i) * 41.0f, length);
        person.sideOffset = road.halfWidth + 1.1f;
        person.collisionRadius = 0.55f;
        person.colorIndex = (i * 2U + 4U) % 7U;
        person.direction = i % 2 == 0 ? 1 : -1;
        if (road.oneWay && road.reverseOneWay) person.direction = -1;
        advanceTrafficActor(person, 0.0f);
        if (distanceBetween(person.position, startPosition_) < 24.0f) continue;
        state_.traffic.push_back(person);
    }
}

void Game::refreshNavigation() {
    Vec2 target = state_.pickupPosition;
    if (state_.phase == MissionPhase::ToDestination) {
        target = state_.destinationPosition;
    } else if (state_.phase == MissionPhase::Complete) {
        state_.route.clear();
        state_.distanceToGoal = 0.0f;
        state_.navigationText = "Mission complete";
        return;
    }

    state_.route = map_.route(state_.vehiclePosition, target);
    if (state_.route.empty()) {
        state_.distanceToGoal = 0.0f;
        state_.navigationText = "No connected OSM route is available.";
        return;
    }
    state_.distanceToGoal = map_.routeDistance(state_.route);
    state_.navigationText = "Continue on the mapped road";
    if (state_.route.size() >= 3) {
        const Vec2 a = state_.route[1];
        const Vec2 b = state_.route[2];
        const Vec2 v1{a.x - state_.route[0].x, a.z - state_.route[0].z};
        const Vec2 v2{b.x - a.x, b.z - a.z};
        const float l1 = std::max(0.01f, std::sqrt(v1.x * v1.x + v1.z * v1.z));
        const float l2 = std::max(0.01f, std::sqrt(v2.x * v2.x + v2.z * v2.z));
        const float dot = (v1.x * v2.x + v1.z * v2.z) / (l1 * l2);
        const float cross = v1.x * v2.z - v1.z * v2.x;
        if (dot < 0.82f) {
            state_.navigationText = cross < 0.0f ? "Turn left at the next junction"
                                                 : "Turn right at the next junction";
        }
    } else {
        state_.navigationText = "Arriving at the mission marker";
    }
}

void Game::updateMissionHud() {
    if (state_.phase == MissionPhase::ToPickup) {
        state_.missionText = "MISSION 1/6  •  PICK UP PASSENGER";
        state_.passengerText = "WAITING";
        state_.distanceToPickup =
            distanceBetween(state_.vehiclePosition, state_.passengerPosition);
    } else if (state_.phase == MissionPhase::ToDestination) {
        state_.missionText = "MISSION 4/6  •  FOLLOW GPS TO DESTINATION";
        state_.passengerText = "ONBOARD";
        state_.distanceToDestination =
            distanceBetween(state_.vehiclePosition, state_.destinationPosition);
    } else {
        state_.missionText = "MISSION COMPLETE";
        state_.passengerText = "DELIVERED";
    }
}

void Game::update(const InputState& input, float deltaSeconds) {
    if (!state_.mapReady) return;
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    simulationTime_ += dt;
    state_.elapsedSeconds += dt;
    state_.timeOfDay += dt / 1200.0f;
    if (state_.timeOfDay >= 24.0f) state_.timeOfDay -= 24.0f;
    if (state_.toastSeconds > 0.0f) state_.toastSeconds -= dt;

    if (input.cameraPressed && state_.started && !state_.paused &&
        !state_.settingsOpen) {
        state_.cameraMode = (state_.cameraMode + 1) % 4;
        static const char* names[] = {"Rear camera", "Front camera",
                                      "Driver camera", "Free camera"};
        showToast(names[state_.cameraMode], 1.5f);
    }

    if (input.settingsPressed) {
        state_.settingsOpen = !state_.settingsOpen;
        if (state_.settingsOpen) state_.paused = true;
        else state_.paused = false;
        return;
    }
    if (state_.settingsOpen) {
        if (input.dayNightPressed) {
            nightPreference_ = !nightPreference_;
            state_.timeOfDay = nightPreference_ ? 21.0f : 14.0f;
            showToast(nightPreference_ ? "Night lighting enabled."
                                       : "Day lighting enabled.");
        }
        if (input.rainPressed) {
            state_.rain = !state_.rain;
            showToast(state_.rain ? "Rain started." : "Rain stopped.");
        }
        if (input.closeSettingsPressed || input.resumePressed) {
            state_.settingsOpen = false;
            state_.paused = false;
        }
        return;
    }

    if (input.pausePressed && state_.started) {
        state_.paused = !state_.paused;
        return;
    }
    if (state_.paused) {
        if (input.resumePressed) state_.paused = false;
        if (input.restartPressed) resetMission();
        return;
    }

    if (!state_.started) {
        if (input.startPressed) startMission();
        if (input.restartPressed) resetMission();
        if (input.replayPressed) {
            resetMission();
            startMission();
        }
        return;
    }

    if (input.reversePressed) {
        state_.reverseGear = !state_.reverseGear;
        showToast(state_.reverseGear ? "Reverse gear selected."
                                     : "Forward gear selected.");
    }
    if (input.restartPressed) {
        resetMission();
        startMission();
        return;
    }
    if (state_.phase == MissionPhase::Complete) {
        if (input.replayPressed) {
            resetMission();
            startMission();
        }
        return;
    }

    state_.freeCameraYaw += input.freeCameraDeltaX * 0.006f;
    state_.freeCameraPitch =
        std::clamp(state_.freeCameraPitch + input.freeCameraDeltaY * 0.004f,
                   -0.05f, 1.1f);
    if (!state_.paused) {
        updateVehicle(input, dt);
        updateTraffic(dt);
        navigationTimer_ -= dt;
        if (navigationTimer_ <= 0.0f) {
            refreshNavigation();
            navigationTimer_ = 0.55f;
        }
        if (input.actionPressed) doMissionAction();
        updateMissionHud();
    }
}
