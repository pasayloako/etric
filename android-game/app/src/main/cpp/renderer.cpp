#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

using Vertex = RenderVertex;

struct HudVertex {
    float x, y;
    float r, g, b, a;
};

struct Color {
    float r, g, b;
};

struct Vec3Like {
    float x, y, z;
};

Color rgb(unsigned value) {
    return {
        static_cast<float>((value >> 16) & 0xff) / 255.0f,
        static_cast<float>((value >> 8) & 0xff) / 255.0f,
        static_cast<float>(value & 0xff) / 255.0f,
    };
}

class Geometry {
public:
    std::vector<Vertex> vertices;

    void triangle(Vec2 a, float ay, Vec2 b, float by, Vec2 c, float cy, Color color) {
        vertices.push_back({a.x, ay, a.z, color.r, color.g, color.b});
        vertices.push_back({b.x, by, b.z, color.r, color.g, color.b});
        vertices.push_back({c.x, cy, c.z, color.r, color.g, color.b});
    }

    void quad(Vec2 a, float ay, Vec2 b, float by, Vec2 c, float cy, Vec2 d, float dy,
              Color color) {
        triangle(a, ay, b, by, c, cy, color);
        triangle(a, ay, c, cy, d, dy, color);
    }

    void box(Vec3Like center, Vec3Like size, Color color) {
        const float x0 = center.x - size.x * 0.5f;
        const float x1 = center.x + size.x * 0.5f;
        const float y0 = center.y - size.y * 0.5f;
        const float y1 = center.y + size.y * 0.5f;
        const float z0 = center.z - size.z * 0.5f;
        const float z1 = center.z + size.z * 0.5f;
        const Vec2 p000{x0, z0}, p001{x0, z1};
        const Vec2 p100{x1, z0}, p101{x1, z1};
        quad(p001, y0, p101, y0, p101, y1, p001, y1, color);
        quad(p000, y0, p001, y0, p001, y1, p000, y1, color);
        quad(p101, y0, p100, y0, p100, y1, p101, y1, color);
        quad(p100, y0, p000, y0, p000, y1, p100, y1, color);
        quad(p000, y1, p001, y1, p101, y1, p100, y1, color);
        quad(p000, y0, p100, y0, p101, y0, p001, y0, color);
    }

    void cylinder(Vec2 center, float bottom, float top, float radiusBottom,
                  float radiusTop, Color color, int sides = 7) {
        for (int i = 0; i < sides; ++i) {
            const float a0 = 2.0f * kPi * static_cast<float>(i) / sides;
            const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / sides;
            const Vec2 b0{center.x + std::cos(a0) * radiusBottom,
                          center.z + std::sin(a0) * radiusBottom};
            const Vec2 b1{center.x + std::cos(a1) * radiusBottom,
                          center.z + std::sin(a1) * radiusBottom};
            const Vec2 t0{center.x + std::cos(a0) * radiusTop,
                          center.z + std::sin(a0) * radiusTop};
            const Vec2 t1{center.x + std::cos(a1) * radiusTop,
                          center.z + std::sin(a1) * radiusTop};
            quad(b0, bottom, b1, bottom, t1, top, t0, top, color);
            triangle({center.x, center.z}, top, t0, top, t1, top, color);
        }
    }
};

