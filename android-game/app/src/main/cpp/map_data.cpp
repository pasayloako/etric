#include "map_data.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace {
constexpr double kEarthMetersPerDegree = 111320.0;
constexpr double kLatMetersPerDegree = 110574.0;
constexpr double kOriginLatitude = MapData::kPickupLat;
constexpr double kOriginLongitude = MapData::kPickupLon;
const double kLongitudeScale =
    kEarthMetersPerDegree * std::cos(kOriginLatitude * 3.14159265358979323846 / 180.0);

std::string attribute(const std::string& source, const std::string& name) {
    const std::string token = name + "=\"";
    std::size_t position = source.find(token);
    while (position != std::string::npos) {
        if (position == 0 || source[position - 1] == ' ' ||
            source[position - 1] == '\t' || source[position - 1] == '\n') {
            const std::size_t start = position + token.size();
            const std::size_t end = source.find('"', start);
            if (end != std::string::npos) {
                return source.substr(start, end - start);
            }
        }
        position = source.find(token, position + 1);
    }
    return {};
}

float parseFloat(const std::string& text, float fallback) {
    if (text.empty()) return fallback;
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    return end != text.c_str() && std::isfinite(value) ? value : fallback;
}

std::int64_t parseId(const std::string& text) {
    if (text.empty()) return 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    return end != text.c_str() ? static_cast<std::int64_t>(value) : 0;
}

std::unordered_map<std::string, std::string> readTags(const std::string& body) {
    std::unordered_map<std::string, std::string> tags;
    std::size_t position = 0;
    while ((position = body.find("<tag ", position)) != std::string::npos) {
        const std::size_t end = body.find('>', position);
        if (end == std::string::npos) break;
        const std::string tag = body.substr(position, end - position + 1);
        const std::string key = attribute(tag, "k");
        const std::string value = attribute(tag, "v");
        if (!key.empty()) tags[key] = value;
        position = end + 1;
    }
    return tags;
}

float roadHalfWidth(const std::string& highway) {
    if (highway == "primary" || highway == "secondary" || highway == "trunk") return 4.6f;
    if (highway == "tertiary" || highway == "unclassified") return 3.8f;
    if (highway == "residential" || highway == "living_street") return 3.1f;
    if (highway == "service" || highway == "road") return 2.6f;
    return 3.2f;
}

bool isDriveable(const std::string& highway) {
    static const std::unordered_set<std::string> excluded = {
        "footway", "path", "steps", "cycleway", "pedestrian", "bridleway",
        "corridor", "construction", "proposed", "platform",
    };
    return !highway.empty() && excluded.find(highway) == excluded.end();
}

float segmentProjection(Vec2 point, Vec2 a, Vec2 b, float* outT, Vec2* outPoint) {
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float lengthSquared = dx * dx + dz * dz;
    const float t = lengthSquared > 1.0e-5f
                        ? std::clamp(((point.x - a.x) * dx + (point.z - a.z) * dz) /
                                         lengthSquared,
                                     0.0f, 1.0f)
                        : 0.0f;
    const Vec2 projected{a.x + t * dx, a.z + t * dz};
    if (outT) *outT = t;
    if (outPoint) *outPoint = projected;
    return distanceBetween(point, projected);
}

bool allowedAlongWay(const RoadWay& way, bool aToB) {
    if (!way.oneWay) return true;
    return way.reverseOneWay ? !aToB : aToB;
}
}  // namespace

