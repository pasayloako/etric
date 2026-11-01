#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct Vec2 {
    float x = 0.0f;
    float z = 0.0f;
};

struct MapNode {
    Vec2 point;
    std::unordered_map<std::string, std::string> tags;
};

struct RoadWay {
    std::int64_t id = 0;
    std::string name;
    std::vector<std::int64_t> nodeIds;
    std::vector<Vec2> points;
    float halfWidth = 3.0f;
    bool oneWay = false;
    bool reverseOneWay = false;
};

struct RoadSegment {
    Vec2 a;
    Vec2 b;
    std::int64_t nodeA = 0;
    std::int64_t nodeB = 0;
    std::size_t wayIndex = 0;
    std::size_t pointIndex = 0;
    float halfWidth = 3.0f;
};

struct BuildingFootprint {
    std::int64_t id = 0;
    std::vector<Vec2> points;
    float height = 4.0f;
    unsigned colorIndex = 0;
};

struct NearestRoadPoint {
    Vec2 point;
    float distance = 1.0e9f;
    float heading = 0.0f;
    float segmentT = 0.0f;
    std::size_t segmentIndex = 0;
};

struct TrafficSignal {
    Vec2 point;
};

class MapData {
public:
    static constexpr double kPickupLat = 10.2802038;
    static constexpr double kPickupLon = 123.9606781;
    static constexpr double kDestinationLat = 10.2782869;
    static constexpr double kDestinationLon = 123.9588547;

    bool loadFromOsmXml(const std::string& xml, std::string* error = nullptr);
    Vec2 toWorld(double latitude, double longitude) const;
    Vec2 pickupPoint() const;
    Vec2 destinationPoint() const;
    NearestRoadPoint nearestRoadPoint(Vec2 point) const;
    bool isOnRoad(Vec2 point, float edgeInset = 0.2f) const;
    std::vector<Vec2> route(Vec2 from, Vec2 to) const;
    float routeDistance(const std::vector<Vec2>& points) const;
    bool hasConnectedMissionRoute() const;

    const std::vector<RoadWay>& roads() const { return roads_; }
    const std::vector<RoadSegment>& segments() const { return segments_; }
    const std::vector<BuildingFootprint>& buildings() const { return buildings_; }
    const std::vector<Vec2>& trees() const { return trees_; }
    const std::vector<Vec2>& poles() const { return poles_; }
    const std::vector<Vec2>& streetLights() const { return streetLights_; }
    const std::vector<Vec2>& intersections() const { return intersections_; }
    const std::vector<TrafficSignal>& trafficSignals() const { return trafficSignals_; }
    const std::vector<std::vector<Vec2>>& waterAreas() const { return waterAreas_; }
    bool loaded() const { return loaded_; }

private:
    struct Edge {
        std::int64_t to = 0;
        float length = 0.0f;
        std::string roadName;
    };

    std::unordered_map<std::int64_t, MapNode> nodes_;
    std::unordered_map<std::int64_t, std::vector<Edge>> adjacency_;
    std::vector<RoadWay> roads_;
    std::vector<RoadSegment> segments_;
    std::vector<BuildingFootprint> buildings_;
    std::vector<Vec2> trees_;
    std::vector<Vec2> poles_;
    std::vector<Vec2> streetLights_;
    std::vector<Vec2> intersections_;
    std::vector<TrafficSignal> trafficSignals_;
    std::vector<std::vector<Vec2>> waterAreas_;
    bool loaded_ = false;
};

float distanceBetween(Vec2 a, Vec2 b);