std::array<const char*, 7> glyph(char c) {
    switch (c) {
        case 'A': return {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
        case 'B': return {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
        case 'C': return {"01111", "10000", "10000", "10000", "10000", "10000", "01111"};
        case 'D': return {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
        case 'E': return {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
        case 'F': return {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
        case 'G': return {"01111", "10000", "10000", "10111", "10001", "10001", "01111"};
        case 'H': return {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
        case 'I': return {"11111", "00100", "00100", "00100", "00100", "00100", "11111"};
        case 'J': return {"00111", "00010", "00010", "00010", "00010", "10010", "01100"};
        case 'K': return {"10001", "10010", "10100", "11000", "10100", "10010", "10001"};
        case 'L': return {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
        case 'M': return {"10001", "11011", "10101", "10101", "10001", "10001", "10001"};
        case 'N': return {"10001", "11001", "10101", "10011", "10001", "10001", "10001"};
        case 'O': return {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
        case 'P': return {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
        case 'Q': return {"01110", "10001", "10001", "10001", "10101", "10010", "01101"};
        case 'R': return {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
        case 'S': return {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
        case 'T': return {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
        case 'U': return {"10001", "10001", "10001", "10001", "10001", "10001", "01110"};
        case 'V': return {"10001", "10001", "10001", "10001", "10001", "01010", "00100"};
        case 'W': return {"10001", "10001", "10001", "10101", "10101", "10101", "01010"};
        case 'X': return {"10001", "10001", "01010", "00100", "01010", "10001", "10001"};
        case 'Y': return {"10001", "10001", "01010", "00100", "00100", "00100", "00100"};
        case 'Z': return {"11111", "00001", "00010", "00100", "01000", "10000", "11111"};
        case '0': return {"01110", "10001", "10011", "10101", "11001", "10001", "01110"};
        case '1': return {"00100", "01100", "00100", "00100", "00100", "00100", "01110"};
        case '2': return {"01110", "10001", "00001", "00010", "00100", "01000", "11111"};
        case '3': return {"11110", "00001", "00001", "01110", "00001", "00001", "11110"};
        case '4': return {"00010", "00110", "01010", "10010", "11111", "00010", "00010"};
        case '5': return {"11111", "10000", "10000", "11110", "00001", "00001", "11110"};
        case '6': return {"01110", "10000", "10000", "11110", "10001", "10001", "01110"};
        case '7': return {"11111", "00001", "00010", "00100", "01000", "01000", "01000"};
        case '8': return {"01110", "10001", "10001", "01110", "10001", "10001", "01110"};
        case '9': return {"01110", "10001", "10001", "01111", "00001", "00001", "01110"};
        case ':': return {"00000", "00100", "00100", "00000", "00100", "00100", "00000"};
        case '.': return {"00000", "00000", "00000", "00000", "00000", "00110", "00110"};
        case '-': return {"00000", "00000", "00000", "11111", "00000", "00000", "00000"};
        case '/': return {"00001", "00010", "00010", "00100", "01000", "01000", "10000"};
        case '+': return {"00000", "00100", "00100", "11111", "00100", "00100", "00000"};
        default: return {"00000", "00000", "00000", "00000", "00000", "00000", "00000"};
    }
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aColor");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

std::array<float, 16> multiply(const std::array<float, 16>& a,
                               const std::array<float, 16>& b) {
    std::array<float, 16> out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int i = 0; i < 4; ++i) {
                out[col * 4 + row] += a[i * 4 + row] * b[col * 4 + i];
            }
        }
    }
    return out;
}

std::array<float, 16> perspective(float fov, float aspect, float nearPlane,
                                  float farPlane) {
    std::array<float, 16> out{};
    const float f = 1.0f / std::tan(fov * 0.5f);
    out[0] = f / std::max(0.01f, aspect);
    out[5] = f;
    out[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    out[11] = -1.0f;
    out[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return out;
}

std::array<float, 16> lookAt(Vec3Like eye, Vec3Like target) {
    const auto normalize = [](Vec3Like v) {
        const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length < 1.0e-5f) return Vec3Like{0, 0, 1};
        return Vec3Like{v.x / length, v.y / length, v.z / length};
    };
    const auto cross = [](Vec3Like a, Vec3Like b) {
        return Vec3Like{a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x};
    };
    const auto dot = [](Vec3Like a, Vec3Like b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const Vec3Like forward = normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const Vec3Like side = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    const Vec3Like up = cross(side, forward);
    return {
        side.x, up.x, -forward.x, 0.0f,
        side.y, up.y, -forward.y, 0.0f,
        side.z, up.z, -forward.z, 0.0f,
        -dot(side, eye), -dot(up, eye), dot(forward, eye), 1.0f,
    };
}

Vec3Like worldPoint(Vec2 origin, float heading, Vec3Like local, float lift = 0.0f) {
    const float cosine = std::cos(heading);
    const float sine = std::sin(heading);
    return {origin.x + local.x * cosine + local.z * sine,
            local.y + lift,
            origin.z - local.x * sine + local.z * cosine};
}

void addTriangle(std::vector<Vertex>& vertices, Vec3Like a, Vec3Like b, Vec3Like c,
                 Color color) {
    vertices.push_back({a.x, a.y, a.z, color.r, color.g, color.b});
    vertices.push_back({b.x, b.y, b.z, color.r, color.g, color.b});
    vertices.push_back({c.x, c.y, c.z, color.r, color.g, color.b});
}

void addQuad(std::vector<Vertex>& vertices, Vec3Like a, Vec3Like b, Vec3Like c,
             Vec3Like d, Color color) {
    addTriangle(vertices, a, b, c, color);
    addTriangle(vertices, a, c, d, color);
}

void addLocalBox(std::vector<Vertex>& vertices, Vec2 origin, float heading,
                 Vec3Like center, Vec3Like size, Color color,
                 float lift = 0.0f, float pitch = 0.0f, float roll = 0.0f) {
    const float x0 = center.x - size.x * 0.5f;
    const float x1 = center.x + size.x * 0.5f;
    const float y0 = center.y - size.y * 0.5f;
    const float y1 = center.y + size.y * 0.5f;
    const float z0 = center.z - size.z * 0.5f;
    const float z1 = center.z + size.z * 0.5f;
    const Vec3Like points[] = {
        {x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0},
        {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0},
    };
    const int faces[][4] = {
        {1, 2, 6, 5}, {0, 1, 5, 4}, {2, 3, 7, 6},
        {3, 0, 4, 7}, {4, 5, 6, 7}, {0, 3, 2, 1},
    };
    const auto transform = [&](Vec3Like point) {
        const float rolledX = point.x * std::cos(roll) - point.y * std::sin(roll);
        const float rolledY = point.x * std::sin(roll) + point.y * std::cos(roll);
        const float pitchedY = rolledY * std::cos(pitch) - point.z * std::sin(pitch);
        const float pitchedZ = rolledY * std::sin(pitch) + point.z * std::cos(pitch);
        return worldPoint(origin, heading, {rolledX, pitchedY, pitchedZ}, lift);
    };
    for (const auto& face : faces) {
        addQuad(vertices, transform(points[face[0]]), transform(points[face[1]]),
                transform(points[face[2]]), transform(points[face[3]]), color);
    }
}

void addWheel(std::vector<Vertex>& vertices, Vec2 origin, float heading,
              float x, float y, float z, float rotation, float lift) {
    constexpr int sides = 10;
    const float radius = 0.34f;
    const float halfWidth = 0.09f;
    const Color tire = rgb(0x141518);
    const Color rim = rgb(0x9aa0a8);
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / sides + rotation;
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / sides + rotation;
        const Vec3Like inner0{x - halfWidth, y + std::cos(a0) * radius,
                              z + std::sin(a0) * radius};
        const Vec3Like inner1{x - halfWidth, y + std::cos(a1) * radius,
                              z + std::sin(a1) * radius};
        const Vec3Like outer0{x + halfWidth, y + std::cos(a0) * radius,
                              z + std::sin(a0) * radius};
        const Vec3Like outer1{x + halfWidth, y + std::cos(a1) * radius,
                              z + std::sin(a1) * radius};
        addQuad(vertices, worldPoint(origin, heading, inner0, lift),
                worldPoint(origin, heading, outer0, lift),
                worldPoint(origin, heading, outer1, lift),
                worldPoint(origin, heading, inner1, lift), tire);
        const Vec3Like hub{x + halfWidth + 0.005f, y, z};
        const Vec3Like rim0{x + halfWidth + 0.007f, y + std::cos(a0) * radius * 0.56f,
                            z + std::sin(a0) * radius * 0.56f};
        const Vec3Like rim1{x + halfWidth + 0.007f, y + std::cos(a1) * radius * 0.56f,
                            z + std::sin(a1) * radius * 0.56f};
        addTriangle(vertices, worldPoint(origin, heading, hub, lift),
                    worldPoint(origin, heading, rim0, lift),
                    worldPoint(origin, heading, rim1, lift), rim);
    }
}

void addPerson(std::vector<Vertex>& vertices, Vec2 origin, float heading,
               float baseY, bool seated, unsigned clothesColor, float lift = 0.0f) {
    const Color dark = rgb(0x26304a);
    const Color shirt = rgb(clothesColor);
    const Color skin = rgb(0xd7a27a);
    if (seated) {
        addLocalBox(vertices, origin, heading, {0.0f, baseY + 0.32f, 0.04f},
                    {0.3f, 0.53f, 0.23f}, shirt, lift);
        addLocalBox(vertices, origin, heading, {0.0f, baseY + 0.68f, 0.03f},
                    {0.22f, 0.22f, 0.22f}, skin, lift);
        addLocalBox(vertices, origin, heading, {0.0f, baseY + 0.07f, -0.06f},
                    {0.34f, 0.12f, 0.4f}, dark, lift);
    } else {
        addLocalBox(vertices, origin, heading, {0.0f, baseY + 0.42f, 0.0f},
                    {0.34f, 0.75f, 0.25f}, shirt, lift);
        addLocalBox(vertices, origin, heading, {0.0f, baseY + 0.84f, 0.0f},
                    {0.22f, 0.24f, 0.22f}, skin, lift);
    }
}

void addVehicle(std::vector<Vertex>& vertices, const GameState& state) {
    const Vec2 origin = state.vehiclePosition;
    const float heading = state.heading;
    const float lift = state.suspension + 0.35f;
    const float pitch = std::clamp(state.suspension * 0.6f, -0.09f, 0.09f);
    const float roll = std::clamp(state.bodyRoll, -0.12f, 0.12f);
    const Color black = rgb(0x17191c);
    const Color blackEdge = rgb(0x25282b);
    const Color glass = rgb(0x8dbdd2);
    const Color seat = rgb(0x565a61);
    const auto box = [&](Vec3Like center, Vec3Like size, Color color) {
        addLocalBox(vertices, origin, heading, center, size, color, lift, pitch, roll);
    };
    box({0, 0.22f, -0.08f}, {1.48f, 0.26f, 2.18f}, black);
    box({0, 0.48f, 0.0f}, {1.26f, 0.18f, 0.62f}, blackEdge);
    box({0, 0.76f, -1.0f}, {1.42f, 0.62f, 0.12f}, black);
    box({0, 0.79f, -0.37f}, {0.09f, 0.92f, 1.25f}, blackEdge);
    box({-0.68f, 1.15f, -0.37f}, {0.06f, 0.94f, 1.26f}, black);
    box({0.68f, 1.15f, -0.37f}, {0.06f, 0.94f, 1.26f}, black);
    box({-0.67f, 1.16f, 0.61f}, {0.07f, 1.0f, 0.07f}, black);
    box({0.67f, 1.16f, 0.61f}, {0.07f, 1.0f, 0.07f}, black);
    box({-0.67f, 1.14f, -1.1f}, {0.06f, 0.95f, 0.07f}, black);
    box({0.67f, 1.14f, -1.1f}, {0.06f, 0.95f, 0.07f}, black);
    box({0, 1.7f, -0.29f}, {1.55f, 0.1f, 2.08f}, black);
    box({0, 1.74f, -0.29f}, {1.34f, 0.045f, 1.87f}, blackEdge);
    box({0, 0.62f, 0.78f}, {0.92f, 0.56f, 0.65f}, black);
    box({0, 1.04f, 0.72f}, {1.08f, 0.18f, 0.49f}, black);
    addLocalBox(vertices, origin, heading, {0, 1.48f, 0.52f},
                {0.95f, 0.62f, 0.045f}, glass, lift, pitch - 0.18f, roll);
    box({-0.30f, 0.94f, -0.61f}, {0.5f, 0.16f, 0.45f}, seat);
    box({0.30f, 0.94f, -0.61f}, {0.5f, 0.16f, 0.45f}, seat);
    box({-0.30f, 1.12f, -0.89f}, {0.48f, 0.42f, 0.12f}, blackEdge);
    box({0.30f, 1.12f, -0.89f}, {0.48f, 0.42f, 0.12f}, blackEdge);
    box({0, 0.77f, 0.14f}, {0.5f, 0.18f, 0.47f}, seat);
    box({0, 1.02f, -0.02f}, {0.5f, 0.42f, 0.09f}, seat);
    addPerson(vertices, origin, heading, 0.83f, true, 0xe8e9f1, lift);
    if (state.passengerOnboard) {
        const Vec3Like passengerSeat =
            worldPoint(origin, heading, {0.30f, 0.0f, -0.65f}, lift);
        addPerson(vertices, {passengerSeat.x, passengerSeat.z}, heading,
                  0.83f, true, 0xdcdcf5);
    }
    box({0, 0.79f, -1.075f}, {0.52f, 0.16f, 0.035f}, rgb(0x101114));
    for (int i = 0; i < 9; ++i) {
        const float xStart = -0.22f + static_cast<float>(i) * 0.055f;
        const auto rows = glyph("IMISUBEBE"[i]);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row][col] != '1') continue;
                const float x = xStart + static_cast<float>(col) * 0.009f;
                const float y = 0.83f - static_cast<float>(row) * 0.018f;
                box({x, y, -1.098f}, {0.008f, 0.014f, 0.012f},
                    rgb(0xf5f5f3));
            }
        }
    }
    box({-0.52f, 0.84f, -1.15f}, {0.17f, 0.12f, 0.035f}, rgb(0xff2020));
    box({0.52f, 0.84f, -1.15f}, {0.17f, 0.12f, 0.035f}, rgb(0xff2020));
    box({-0.28f, 0.93f, 1.09f}, {0.23f, 0.15f, 0.06f}, rgb(0xfff4cb));
    box({0.28f, 0.93f, 1.09f}, {0.23f, 0.15f, 0.06f}, rgb(0xfff4cb));
    box({0, 0.68f, 1.0f}, {0.2f, 0.05f, 0.7f}, black);

    addWheel(vertices, origin, heading, -0.69f, 0.34f, -0.71f,
             state.wheelRotation, 0.0f);
    addWheel(vertices, origin, heading, 0.69f, 0.34f, -0.71f,
             state.wheelRotation, 0.0f);
    const Vec3Like frontWheel = worldPoint(origin, heading, {0.0f, 0.0f, 1.22f});
    addWheel(vertices, {frontWheel.x, frontWheel.z}, heading + state.steering,
             0.0f, 0.34f, 0.0f, state.wheelRotation, 0.0f);
}

void addTraffic(std::vector<Vertex>& vertices, const GameState& state) {
    const std::array<Color, 7> colors = {
        rgb(0xd6392e), rgb(0xd5d8d9), rgb(0x2874a6), rgb(0xe4b83e),
        rgb(0x31865a), rgb(0x7c5aa6), rgb(0xf0f0e9),
    };
    for (const TrafficActor& actor : state.traffic) {
        const float distance = distanceBetween(actor.position, state.vehiclePosition);
        if (distance > 240.0f) continue;
        if (actor.kind == 3) {
            const unsigned clothes =
                actor.colorIndex % 2 == 0 ? 0xf0c750 : 0x63a9d8;
            if (distance > 75.0f) {
                addLocalBox(vertices, actor.position, actor.heading,
                            {0, 0.43f, 0}, {0.24f, 0.68f, 0.20f},
                            rgb(clothes));
            } else {
                addPerson(vertices, actor.position, actor.heading, 0.0f, false,
                          clothes);
            }
            continue;
        }
        const Color body = colors[actor.colorIndex % colors.size()];
        const float length = actor.kind == 1 ? 4.1f : (actor.kind == 2 ? 1.5f : 3.7f);
        const float width = actor.kind == 2 ? 0.55f : (actor.kind == 1 ? 1.8f : 1.65f);
        addLocalBox(vertices, actor.position, actor.heading,
                    {0, 0.66f, 0}, {width, 0.72f, length}, body);
        if (distance > 90.0f) continue;
        if (actor.kind == 1) {
            addLocalBox(vertices, actor.position, actor.heading,
                        {0, 1.35f, -0.2f}, {width * 0.93f, 0.72f, length * 0.63f},
                        rgb(0xe5e5df));
        } else if (actor.kind == 0) {
            addLocalBox(vertices, actor.position, actor.heading,
                        {0, 1.12f, -0.15f}, {width * 0.78f, 0.48f, length * 0.46f},
                        rgb(0x9fc3d1));
        }
    }
}

void addRain(std::vector<Vertex>& vertices, const GameState& state,
             float simulationTime) {
    if (!state.rain) return;
    const Color rain = rgb(0x9cc9ed);
    for (int i = 0; i < 440; ++i) {
        const float seed = static_cast<float>(i);
        const float dx = std::fmod(seed * 17.37f + simulationTime * 2.2f, 42.0f) - 21.0f;
        const float dz = std::fmod(seed * 29.13f + simulationTime * 3.8f, 42.0f) - 21.0f;
        const float y = 3.0f + std::fmod(seed * 11.19f + simulationTime * 13.0f, 13.0f);
        const Vec3Like a{state.vehiclePosition.x + dx, y,
                         state.vehiclePosition.z + dz};
        const Vec3Like b{a.x - 0.16f, y - 0.72f, a.z + 0.08f};
        const Vec3Like c{b.x + 0.025f, b.y, b.z + 0.025f};
        const Vec3Like d{a.x + 0.025f, a.y, a.z + 0.025f};
        addQuad(vertices, a, b, c, d, rain);
    }
}

void appendRoadGeometry(Geometry& geometry, const MapData& map) {
    geometry.quad({-1000.0f, -1000.0f}, -0.14f, {1000.0f, -1000.0f}, -0.14f,
                  {1000.0f, 1000.0f}, -0.14f, {-1000.0f, 1000.0f}, -0.14f,
                  rgb(0x8cae68));
    for (const auto& area : map.waterAreas()) {
        if (area.size() < 3) continue;
        for (std::size_t i = 1; i + 1 < area.size(); ++i) {
            geometry.triangle(area[0], -0.055f, area[i], -0.055f,
                              area[i + 1], -0.055f, rgb(0x548fb2));
        }
    }
    for (const RoadSegment& segment : map.segments()) {
        const float dx = segment.b.x - segment.a.x;
        const float dz = segment.b.z - segment.a.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length < 0.05f) continue;
        const Vec2 normal{-dz / length, dx / length};
        const float halfWidth = segment.halfWidth;
        const Vec2 left{normal.x * halfWidth, normal.z * halfWidth};
        const Vec2 sidewalk{normal.x * (halfWidth + 0.85f),
                            normal.z * (halfWidth + 0.85f)};
        geometry.quad({segment.a.x + left.x, segment.a.z + left.z}, 0.025f,
                      {segment.a.x - left.x, segment.a.z - left.z}, 0.025f,
                      {segment.b.x - left.x, segment.b.z - left.z}, 0.025f,
                      {segment.b.x + left.x, segment.b.z + left.z}, 0.025f,
                      rgb(0x45484d));
        geometry.quad({segment.a.x + left.x, segment.a.z + left.z}, 0.052f,
                      {segment.a.x + sidewalk.x, segment.a.z + sidewalk.z}, 0.052f,
                      {segment.b.x + sidewalk.x, segment.b.z + sidewalk.z}, 0.052f,
                      {segment.b.x + left.x, segment.b.z + left.z}, 0.052f,
                      rgb(0xb7b5aa));
        geometry.quad({segment.a.x - left.x, segment.a.z - left.z}, 0.052f,
                      {segment.a.x - sidewalk.x, segment.a.z - sidewalk.z}, 0.052f,
                      {segment.b.x - sidewalk.x, segment.b.z - sidewalk.z}, 0.052f,
                      {segment.b.x - left.x, segment.b.z - left.z}, 0.052f,
                      rgb(0xb7b5aa));
        const Vec2 edgeLeft{normal.x * (halfWidth - 0.14f),
                            normal.z * (halfWidth - 0.14f)};
        const Vec2 edgeRight{-edgeLeft.x, -edgeLeft.z};
        geometry.quad({segment.a.x + edgeLeft.x, segment.a.z + edgeLeft.z}, 0.063f,
                      {segment.a.x + edgeLeft.x * 0.78f, segment.a.z + edgeLeft.z * 0.78f},
                      0.064f,
                      {segment.b.x + edgeLeft.x * 0.78f, segment.b.z + edgeLeft.z * 0.78f},
                      0.064f,
                      {segment.b.x + edgeLeft.x, segment.b.z + edgeLeft.z}, 0.063f,
                      rgb(0xdedbd0));
        geometry.quad({segment.a.x + edgeRight.x * 0.78f, segment.a.z + edgeRight.z * 0.78f},
                      0.064f,
                      {segment.a.x + edgeRight.x, segment.a.z + edgeRight.z}, 0.063f,
                      {segment.b.x + edgeRight.x, segment.b.z + edgeRight.z}, 0.063f,
                      {segment.b.x + edgeRight.x * 0.78f, segment.b.z + edgeRight.z * 0.78f},
                      0.064f, rgb(0xdedbd0));
        if (halfWidth >= 3.0f) {
            const float dashLength = 4.1f;
            const float gapLength = 5.3f;
            for (float offset = 0.0f; offset < length; offset += dashLength + gapLength) {
                const float end = std::min(length, offset + dashLength);
                const Vec2 a{segment.a.x + dx * offset / length,
                             segment.a.z + dz * offset / length};
                const Vec2 b{segment.a.x + dx * end / length,
                             segment.a.z + dz * end / length};
                const Vec2 marking{normal.x * 0.07f, normal.z * 0.07f};
                geometry.quad({a.x + marking.x, a.z + marking.z}, 0.075f,
                              {a.x - marking.x, a.z - marking.z}, 0.075f,
                              {b.x - marking.x, b.z - marking.z}, 0.075f,
                              {b.x + marking.x, b.z + marking.z}, 0.075f,
                              rgb(0xe6c44b));
            }
        }
    }
}

void appendBuildings(Geometry& geometry, const MapData& map) {
    const std::array<Color, 7> walls = {
        rgb(0xe6d2b5), rgb(0xc6d6c4), rgb(0xf0d987), rgb(0xc0d9e5),
        rgb(0xe8bca9), rgb(0xd3d2cb), rgb(0xd9c6a3),
    };
    const std::array<Color, 4> roofs = {
        rgb(0x9b5140), rgb(0x777e85), rgb(0x80463a), rgb(0x556b77),
    };
    for (const BuildingFootprint& building : map.buildings()) {
        if (building.points.size() < 3) continue;
        const Color wall = walls[building.colorIndex % walls.size()];
        const Color roof = roofs[building.colorIndex % roofs.size()];
        for (std::size_t i = 0; i < building.points.size(); ++i) {
            const Vec2 a = building.points[i];
            const Vec2 b = building.points[(i + 1) % building.points.size()];
            geometry.quad(a, 0.0f, b, 0.0f,
                          {b.x, b.z}, building.height,
                          {a.x, a.z}, building.height, wall);
        }
        const Vec2 origin = building.points[0];
        for (std::size_t i = 1; i + 1 < building.points.size(); ++i) {
            geometry.triangle(origin, building.height,
                              building.points[i], building.height,
                              building.points[i + 1], building.height, roof);
        }
    }
}

void appendTree(Geometry& geometry, Vec2 point, float height = 4.0f) {
    geometry.cylinder(point, 0.0f, height * 0.42f, 0.16f, 0.11f,
                      rgb(0x77573b), 6);
    const float y = height * 0.55f;
    for (int level = 0; level < 3; ++level) {
        const float radius = 1.25f - level * 0.28f;
        const float bottom = y + level * 0.65f;
        const float top = bottom + 1.45f;
        for (int i = 0; i < 7; ++i) {
            const float a0 = 2.0f * kPi * i / 7.0f;
            const float a1 = 2.0f * kPi * (i + 1) / 7.0f;
            const Vec2 p0{point.x + std::cos(a0) * radius,
                          point.z + std::sin(a0) * radius};
            const Vec2 p1{point.x + std::cos(a1) * radius,
                          point.z + std::sin(a1) * radius};
            geometry.triangle(p0, bottom, p1, bottom, point, top,
                              level == 0 ? rgb(0x2d7944) : rgb(0x39874b));
        }
    }
}

void appendRoadsideProps(Geometry& geometry, const MapData& map) {
    std::size_t generatedTrees = 0;
    std::size_t generatedLamps = 0;
    for (const Vec2 tree : map.trees()) appendTree(geometry, tree);
    for (const Vec2 pole : map.poles()) {
        geometry.cylinder(pole, 0.0f, 5.8f, 0.08f, 0.035f, rgb(0x69655b), 6);
    }
    for (const Vec2 lamp : map.streetLights()) {
        geometry.cylinder(lamp, 0.0f, 5.2f, 0.09f, 0.04f, rgb(0x585b5e), 6);
        geometry.box({lamp.x + 0.45f, 5.15f, lamp.z}, {0.95f, 0.10f, 0.12f},
                     rgb(0xf4e0a0));
    }
    for (const RoadWay& road : map.roads()) {
        float distanceToNextTree = 25.0f;
        float distanceToNextLamp = 80.0f;
        for (std::size_t i = 1; i < road.points.size(); ++i) {
            const Vec2 a = road.points[i - 1];
            const Vec2 b = road.points[i];
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            const float length = std::sqrt(dx * dx + dz * dz);
            if (length < 0.01f) continue;
            const Vec2 side{-dz / length, dx / length};
            float consumed = 0.0f;
            while (consumed < length) {
                const float step = std::min(distanceToNextTree, length - consumed);
                consumed += step;
                distanceToNextTree -= step;
                if (distanceToNextTree <= 0.01f && generatedTrees < 220) {
                    const float t = std::clamp(consumed / length, 0.0f, 1.0f);
                    const float sign = (generatedTrees % 2 == 0) ? 1.0f : -1.0f;
                    const Vec2 point{
                        a.x + dx * t + side.x * sign * (road.halfWidth + 3.2f),
                        a.z + dz * t + side.z * sign * (road.halfWidth + 3.2f),
                    };
                    appendTree(geometry, point, 3.6f + (generatedTrees % 4) * 0.35f);
                    ++generatedTrees;
                    distanceToNextTree = 25.0f;
                }
            }
            consumed = 0.0f;
            while (consumed < length) {
                const float step = std::min(distanceToNextLamp, length - consumed);
                consumed += step;
                distanceToNextLamp -= step;
                if (distanceToNextLamp <= 0.01f && generatedLamps < 60) {
                    const float t = std::clamp(consumed / length, 0.0f, 1.0f);
                    const float sign = generatedLamps % 2 == 0 ? 1.0f : -1.0f;
                    const Vec2 lamp{
                        a.x + dx * t + side.x * sign * (road.halfWidth + 2.2f),
                        a.z + dz * t + side.z * sign * (road.halfWidth + 2.2f),
                    };
                    geometry.cylinder(lamp, 0.0f, 5.0f, 0.07f, 0.035f,
                                      rgb(0x5b5852), 6);
                    geometry.box({lamp.x + 0.35f, 4.95f, lamp.z},
                                 {0.72f, 0.10f, 0.13f}, rgb(0xf2d891));
                    ++generatedLamps;
                    distanceToNextLamp = 80.0f;
                }
            }
        }
    }
    const auto roadsidePlacement = [&](Vec2 point, Vec2* side,
                                       float* roadHalfWidth, float* heading) {
        float nearest = std::numeric_limits<float>::max();
        *side = {1.0f, 0.0f};
        *roadHalfWidth = 2.0f;
        *heading = 0.0f;
        for (const RoadSegment& segment : map.segments()) {
            const float dx = segment.b.x - segment.a.x;
            const float dz = segment.b.z - segment.a.z;
            const float lengthSquared = dx * dx + dz * dz;
            if (lengthSquared < 0.01f) continue;
            const float t = std::clamp(
                ((point.x - segment.a.x) * dx +
                 (point.z - segment.a.z) * dz) / lengthSquared,
                0.0f, 1.0f);
            const Vec2 projection{segment.a.x + dx * t, segment.a.z + dz * t};
            const float distance = distanceBetween(point, projection);
            if (distance >= nearest) continue;
            nearest = distance;
            const float length = std::sqrt(lengthSquared);
            *side = {-dz / length, dx / length};
            *roadHalfWidth = segment.halfWidth;
            *heading = std::atan2(dx, dz);
        }
    };

    const auto appendTrafficLight = [&](Vec2 point) {
        Vec2 side;
        float roadHalfWidth;
        float heading;
        roadsidePlacement(point, &side, &roadHalfWidth, &heading);
        const float offset = roadHalfWidth + 2.4f;
        const Vec2 pole{point.x + side.x * offset, point.z + side.z * offset};
        geometry.cylinder(pole, 0.0f, 4.2f, 0.075f, 0.05f, rgb(0x4a4c4e), 6);
        addLocalBox(geometry.vertices, pole, heading, {0, 3.73f, 0},
                    {0.36f, 0.91f, 0.26f}, rgb(0x282a2c));
        for (const auto& light : std::array<std::pair<float, unsigned>, 3>{{
                 {4.00f, 0xe84b43}, {3.73f, 0xe4c64a}, {3.46f, 0x49a967},
             }}) {
            addLocalBox(geometry.vertices, pole, heading,
                        {0, light.first, 0.145f}, {0.14f, 0.15f, 0.035f},
                        rgb(light.second));
        }
    };

    std::vector<Vec2> lightLocations;
    for (const TrafficSignal& signal : map.trafficSignals()) {
        lightLocations.push_back(signal.point);
    }
    if (lightLocations.empty()) {
        for (const Vec2 junction : map.intersections()) {
            bool tooClose = false;
            for (const Vec2 placed : lightLocations) {
                if (distanceBetween(junction, placed) < 40.0f) tooClose = true;
            }
            if (tooClose) continue;
            lightLocations.push_back(junction);
            if (lightLocations.size() == 4) break;
        }
    }
    for (const Vec2 light : lightLocations) appendTrafficLight(light);

    std::size_t signCount = 0;
    for (const Vec2 junction : map.intersections()) {
        Vec2 side;
        float roadHalfWidth;
        float heading;
        roadsidePlacement(junction, &side, &roadHalfWidth, &heading);
        const float offset = roadHalfWidth + 2.0f;
        const Vec2 sign{junction.x - side.x * offset,
                        junction.z - side.z * offset};
        geometry.cylinder(sign, 0.0f, 2.45f, 0.035f, 0.025f,
                          rgb(0x73736b), 6);
        addLocalBox(geometry.vertices, sign, heading, {0, 2.23f, 0},
                    {0.72f, 0.55f, 0.09f}, rgb(0xb82626));
        float charX = -0.25f;
        for (const char letter : std::string("PARE")) {
            const auto pixels = glyph(letter);
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (pixels[row][column] != '1') continue;
                    const Vec3Like pixel{
                        charX + static_cast<float>(column) * 0.018f,
                        2.30f - static_cast<float>(row) * 0.026f,
                        0.053f,
                    };
                    addLocalBox(geometry.vertices, sign, heading, pixel,
                                {0.013f, 0.019f, 0.012f}, rgb(0xffffff));
                }
            }
            charX += 0.14f;
        }
        if (++signCount >= 8) break;
    }
}

class Hud {
public:
    std::vector<HudVertex> vertices;
    int width;
    int height;

    Hud(int w, int h) : width(w), height(h) {}

    void rect(float x, float y, float w, float h, Color color, float alpha = 0.92f) {
        const float x0 = 2.0f * x / width - 1.0f;
        const float x1 = 2.0f * (x + w) / width - 1.0f;
        const float y0 = 1.0f - 2.0f * y / height;
        const float y1 = 1.0f - 2.0f * (y + h) / height;
        triangle({x0, y1}, {x1, y1}, {x1, y0}, color, alpha);
        triangle({x0, y1}, {x1, y0}, {x0, y0}, color, alpha);
    }

    void circle(float cx, float cy, float radius, Color color, float alpha = 0.94f) {
        const float x0 = 2.0f * cx / width - 1.0f;
        const float y0 = 1.0f - 2.0f * cy / height;
        for (int i = 0; i < 20; ++i) {
            const float a0 = 2.0f * kPi * i / 20.0f;
            const float a1 = 2.0f * kPi * (i + 1) / 20.0f;
            const float x1 = 2.0f * (cx + std::cos(a0) * radius) / width - 1.0f;
            const float y1 = 1.0f - 2.0f * (cy + std::sin(a0) * radius) / height;
            const float x2 = 2.0f * (cx + std::cos(a1) * radius) / width - 1.0f;
            const float y2 = 1.0f - 2.0f * (cy + std::sin(a1) * radius) / height;
            vertices.push_back({x0, y0, color.r, color.g, color.b, alpha});
            vertices.push_back({x1, y1, color.r, color.g, color.b, alpha});
            vertices.push_back({x2, y2, color.r, color.g, color.b, alpha});
        }
    }

    void line(float x0, float y0, float x1, float y1, float thickness,
              Color color, float alpha = 1.0f) {
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 0.1f) return;
        const float ox = -dy / length * thickness * 0.5f;
        const float oy = dx / length * thickness * 0.5f;
        const float ax = 2.0f * (x0 + ox) / width - 1.0f;
        const float ay = 1.0f - 2.0f * (y0 + oy) / height;
        const float bx = 2.0f * (x0 - ox) / width - 1.0f;
        const float by = 1.0f - 2.0f * (y0 - oy) / height;
        const float cx = 2.0f * (x1 - ox) / width - 1.0f;
        const float cy = 1.0f - 2.0f * (y1 - oy) / height;
        const float dx2 = 2.0f * (x1 + ox) / width - 1.0f;
        const float dy2 = 1.0f - 2.0f * (y1 + oy) / height;
        triangle({ax, ay}, {bx, by}, {cx, cy}, color, alpha);
        triangle({ax, ay}, {cx, cy}, {dx2, dy2}, color, alpha);
    }

    void text(float x, float y, const std::string& value, float scale, Color color,
              float maxWidth = 0.0f) {
        std::size_t maxChars = value.size();
        if (maxWidth > 0.0f) {
            maxChars = std::min(maxChars, static_cast<std::size_t>(
                                                   std::max(1.0f, maxWidth / (scale * 6.0f))));
        }
        float cursorX = x;
        for (std::size_t index = 0; index < maxChars; ++index) {
            const auto rows = glyph(value[index]);
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (rows[row][col] == '1') {
                        rect(cursorX + col * scale, y + row * scale,
                             scale * 0.82f, scale * 0.82f, color, 1.0f);
                    }
                }
            }
            cursorX += scale * 6.0f;
        }
        if (maxChars < value.size() && maxChars > 1) {
            text(cursorX - scale * 6.0f, y, "..", scale, color);
        }
    }

private:
    void triangle(std::array<float, 2> a, std::array<float, 2> b,
                  std::array<float, 2> c, Color color, float alpha) {
        vertices.push_back({a[0], a[1], color.r, color.g, color.b, alpha});
        vertices.push_back({b[0], b[1], color.r, color.g, color.b, alpha});
        vertices.push_back({c[0], c[1], color.r, color.g, color.b, alpha});
    }
};

Color hudColor(unsigned value) {
    return rgb(value);
}

std::string formatDistance(float meters) {
    if (meters >= 1000.0f) {
        char text[24];
        std::snprintf(text, sizeof(text), "%.1f KM", meters / 1000.0f);
        return text;
    }
    return std::to_string(static_cast<int>(std::round(std::max(0.0f, meters)))) +
           " M";
}

void drawButton(Hud& hud, const ControlRect& rect, const std::string& label,
                unsigned color, bool circular = false, float textScale = 2.0f) {
    if (circular) {
        const float radius = (rect.right - rect.left) * 0.5f;
        const float cx = (rect.left + rect.right) * 0.5f;
        const float cy = (rect.top + rect.bottom) * 0.5f;
        hud.circle(cx + 1.5f, cy + 2.0f, radius, hudColor(0x11171d), 0.62f);
        hud.circle(cx, cy, radius - 1.0f, hudColor(color), 0.95f);
        const float textWidth = label.size() * 6.0f * textScale;
        hud.text(cx - textWidth * 0.5f, cy - textScale * 3.5f, label, textScale,
                 hudColor(0xffffff));
    } else {
        hud.rect(rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, hudColor(color), 0.94f);
        const float textWidth = label.size() * 6.0f * textScale;
        const float left = std::max(rect.left + 7.0f,
                                    (rect.left + rect.right - textWidth) * 0.5f);
        hud.text(left, (rect.top + rect.bottom) * 0.5f - textScale * 3.5f,
                 label, textScale, hudColor(0xffffff),
                 rect.right - rect.left - 12.0f);
    }
}

void drawMinimap(Hud& hud, const UiLayout& layout, const MapData& map,
                 const GameState& game) {
    const ControlRect& box = layout.minimap;
    const float size = box.right - box.left;
    hud.rect(box.left - 3, box.top - 3, size + 6, size + 6,
             hudColor(0x10181f), 0.96f);
    hud.rect(box.left, box.top, size, size, hudColor(0x303a41), 0.99f);
    const float scale = size * 0.86f / 150.0f;
    const float cx = box.left + size * 0.5f;
    const float cy = box.top + size * 0.5f;
    const auto screen = [&](Vec2 point) {
        return std::array<float, 2>{
            cx + (point.x - game.vehiclePosition.x) * scale,
            cy + (point.z - game.vehiclePosition.z) * scale,
        };
    };
    for (const RoadSegment& segment : map.segments()) {
        const float maxRange = 82.0f;
        if (std::abs(segment.a.x - game.vehiclePosition.x) > maxRange ||
            std::abs(segment.a.z - game.vehiclePosition.z) > maxRange) {
            continue;
        }
        const auto a = screen(segment.a);
        const auto b = screen(segment.b);
        hud.line(a[0], a[1], b[0], b[1], std::clamp(segment.halfWidth * scale, 2.0f, 5.0f),
                 hudColor(0xaeb9bf), 1.0f);
    }
    for (std::size_t i = 1; i < game.route.size(); ++i) {
        const auto a = screen(game.route[i - 1]);
        const auto b = screen(game.route[i]);
        hud.line(a[0], a[1], b[0], b[1], 3.2f, hudColor(0x3191ff), 1.0f);
    }
    const Vec2 marker = game.phase == MissionPhase::ToDestination
                            ? game.destinationPosition
                            : game.passengerPosition;
    const auto destination = screen(marker);
    const float radius = std::clamp(size * 0.044f, 4.0f, 8.0f);
    hud.circle(destination[0], destination[1], radius,
               game.phase == MissionPhase::ToDestination ? hudColor(0xff4f4f)
                                                         : hudColor(0xffca42));
    hud.circle(cx, cy, std::clamp(size * 0.037f, 4.0f, 7.0f),
               hudColor(0x5dc1ff));
    hud.text(box.left + 4, box.top + size - 11, "OSM", 1.1f,
             hudColor(0xffffff), size - 8);
}

void drawHud(Hud& hud, const UiLayout& layout, const MapData& map,
             const GameState& game) {
    if (!game.mapReady) {
        hud.rect(0, 0, hud.width, hud.height, hudColor(0x101820), 1.0f);
        hud.text(20, hud.height * 0.34f, "MAP DATA FAILED", 3.0f,
                 hudColor(0xff6262), hud.width - 40.0f);
        hud.text(20, hud.height * 0.46f, "NO FICTIONAL ROADS SUBSTITUTED", 1.4f,
                 hudColor(0xffffff), hud.width - 40.0f);
        return;
    }

    const float topScale = layout.portrait ? 1.2f : 1.45f;
    hud.rect(layout.mission.left, layout.mission.top,
             layout.mission.right - layout.mission.left,
             layout.mission.bottom - layout.mission.top, hudColor(0x111921), 0.86f);
        hud.text(layout.mission.left + 8, layout.mission.top + 8,
             game.missionText, topScale, hudColor(0xffcf53),
             layout.mission.right - layout.mission.left - 16);
    hud.text(layout.mission.left + 8, layout.mission.top + 28,
             "PASSENGER: " + game.passengerText, topScale, hudColor(0xffffff),
             layout.mission.right - layout.mission.left - 16);
    if (!game.navigationText.empty() && game.started &&
        game.phase != MissionPhase::Complete) {
        std::string routeText = "GPS " + formatDistance(game.distanceToGoal) +
                                "  " + game.navigationText;
        hud.text(layout.mission.left + 8, layout.mission.top + 48,
                 routeText, topScale * 0.78f, hudColor(0x9bd5ff),
                 layout.mission.right - layout.mission.left - 16);
    }

    hud.rect(layout.speed.left, layout.speed.top,
             layout.speed.right - layout.speed.left,
             layout.speed.bottom - layout.speed.top, hudColor(0x111921), 0.92f);
    char speedText[16];
    std::snprintf(speedText, sizeof(speedText), "%02d",
                  static_cast<int>(std::round(std::abs(game.speed) * 3.6f)));
    const float speedScale = layout.portrait ? 2.4f : 2.9f;
    hud.text(layout.speed.left + 9, layout.speed.top + 5, speedText,
             speedScale, hudColor(0xffffff));
    hud.text(layout.speed.left + 9, layout.speed.top + 31, "KM/H",
             layout.portrait ? 1.0f : 1.2f, hudColor(0xbac6cc));
    hud.rect(layout.coins.left, layout.coins.top,
             layout.coins.right - layout.coins.left,
             layout.coins.bottom - layout.coins.top, hudColor(0x111921), 0.84f);
    hud.text(layout.coins.left + 5, layout.coins.top + 5,
             "COINS " + std::to_string(game.coins),
             layout.portrait ? 1.2f : 1.4f, hudColor(0xffd35a),
             layout.coins.right - layout.coins.left - 10);
    drawMinimap(hud, layout, map, game);

    if (!game.started) {
        hud.rect(0, 0, hud.width, hud.height, hudColor(0x0c1823), 0.70f);
        hud.text(hud.width * 0.5f - (layout.portrait ? 85.0f : 146.0f),
                 hud.height * 0.24f, "IMISUBEBE", layout.portrait ? 3.0f : 4.2f,
                 hudColor(0xffca42));
        hud.text(hud.width * 0.5f - (layout.portrait ? 72.0f : 132.0f),
                 hud.height * 0.24f + (layout.portrait ? 36.0f : 49.0f),
                 "E-TRICYCLE SIMULATOR", layout.portrait ? 1.5f : 2.0f,
                 hudColor(0xffffff));
        hud.text(hud.width * 0.5f - (layout.portrait ? 85.0f : 174.0f),
                 hud.height * 0.42f,
                 "REAL CEBU OSM ROADS  |  OFFLINE MAP INCLUDED",
                 layout.portrait ? 1.2f : 1.7f, hudColor(0xcfe1eb),
                 hud.width * 0.86f);
        for (const ControlId id : {ControlId::Start, ControlId::MenuSettings}) {
            const auto* control = layout.control(id);
            if (!control) continue;
            drawButton(hud, *control,
                       id == ControlId::Start ? "START MISSION" : "SETTINGS",
                       id == ControlId::Start ? 0x28865a : 0x394b59,
                       false, layout.portrait ? 1.45f : 1.75f);
        }
    } else if (game.settingsOpen) {
        hud.rect(0, 0, hud.width, hud.height, hudColor(0x0b1118), 0.82f);
        const float panelW = std::min(hud.width * 0.78f, 430.0f);
        const float panelX = (hud.width - panelW) * 0.5f;
        const float panelY = hud.height * 0.16f;
        hud.rect(panelX, panelY, panelW, hud.height * 0.68f,
                 hudColor(0x18242d), 0.98f);
        hud.text(panelX + 18, panelY + 18, "SETTINGS", 2.2f,
                 hudColor(0xffca42), panelW - 35);
        for (const ControlId id : {ControlId::DayNight, ControlId::Rain,
                                   ControlId::CloseSettings}) {
            const auto* control = layout.control(id);
            if (!control) continue;
            std::string label;
            unsigned color = 0x38515f;
            if (id == ControlId::DayNight) {
                label = game.timeOfDay > 18.0f || game.timeOfDay < 6.0f
                            ? "DAY LIGHTING"
                            : "NIGHT LIGHTING";
                color = 0x765329;
            } else if (id == ControlId::Rain) {
                label = game.rain ? "STOP RAIN" : "START RAIN";
                color = game.rain ? 0x355e82 : 0x38515f;
            } else {
                label = "BACK TO GAME";
            }
            drawButton(hud, *control, label, color, false,
                       layout.portrait ? 1.25f : 1.6f);
        }
        hud.text(panelX + 12.0f, panelY + hud.height * 0.68f - 24.0f,
                 "MAP DATA: OPENSTREETMAP CONTRIBUTORS - ODbL 1.0",
                 0.88f, hudColor(0xcad6dc), panelW - 24.0f);
    } else if (game.paused) {
        hud.rect(0, 0, hud.width, hud.height, hudColor(0x0b1118), 0.82f);
        hud.text(hud.width * 0.5f - 63.0f, hud.height * 0.22f,
                 "PAUSED", 3.0f, hudColor(0xffffff));
        const auto* resume = layout.control(ControlId::Resume);
        const auto* restart = layout.control(ControlId::Restart);
        const auto* settings = layout.control(ControlId::MenuSettings);
        if (resume) drawButton(hud, *resume, "RESUME", 0x28865a, false, 1.7f);
        if (restart) drawButton(hud, *restart, "RESTART MISSION", 0x78463c,
                                false, 1.45f);
        if (settings) drawButton(hud, *settings, "SETTINGS", 0x394b59,
                                 false, 1.4f);
    } else if (game.phase == MissionPhase::Complete) {
        hud.rect(0, 0, hud.width, hud.height, hudColor(0x0b1118), 0.78f);
        hud.text(hud.width * 0.5f - 100.0f, hud.height * 0.24f,
                 "MISSION COMPLETE", 2.4f, hudColor(0xffd15b),
                 hud.width - 40.0f);
        hud.text(hud.width * 0.5f - 62.0f, hud.height * 0.34f,
                 "+250 COINS", 2.0f, hudColor(0xffffff));
        const auto* replay = layout.control(ControlId::Replay);
        if (replay) drawButton(hud, *replay, "REPLAY MISSION", 0x28865a,
                               false, 1.5f);
    } else {
        const bool portrait = layout.portrait;
        for (const auto& entry : std::array<std::pair<ControlId, std::pair<const char*, unsigned>>, 7>{{
                 {ControlId::Left, {"LEFT", 0x39434a}},
                 {ControlId::Right, {"RIGHT", 0x39434a}},
                 {ControlId::Gas, {"GAS", 0x239861}},
                 {ControlId::Brake, {"BRAKE", 0xb9463f}},
                 {ControlId::Reverse, {"REVERSE", 0x545967}},
                 {ControlId::Action, {game.phase == MissionPhase::ToPickup ? "PICK UP" : "DROP OFF",
                                      0x23845a}},
                 {ControlId::Camera, {"CAMERA", 0x38515f}},
             }}) {
            const auto* control = layout.control(entry.first);
            if (!control) continue;
            const bool circular = entry.first == ControlId::Left ||
                                  entry.first == ControlId::Right ||
                                  entry.first == ControlId::Gas ||
                                  entry.first == ControlId::Brake ||
                                  entry.first == ControlId::Reverse;
            const std::string label =
                entry.first == ControlId::Reverse
                    ? (game.reverseGear ? "REV ON" : "REVERSE")
                    : entry.second.first;
            const unsigned buttonColor =
                entry.first == ControlId::Reverse && game.reverseGear
                    ? 0x9a6528
                    : entry.second.second;
            const float labelScale = (entry.first == ControlId::Reverse)
                                         ? (portrait ? 1.0f : 1.1f)
                                         : (portrait ? 1.15f : 1.35f);
            drawButton(hud, *control, label, buttonColor,
                       circular, labelScale);
        }
        for (const auto& entry : std::array<std::pair<ControlId, const char*>, 3>{{
                 {ControlId::Horn, "HORN"},
                 {ControlId::Settings, "SET"},
                 {ControlId::Pause, "II"},
             }}) {
            const auto* control = layout.control(entry.first);
            if (control) drawButton(hud, *control, entry.second, 0x34454f,
                                    false, portrait ? 1.05f : 1.2f);
        }
    }

    if (game.toastSeconds > 0.0f && !game.toast.empty()) {
        const float w = std::min(hud.width * 0.8f,
                                 std::max(220.0f, game.toast.size() * 7.0f));
        const float x = (hud.width - w) * 0.5f;
        const float y = layout.portrait ? hud.height * 0.39f : hud.height * 0.29f;
        hud.rect(x, y, w, 40.0f, hudColor(0x152128), 0.95f);
        hud.text(x + 9.0f, y + 9.0f, game.toast,
                 layout.portrait ? 1.05f : 1.25f, hudColor(0xffffff), w - 18.0f);
    }
    if (!game.started) {
        hud.text(8.0f, hud.height - 18.0f, "MAP DATA: OPENSTREETMAP CONTRIBUTORS",
                 1.0f, hudColor(0xffffff), hud.width - 16.0f);
    }
    if (game.started && !game.settingsOpen) {
        hud.text(layout.minimap.left + 1.0f, layout.minimap.bottom + 3.0f,
                 "OPENSTREETMAP CONTRIBUTORS", 0.82f, hudColor(0xffffff),
                 layout.minimap.right - layout.minimap.left + 38.0f);
    }
}
}  // namespace