float distanceBetween(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Vec2 MapData::toWorld(double latitude, double longitude) const {
    return {
        static_cast<float>((longitude - kOriginLongitude) * kLongitudeScale),
        static_cast<float>(-(latitude - kOriginLatitude) * kLatMetersPerDegree),
    };
}

Vec2 MapData::pickupPoint() const {
    return toWorld(kPickupLat, kPickupLon);
}

Vec2 MapData::destinationPoint() const {
    return toWorld(kDestinationLat, kDestinationLon);
}

bool MapData::loadFromOsmXml(const std::string& xml, std::string* error) {
    nodes_.clear();
    adjacency_.clear();
    roads_.clear();
    segments_.clear();
    buildings_.clear();
    trees_.clear();
    poles_.clear();
    streetLights_.clear();
    intersections_.clear();
    trafficSignals_.clear();
    waterAreas_.clear();
    loaded_ = false;

    std::size_t position = 0;
    while ((position = xml.find("<node ", position)) != std::string::npos) {
        const std::size_t openingEnd = xml.find('>', position);
        if (openingEnd == std::string::npos) break;
        const std::string opening = xml.substr(position, openingEnd - position + 1);
        const bool selfClosing = opening.size() > 1 &&
                                 opening[opening.size() - 2] == '/';
        std::size_t bodyEnd = openingEnd;
        if (!selfClosing) {
            bodyEnd = xml.find("</node>", openingEnd);
            if (bodyEnd == std::string::npos) bodyEnd = openingEnd;
        }
        const std::string body = selfClosing
                                     ? std::string()
                                     : xml.substr(openingEnd + 1, bodyEnd - openingEnd - 1);
        const auto id = parseId(attribute(opening, "id"));
        const double lat = parseFloat(attribute(opening, "lat"), 999.0f);
        const double lon = parseFloat(attribute(opening, "lon"), 999.0f);
        if (id != 0 && std::abs(lat) <= 90.0 && std::abs(lon) <= 180.0) {
            MapNode node{toWorld(lat, lon), readTags(body)};
            const auto natural = node.tags.find("natural");
            const auto power = node.tags.find("power");
            const auto highway = node.tags.find("highway");
            if (natural != node.tags.end() && natural->second == "tree") {
                trees_.push_back(node.point);
            }
            if (power != node.tags.end() && power->second == "pole") {
                poles_.push_back(node.point);
            }
            if (highway != node.tags.end() && highway->second == "street_lamp") {
                streetLights_.push_back(node.point);
            }
            if (highway != node.tags.end() && highway->second == "traffic_signals") {
                trafficSignals_.push_back({node.point});
            }
            nodes_[id] = std::move(node);
        }
        position = selfClosing ? openingEnd + 1 : bodyEnd + 7;
    }

    position = 0;
    while ((position = xml.find("<way ", position)) != std::string::npos) {
        const std::size_t openingEnd = xml.find('>', position);
        const std::size_t wayEnd = xml.find("</way>", openingEnd);
        if (openingEnd == std::string::npos || wayEnd == std::string::npos) break;
        const std::string opening = xml.substr(position, openingEnd - position + 1);
        const std::string body = xml.substr(openingEnd + 1, wayEnd - openingEnd - 1);
        const auto tags = readTags(body);

        std::vector<std::int64_t> refs;
        std::size_t ndPosition = 0;
        while ((ndPosition = body.find("<nd ", ndPosition)) != std::string::npos) {
            const std::size_t ndEnd = body.find('>', ndPosition);
            if (ndEnd == std::string::npos) break;
            const auto ref = parseId(attribute(body.substr(ndPosition, ndEnd - ndPosition + 1), "ref"));
            if (ref != 0) refs.push_back(ref);
            ndPosition = ndEnd + 1;
        }

        std::vector<Vec2> points;
        points.reserve(refs.size());
        bool complete = true;
        for (const auto ref : refs) {
            const auto found = nodes_.find(ref);
            if (found == nodes_.end()) {
                complete = false;
                break;
            }
            points.push_back(found->second.point);
        }

        const auto highway = tags.find("highway");
        if (complete && refs.size() >= 2 && highway != tags.end() &&
            isDriveable(highway->second)) {
            RoadWay road;
            road.id = parseId(attribute(opening, "id"));
            road.nodeIds = refs;
            road.points = points;
            road.halfWidth = roadHalfWidth(highway->second);
            road.name = tags.count("name") ? tags.at("name") : "";
            const auto oneWay = tags.find("oneway");
            const auto junction = tags.find("junction");
            road.oneWay = oneWay != tags.end() &&
                          (oneWay->second == "yes" || oneWay->second == "1" ||
                           oneWay->second == "true");
            road.oneWay = road.oneWay ||
                          (junction != tags.end() && junction->second == "roundabout");
            road.reverseOneWay = oneWay != tags.end() && oneWay->second == "-1";
            const std::size_t wayIndex = roads_.size();
            roads_.push_back(road);
            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                const float length = distanceBetween(points[i], points[i + 1]);
                if (length < 0.05f) continue;
                segments_.push_back({points[i], points[i + 1], refs[i], refs[i + 1],
                                     wayIndex, i, road.halfWidth});
                const auto addEdge = [&](std::int64_t from, std::int64_t to) {
                    adjacency_[from].push_back({to, length, road.name});
                };
                if (!road.oneWay) {
                    addEdge(refs[i], refs[i + 1]);
                    addEdge(refs[i + 1], refs[i]);
                } else if (road.reverseOneWay) {
                    addEdge(refs[i + 1], refs[i]);
                } else {
                    addEdge(refs[i], refs[i + 1]);
                }
            }
        }

        const auto building = tags.find("building");
        if (complete && points.size() >= 4 && building != tags.end()) {
            BuildingFootprint footprint;
            footprint.id = parseId(attribute(opening, "id"));
            footprint.points = points;
            const auto height = tags.find("height");
            const auto levels = tags.find("building:levels");
            footprint.height = height != tags.end()
                                   ? parseFloat(height->second, 5.0f)
                                   : (levels != tags.end()
                                          ? parseFloat(levels->second, 2.0f) * 3.1f
                                          : 4.0f + static_cast<float>(
                                                       (static_cast<std::uint64_t>(
                                                            footprint.id) *
                                                        2654435761U) %
                                                       4U));
            footprint.height = std::clamp(footprint.height, 2.5f, 28.0f);
            footprint.colorIndex = static_cast<unsigned>(std::abs(footprint.id) % 7);
            buildings_.push_back(std::move(footprint));
        }

        const auto natural = tags.find("natural");
        if (complete && points.size() >= 4 && natural != tags.end() &&
            natural->second == "water") {
            waterAreas_.push_back(points);
        }
        position = wayEnd + 6;
    }

    std::unordered_map<std::int64_t, std::unordered_set<std::int64_t>> roadNeighbors;
    for (const RoadWay& road : roads_) {
        for (std::size_t i = 1; i < road.nodeIds.size(); ++i) {
            roadNeighbors[road.nodeIds[i - 1]].insert(road.nodeIds[i]);
            roadNeighbors[road.nodeIds[i]].insert(road.nodeIds[i - 1]);
        }
    }
    for (const auto& [nodeId, neighbors] : roadNeighbors) {
        const auto node = nodes_.find(nodeId);
        if (neighbors.size() >= 3 && node != nodes_.end()) {
            intersections_.push_back(node->second.point);
        }
    }
    std::sort(intersections_.begin(), intersections_.end(),
              [&](Vec2 a, Vec2 b) {
                  return distanceBetween(a, pickupPoint()) <
                         distanceBetween(b, pickupPoint());
              });

    if (segments_.empty()) {
        if (error) *error = "The bundled OpenStreetMap area contains no drivable roads.";
        return false;
    }
    if (buildings_.empty()) {
        if (error) *error = "The bundled OpenStreetMap area contains no building footprints.";
        return false;
    }

    loaded_ = true;
    const NearestRoadPoint pickup = nearestRoadPoint(pickupPoint());
    const NearestRoadPoint destination = nearestRoadPoint(destinationPoint());
    if (pickup.distance > 70.0f || destination.distance > 90.0f) {
        loaded_ = false;
        if (error) *error = "An IMISUBEBE mission coordinate is too far from mapped roads.";
        return false;
    }
    if (route(pickup.point, destination.point).empty()) {
        loaded_ = false;
        if (error) *error = "The OSM road graph has no connected pickup-to-destination route.";
        return false;
    }
    return true;
}

