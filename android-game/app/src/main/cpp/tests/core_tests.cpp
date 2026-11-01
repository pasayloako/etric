#include "../game.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: core-tests <bundled-osm-file>\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    assert(input.good());
    const std::string xml((std::istreambuf_iterator<char>(input)),
                          std::istreambuf_iterator<char>());
    assert(xml.size() > 1000);

    MapData map;
    std::string error;
    assert(map.loadFromOsmXml(xml, &error) && error.empty());
    assert(map.roads().size() >= 250);
    assert(map.segments().size() >= 500);
    assert(map.buildings().size() >= 150);
    assert(map.intersections().size() >= 3);

    MapData unavailableMap;
    std::string unavailableError;
    assert(!unavailableMap.loadFromOsmXml("<osm version=\"0.6\"/>",
                                          &unavailableError));
    assert(!unavailableMap.loaded());
    assert(!unavailableError.empty());

    const auto pickup = map.nearestRoadPoint(map.pickupPoint());
    const auto destination = map.nearestRoadPoint(map.destinationPoint());
    assert(pickup.distance < 70.0f);
    assert(destination.distance < 90.0f);
    const auto route = map.route(pickup.point, destination.point);
    assert(route.size() >= 2);
    const float routeLength = map.routeDistance(route);
    assert(routeLength > 30.0f && routeLength < 1500.0f);
    assert(map.isOnRoad(pickup.point));

    Game game;
    assert(game.loadMap(xml, &error));
    game.setCoins(125);
    assert(game.state().coins == 125);
    assert(game.state().phase == MissionPhase::ToPickup);

    InputState start;
    start.startPressed = true;
    game.update(start, 1.0f / 60.0f);
    assert(game.state().started);

    InputState gas;
    gas.gas = true;
    for (int frame = 0; frame < 60; ++frame) {
        game.update(gas, 1.0f / 60.0f);
    }
    assert(game.state().speed > 0.5f);
    assert(game.state().speed < 9.3f);

    const float forwardSpeed = game.state().speed;
    InputState brake;
    brake.brake = true;
    for (int frame = 0; frame < 60; ++frame) {
        game.update(brake, 1.0f / 60.0f);
    }
    assert(std::abs(game.state().speed) < forwardSpeed);

    InputState steering;
    steering.gas = true;
    steering.left = true;
    for (int frame = 0; frame < 30; ++frame) {
        game.update(steering, 1.0f / 60.0f);
    }
    assert(std::abs(game.state().steering) > 0.05f);
    assert(game.state().coins == 125);

    InputState stop;
    stop.brake = true;
    for (int frame = 0; frame < 120; ++frame) {
        game.update(stop, 1.0f / 60.0f);
    }
    InputState selectReverse;
    selectReverse.reversePressed = true;
    game.update(selectReverse, 1.0f / 60.0f);
    assert(game.state().reverseGear);
    InputState reverseGas;
    reverseGas.gas = true;
    for (int frame = 0; frame < 60; ++frame) {
        game.update(reverseGas, 1.0f / 60.0f);
    }
    assert(game.state().speed < -0.2f);

    InputState weather;
    weather.settingsPressed = true;
    game.update(weather, 1.0f / 60.0f);
    assert(game.state().settingsOpen);
    InputState rain;
    rain.rainPressed = true;
    game.update(rain, 1.0f / 60.0f);
    assert(game.state().rain);

    std::cout << "PASS: bundled OSM (" << map.roads().size() << " driveable ways, "
              << map.buildings().size() << " buildings), connected route "
              << static_cast<int>(std::round(routeLength)) << " m, physics and controls.\n";
    return 0;
}