const ControlRect* UiLayout::control(ControlId id) const {
    for (const ControlRect& item : controls) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

float UiLayout::find(ControlId id) const {
    const auto* item = control(id);
    return item ? item->left : -1.0f;
}

UiLayout makeUiLayout(int width, int height) {
    UiLayout layout;
    width = std::max(width, 1);
    height = std::max(height, 1);
    layout.portrait = height > width;
    const float shortSide = static_cast<float>(std::min(width, height));
    const float margin = std::clamp(shortSide * 0.035f, 10.0f, 22.0f);
    const float mapSize = std::clamp(shortSide * (layout.portrait ? 0.285f : 0.285f),
                                     92.0f, 168.0f);
    layout.minimap = {ControlId::Gas, margin, margin, margin + mapSize, margin + mapSize};
    const float speedW = std::clamp(shortSide * 0.25f, 90.0f, 126.0f);
    const float speedH = std::clamp(shortSide * 0.16f, 58.0f, 78.0f);
    layout.speed = {ControlId::Gas, width - margin - speedW, margin,
                    width - margin, margin + speedH};
    layout.coins = {ControlId::Gas, width - margin - speedW, margin + speedH + 5.0f,
                    width - margin, margin + speedH + 30.0f};
    const float missionLeft = layout.minimap.right + margin;
    const float missionRight = std::max(missionLeft + 75.0f, layout.speed.left - margin);
    layout.mission = {ControlId::Gas, missionLeft, margin,
                      missionRight, margin + (layout.portrait ? 82.0f : 78.0f)};

    const float controlSize = std::clamp(shortSide * 0.19f, 60.0f, 96.0f);
    const float smallSize = controlSize * 0.80f;
    const float bottom = height - margin;
    const float leftX = margin;
    const float rightX = leftX + controlSize + margin * 0.55f;
    layout.controls = {
        {ControlId::Left, leftX, bottom - controlSize, leftX + controlSize, bottom},
        {ControlId::Right, rightX, bottom - controlSize, rightX + controlSize, bottom},
        {ControlId::Gas, width - margin - controlSize, bottom - controlSize,
         width - margin, bottom},
        {ControlId::Brake, width - margin - controlSize,
         bottom - controlSize * 2.1f - margin * 0.45f,
         width - margin, bottom - controlSize * 1.1f - margin * 0.45f},
        {ControlId::Reverse, width - margin - controlSize * 2.0f - margin * 0.4f,
         bottom - controlSize * 0.76f,
         width - margin - controlSize - margin * 0.4f,
         bottom - controlSize * 0.02f},
        {ControlId::Action, width - margin - controlSize * 2.35f,
         height * 0.52f, width - margin, height * 0.52f + 48.0f},
        {ControlId::Camera, width - margin - smallSize * 3.0f - 8.0f,
         margin + speedH + 40.0f,
         width - margin - smallSize * 2.0f - 8.0f,
         margin + speedH + 40.0f + 36.0f},
        {ControlId::Horn, width - margin - smallSize * 2.0f - 4.0f,
         margin + speedH + 40.0f,
         width - margin - smallSize - 4.0f,
         margin + speedH + 40.0f + 36.0f},
        {ControlId::Settings, width - margin - smallSize,
         margin + speedH + 40.0f,
         width - margin,
         margin + speedH + 40.0f + 36.0f},
        {ControlId::Pause, width - margin - smallSize,
         margin + speedH + 82.0f,
         width - margin,
         margin + speedH + 82.0f + 36.0f},
    };

    const float panelW = std::min(width * 0.78f, 430.0f);
    const float panelX = (width - panelW) * 0.5f;
    const float menuButtonW = std::min(panelW - 32.0f, 320.0f);
    const float buttonH = layout.portrait ? 50.0f : 58.0f;
    const float menuY = height * 0.59f;
    layout.controls.push_back({ControlId::Start, panelX + (panelW - menuButtonW) * 0.5f,
                               menuY, panelX + (panelW + menuButtonW) * 0.5f,
                               menuY + buttonH});
    layout.controls.push_back({ControlId::MenuSettings,
                               panelX + (panelW - menuButtonW) * 0.5f,
                               menuY + buttonH + 14.0f,
                               panelX + (panelW + menuButtonW) * 0.5f,
                               menuY + buttonH * 2.0f + 14.0f});
    const float overlayTop = height * 0.37f;
    layout.controls.push_back({ControlId::Resume, panelX + 16.0f, overlayTop,
                               panelX + panelW - 16.0f, overlayTop + buttonH});
    layout.controls.push_back({ControlId::Restart, panelX + 16.0f,
                               overlayTop + buttonH + 12.0f,
                               panelX + panelW - 16.0f,
                               overlayTop + buttonH * 2.0f + 12.0f});
    layout.controls.push_back({ControlId::Replay, panelX + 16.0f,
                               overlayTop + buttonH + 12.0f,
                               panelX + panelW - 16.0f,
                               overlayTop + buttonH * 2.0f + 12.0f});
    const float settingsY = height * 0.37f;
    const float settingsH = buttonH;
    layout.controls.push_back({ControlId::DayNight, panelX + 16.0f, settingsY,
                               panelX + panelW - 16.0f, settingsY + settingsH});
    layout.controls.push_back({ControlId::Rain, panelX + 16.0f,
                               settingsY + settingsH + 12.0f,
                               panelX + panelW - 16.0f,
                               settingsY + settingsH * 2.0f + 12.0f});
    layout.controls.push_back({ControlId::CloseSettings, panelX + 16.0f,
                               settingsY + settingsH * 2.0f + 24.0f,
                               panelX + panelW - 16.0f,
                               settingsY + settingsH * 3.0f + 24.0f});
    return layout;
}

UiLayout Renderer::layout(int width, int height) const {
    return makeUiLayout(width, height);
}

bool Renderer::initialize(const MapData& map) {
    release();
    const char* worldVertex =
        "#version 300 es\n"
        "layout(location=0) in vec3 aPosition;\n"
        "layout(location=1) in vec3 aColor;\n"
        "uniform mat4 uMvp;\n"
        "uniform float uLight;\n"
        "out vec3 vColor;\n"
        "void main(){ gl_Position=uMvp*vec4(aPosition,1.0);"
        "vColor=aColor*uLight; }\n";
    const char* worldFragment =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec3 vColor;\n"
        "out vec4 outColor;\n"
        "void main(){ outColor=vec4(vColor,1.0); }\n";
    const char* hudVertex =
        "#version 300 es\n"
        "layout(location=0) in vec2 aPosition;\n"
        "layout(location=1) in vec4 aColor;\n"
        "out vec4 vColor;\n"
        "void main(){ gl_Position=vec4(aPosition,0.0,1.0); vColor=aColor; }\n";
    const char* hudFragment =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 vColor;\n"
        "out vec4 outColor;\n"
        "void main(){ outColor=vColor; }\n";
    worldProgram_ = linkProgram(worldVertex, worldFragment);
    hudProgram_ = linkProgram(hudVertex, hudFragment);
    if (!worldProgram_ || !hudProgram_) {
        release();
        return false;
    }

    Geometry geometry;
    appendRoadGeometry(geometry, map);
    appendBuildings(geometry, map);
    appendRoadsideProps(geometry, map);
    staticVertices_.reserve(geometry.vertices.size() * 6);
    for (const Vertex& vertex : geometry.vertices) {
        staticVertices_.insert(staticVertices_.end(),
                               {vertex.x, vertex.y, vertex.z,
                                vertex.r, vertex.g, vertex.b});
    }
    staticVertexCount_ = static_cast<GLuint>(geometry.vertices.size());
    glGenBuffers(1, &worldVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, worldVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(staticVertices_.size() * sizeof(float)),
                 staticVertices_.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &hudVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ready_ = glGetError() == GL_NO_ERROR;
    return ready_;
}

void Renderer::release() {
    if (worldVbo_) glDeleteBuffers(1, &worldVbo_);
    if (hudVbo_) glDeleteBuffers(1, &hudVbo_);
    if (worldProgram_) glDeleteProgram(worldProgram_);
    if (hudProgram_) glDeleteProgram(hudProgram_);
    worldVbo_ = 0;
    hudVbo_ = 0;
    worldProgram_ = 0;
    hudProgram_ = 0;
    staticVertexCount_ = 0;
    staticVertices_.clear();
    ready_ = false;
}

void Renderer::draw(const MapData& map, const GameState& game, int width, int height) {
    if (width <= 0 || height <= 0) return;
    const bool isNight = game.timeOfDay < 6.0f || game.timeOfDay > 18.0f;
    if (isNight) glClearColor(0.035f, 0.075f, 0.14f, 1.0f);
    else glClearColor(0.48f, 0.72f, 0.88f, 1.0f);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (ready_ && game.mapReady) {
        const float yaw = game.heading;
        Vec3Like eye;
        Vec3Like target;
        const auto point = [&](float x, float y, float z) {
            return worldPoint(game.vehiclePosition, yaw, {x, y, z},
                              game.suspension + 0.35f);
        };
        if (game.cameraMode == 1) {
            eye = point(0.0f, 1.55f, 6.4f);
            target = point(0.0f, 0.95f, -3.0f);
        } else if (game.cameraMode == 2) {
            eye = point(0.0f, 1.48f, 0.28f);
            target = point(0.0f, 1.5f, 14.0f);
        } else if (game.cameraMode == 3) {
            const float distance = 9.0f;
            const float cosPitch = std::cos(game.freeCameraPitch);
            const float orbitYaw = game.freeCameraYaw;
            eye = {
                game.vehiclePosition.x + std::sin(orbitYaw) * cosPitch * distance,
                1.0f + std::sin(game.freeCameraPitch) * distance,
                game.vehiclePosition.z + std::cos(orbitYaw) * cosPitch * distance,
            };
            target = {game.vehiclePosition.x, 1.0f, game.vehiclePosition.z};
        } else {
            eye = point(0.0f, 2.85f, -7.2f);
            target = point(0.0f, 0.9f, 7.5f);
        }
        const auto projection = perspective(63.0f * kDegToRad,
                                            static_cast<float>(width) / height,
                                            0.1f, 650.0f);
        const auto view = lookAt(eye, target);
        const auto mvp = multiply(projection, view);
        glUseProgram(worldProgram_);
        glUniformMatrix4fv(glGetUniformLocation(worldProgram_, "uMvp"), 1, GL_FALSE,
                           mvp.data());
        const float light = isNight ? 0.64f : 1.0f;
        glUniform1f(glGetUniformLocation(worldProgram_, "uLight"), light);
        glBindBuffer(GL_ARRAY_BUFFER, worldVbo_);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, staticVertexCount_);

        std::vector<Vertex>& dynamicVertices = dynamicVertices_;
        dynamicVertices.clear();
        if (dynamicVertices.capacity() < 7000) dynamicVertices.reserve(7000);
        addTraffic(dynamicVertices, game);
        addVehicle(dynamicVertices, game);
        if (game.phase == MissionPhase::ToPickup) {
            addPerson(dynamicVertices, game.passengerPosition, 0.0f, 0.0f, false,
                      0xdcdcf5);
        }
        addRain(dynamicVertices, game, game.elapsedSeconds);
        if (!dynamicVertices.empty()) {
            std::vector<float>& packed = packedDynamicVertices_;
            packed.clear();
            if (packed.capacity() < dynamicVertices.size() * 6) {
                packed.reserve(dynamicVertices.size() * 6);
            }
            for (const Vertex& vertex : dynamicVertices) {
                packed.insert(packed.end(),
                              {vertex.x, vertex.y, vertex.z,
                               vertex.r, vertex.g, vertex.b});
            }
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                         packed.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0,
                         static_cast<GLsizei>(dynamicVertices.size()));
        }
    }

    const UiLayout uiLayout = makeUiLayout(width, height);
    Hud hud(width, height);
    drawHud(hud, uiLayout, map, game);
    if (!hud.vertices.empty()) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(hudProgram_);
        glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(hud.vertices.size() * sizeof(HudVertex)),
                     hud.vertices.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex), nullptr);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                              reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(hud.vertices.size()));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