NearestRoadPoint MapData::nearestRoadPoint(Vec2 point) const {
    NearestRoadPoint best;
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        const RoadSegment& segment = segments_[i];
        float t = 0.0f;
        Vec2 projection;
        const float distance = segmentProjection(point, segment.a, segment.b, &t, &projection);
        if (distance < best.distance) {
            best.point = projection;
            best.distance = distance;
            best.segmentT = t;
            best.segmentIndex = i;
            best.heading = std::atan2(segment.b.x - segment.a.x,
                                      segment.b.z - segment.a.z);
        }
    }
    return best;
}

bool MapData::isOnRoad(Vec2 point, float edgeInset) const {
    for (const RoadSegment& segment : segments_) {
        if (std::abs(point.x - segment.a.x) >
                std::abs(segment.b.x - segment.a.x) + segment.halfWidth + 1.0f ||
            std::abs(point.z - segment.a.z) >
                std::abs(segment.b.z - segment.a.z) + segment.halfWidth + 1.0f) {
            continue;
        }
        if (segmentProjection(point, segment.a, segment.b, nullptr, nullptr) <=
            std::max(0.5f, segment.halfWidth - edgeInset)) {
            return true;
        }
    }
    return false;
}

std::vector<Vec2> MapData::route(Vec2 from, Vec2 to) const {
    if (!loaded_ || segments_.empty()) return {};
    const NearestRoadPoint source = nearestRoadPoint(from);
    const NearestRoadPoint destination = nearestRoadPoint(to);
    if (source.distance > 90.0f || destination.distance > 90.0f) return {};

    const RoadSegment& sourceSegment = segments_[source.segmentIndex];
    const RoadSegment& targetSegment = segments_[destination.segmentIndex];
    const RoadWay& sourceWay = roads_[sourceSegment.wayIndex];
    const RoadWay& targetWay = roads_[targetSegment.wayIndex];

    struct Candidate {
        std::int64_t node = 0;
        float accessDistance = 0.0f;
        bool valid = false;
    };
    const float sourceLength = distanceBetween(sourceSegment.a, sourceSegment.b);
    const float targetLength = distanceBetween(targetSegment.a, targetSegment.b);
    Candidate sources[2] = {
        {sourceSegment.nodeA, source.segmentT * sourceLength,
         allowedAlongWay(sourceWay, false)},
        {sourceSegment.nodeB, (1.0f - source.segmentT) * sourceLength,
         allowedAlongWay(sourceWay, true)},
    };
    Candidate targets[2] = {
        {targetSegment.nodeA, destination.segmentT * targetLength,
         allowedAlongWay(targetWay, true)},
        {targetSegment.nodeB, (1.0f - destination.segmentT) * targetLength,
         allowedAlongWay(targetWay, false)},
    };

    float bestCost = std::numeric_limits<float>::infinity();
    std::vector<Vec2> bestPath;
    for (const Candidate& start : sources) {
        if (!start.valid) continue;
        using QueueItem = std::pair<float, std::int64_t>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
        std::unordered_map<std::int64_t, float> distances;
        std::unordered_map<std::int64_t, std::int64_t> previous;
        distances[start.node] = 0.0f;
        queue.push({0.0f, start.node});
        while (!queue.empty()) {
            const auto [distance, node] = queue.top();
            queue.pop();
            if (distance > distances[node]) continue;
            const auto neighbors = adjacency_.find(node);
            if (neighbors == adjacency_.end()) continue;
            for (const Edge& edge : neighbors->second) {
                const float nextDistance = distance + edge.length;
                const auto current = distances.find(edge.to);
                if (current == distances.end() || nextDistance < current->second) {
                    distances[edge.to] = nextDistance;
                    previous[edge.to] = node;
                    queue.push({nextDistance, edge.to});
                }
            }
        }

        for (const Candidate& goal : targets) {
            if (!goal.valid) continue;
            const auto found = distances.find(goal.node);
            if (found == distances.end()) continue;
            const float cost = start.accessDistance + found->second + goal.accessDistance;
            if (cost >= bestCost) continue;

            std::vector<std::int64_t> nodePath;
            std::int64_t cursor = goal.node;
            nodePath.push_back(cursor);
            while (cursor != start.node) {
                const auto parent = previous.find(cursor);
                if (parent == previous.end()) {
                    nodePath.clear();
                    break;
                }
                cursor = parent->second;
                nodePath.push_back(cursor);
            }
            if (nodePath.empty()) continue;
            std::reverse(nodePath.begin(), nodePath.end());

            std::vector<Vec2> candidatePath;
            candidatePath.push_back(source.point);
            for (const auto nodeId : nodePath) {
                const auto node = nodes_.find(nodeId);
                if (node != nodes_.end() &&
                    distanceBetween(candidatePath.back(), node->second.point) > 0.25f) {
                    candidatePath.push_back(node->second.point);
                }
            }
            if (distanceBetween(candidatePath.back(), destination.point) > 0.25f) {
                candidatePath.push_back(destination.point);
            }
            bestCost = cost;
            bestPath = std::move(candidatePath);
        }
    }
    return bestPath;
}

float MapData::routeDistance(const std::vector<Vec2>& points) const {
    float distance = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        distance += distanceBetween(points[i - 1], points[i]);
    }
    return distance;
}

bool MapData::hasConnectedMissionRoute() const {
    if (!loaded_) return false;
    const auto start = nearestRoadPoint(pickupPoint());
    const auto destination = nearestRoadPoint(destinationPoint());
    return !route(start.point, destination.point).empty();
}
