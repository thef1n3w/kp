#include "GameState.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <queue>
#include <random>
#include <string>

// ============================================================================
// Transport hubs and JSON persistence
// ============================================================================


static const char* FindMatching(const char* open, char openChar, char closeChar) {
    if (!open || *open != openChar) return nullptr;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (const char* p = open; *p; ++p) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (*p == '\\') {
                escaped = true;
            } else if (*p == '"') {
                inString = false;
            }
            continue;
        }

        if (*p == '"') {
            inString = true;
        } else if (*p == openChar) {
            ++depth;
        } else if (*p == closeChar) {
            --depth;
            if (depth == 0) return p;
        }
    }

    return nullptr;
}

static int CaseInsensitiveCompare(const char* a, const char* b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        unsigned char ca = (unsigned char)std::tolower((unsigned char)*a);
        unsigned char cb = (unsigned char)std::tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        ++a;
        ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static const char* FindKey(const char* begin, const char* end, const char* key) {
    const char* p = begin;
    while (p && p < end) {
        p = strstr(p, key);
        if (!p || p >= end) return nullptr;
        return p;
    }
    return nullptr;
}

static bool ReadStringField(const char* begin, const char* end, const char* key, char* out, int outSize) {
    if (!out || outSize <= 0) return false;
    out[0] = '\0';

    const char* keyPos = FindKey(begin, end, key);
    if (!keyPos) return false;
    const char* colon = strchr(keyPos, ':');
    if (!colon || colon >= end) return false;
    const char* q1 = strchr(colon, '"');
    if (!q1 || q1 >= end) return false;
    ++q1;
    const char* q2 = q1;
    while (q2 < end && *q2) {
        if (*q2 == '"' && (q2 == q1 || q2[-1] != '\\')) break;
        ++q2;
    }
    if (!q2 || q2 >= end) return false;

    int len = (int)(q2 - q1);
    if (len >= outSize) len = outSize - 1;
    std::strncpy(out, q1, len);
    out[len] = '\0';
    return true;
}

static bool ReadFloatField(const char* begin, const char* end, const char* key, float* out) {
    if (!out) return false;
    const char* keyPos = FindKey(begin, end, key);
    if (!keyPos) return false;
    const char* colon = strchr(keyPos, ':');
    if (!colon || colon >= end) return false;
    return sscanf(colon + 1, " %f", out) == 1;
}

static bool ReadBoolField(const char* begin, const char* end, const char* key, bool* out) {
    if (!out) return false;
    const char* keyPos = FindKey(begin, end, key);
    if (!keyPos) return false;
    const char* colon = strchr(keyPos, ':');
    if (!colon || colon >= end) return false;

    const char* value = colon + 1;
    while (value < end && std::isspace((unsigned char)*value)) ++value;
    if (value + 4 <= end && std::strncmp(value, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (value + 5 <= end && std::strncmp(value, "false", 5) == 0) {
        *out = false;
        return true;
    }

    int intValue = 0;
    if (sscanf(value, " %d", &intValue) == 1) {
        *out = intValue != 0;
        return true;
    }
    return false;
}

static std::string JsonEscape(const char* text) {
    std::string out;
    if (!text) return out;
    for (const char* p = text; *p; ++p) {
        switch (*p) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += *p; break;
        }
    }
    return out;
}

const char* TransportHubTypeToString(TransportHubType type) {
    return type == HUB_PORT ? "port" : "airport";
}

TransportHubType TransportHubTypeFromString(const char* text) {
    if (!text) return HUB_AIRPORT;
    if (CaseInsensitiveCompare(text, "port") == 0) return HUB_PORT;
    return HUB_AIRPORT;
}

void AddTransportHub(Region& region, TransportHubType type, Vector2 mapPosition, const char* name, bool closed) {
    TransportHub hub = {};
    hub.type = type;
    hub.mapPosition = mapPosition;
    hub.closed = closed;
    if (name && name[0] != '\0') {
        std::strncpy(hub.name, name, sizeof(hub.name) - 1);
    } else {
        std::snprintf(hub.name, sizeof(hub.name), "%s", TransportHubTypeToString(type));
    }
    hub.name[sizeof(hub.name) - 1] = '\0';
    region.transportHubs.push_back(hub);
}

int CountTransportHubs(const Region& region, TransportHubType type, bool onlyOpen) {
    int count = 0;
    for (const TransportHub& hub : region.transportHubs) {
        if (hub.type != type) continue;
        if (onlyOpen && hub.closed) continue;
        ++count;
    }
    return count;
}

int PickRandomTransportHubIndex(const Region& region, TransportHubType type, bool onlyOpen) {
    std::vector<int> indices;
    for (int i = 0; i < (int)region.transportHubs.size(); ++i) {
        const TransportHub& hub = region.transportHubs[i];
        if (hub.type != type) continue;
        if (onlyOpen && hub.closed) continue;
        indices.push_back(i);
    }
    if (indices.empty()) return -1;
    return indices[GetRandomValue(0, (int)indices.size() - 1)];
}

int CountRegionsWithOpenHub(const std::vector<Region>& regions, TransportHubType type) {
    int count = 0;
    for (const Region& region : regions) {
        if (CountTransportHubs(region, type, true) > 0) ++count;
    }
    return count;
}

static int MatchRegionIndex(const std::vector<Region>& regions, const char* name) {
    for (int i = 0; i < (int)regions.size(); ++i) {
        if (CaseInsensitiveCompare(regions[i].name, name) == 0) return i;
    }
    return -1;
}

bool LoadTransportHubsFromJSON(std::vector<Region>& regions, const char* path) {
    char* text = LoadFileText(path);
    if (!text) return false;

    for (Region& region : regions) {
        region.transportHubs.clear();
    }

    const char* rootKey = strstr(text, "\"regions\"");
    const char* regionsStart = rootKey ? strchr(rootKey, '[') : nullptr;
    const char* regionsEnd = regionsStart ? FindMatching(regionsStart, '[', ']') : nullptr;
    bool loadedAny = false;

    if (!regionsStart || !regionsEnd) {
        UnloadFileText(text);
        return false;
    }

    const char* cur = regionsStart + 1;
    while (cur && cur < regionsEnd) {
        const char* objStart = strchr(cur, '{');
        if (!objStart || objStart >= regionsEnd) break;
        const char* objEnd = FindMatching(objStart, '{', '}');
        if (!objEnd || objEnd > regionsEnd) break;

        char regionName[128] = {};
        if (!ReadStringField(objStart, objEnd, "\"name\"", regionName, sizeof(regionName))) {
            cur = objEnd + 1;
            continue;
        }
        int regionIndex = MatchRegionIndex(regions, regionName);
        if (regionIndex < 0) {
            cur = objEnd + 1;
            continue;
        }

        const char* hubsKey = FindKey(objStart, objEnd, "\"hubs\"");
        if (!hubsKey) {
            cur = objEnd + 1;
            continue;
        }
        const char* arrStart = strchr(hubsKey, '[');
        const char* arrEnd = arrStart ? FindMatching(arrStart, '[', ']') : nullptr;
        if (!arrStart || !arrEnd || arrEnd > objEnd) {
            cur = objEnd + 1;
            continue;
        }

        const char* hubCur = arrStart + 1;
        while (hubCur && hubCur < arrEnd) {
            const char* hubStart = strchr(hubCur, '{');
            if (!hubStart || hubStart >= arrEnd) break;
            const char* hubEnd = FindMatching(hubStart, '{', '}');
            if (!hubEnd || hubEnd > arrEnd) break;

            char typeText[32] = {};
            TransportHub hub = {};
            if (ReadStringField(hubStart, hubEnd, "\"type\"", typeText, sizeof(typeText))) {
                hub.type = TransportHubTypeFromString(typeText);
            } else {
                hub.type = HUB_AIRPORT;
            }
            hub.closed = false;

            if (!ReadFloatField(hubStart, hubEnd, "\"x\"", &hub.mapPosition.x) ||
                !ReadFloatField(hubStart, hubEnd, "\"y\"", &hub.mapPosition.y)) {
                hubCur = hubEnd + 1;
                continue;
            }
            ReadStringField(hubStart, hubEnd, "\"name\"", hub.name, sizeof(hub.name));
            ReadBoolField(hubStart, hubEnd, "\"closed\"", &hub.closed);

            hub.mapPosition.x = std::max(0.0f, std::min(1.0f, hub.mapPosition.x));
            hub.mapPosition.y = std::max(0.0f, std::min(1.0f, hub.mapPosition.y));

            if (hub.name[0] == '\0') {
                std::snprintf(hub.name, sizeof(hub.name), "%s %d", TransportHubTypeToString(hub.type), CountTransportHubs(regions[regionIndex], hub.type) + 1);
            }

            regions[regionIndex].transportHubs.push_back(hub);
            loadedAny = true;
            hubCur = hubEnd + 1;
        }
        cur = objEnd + 1;
    }

    UnloadFileText(text);
    return loadedAny;
}

bool SaveTransportHubsToJSON(const std::vector<Region>& regions, const char* path) {
    std::string out;
    out += "{\n  \"regions\": [\n";

    bool firstRegion = true;
    for (const Region& region : regions) {
        if (region.transportHubs.empty()) continue;

        if (!firstRegion) out += ",\n";
        firstRegion = false;

        out += "    { \"name\": \"";
        out += JsonEscape(region.name);
        out += "\", \"hubs\": [\n";

        bool firstHub = true;
        for (const TransportHub& hub : region.transportHubs) {
            if (!firstHub) out += ",\n";
            firstHub = false;

            out += "      { \"type\": \"";
            out += TransportHubTypeToString(hub.type);
            out += "\", \"name\": \"";
            out += JsonEscape(hub.name);
            out += "\", \"x\": ";
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.5f", hub.mapPosition.x);
            out += buffer;
            out += ", \"y\": ";
            std::snprintf(buffer, sizeof(buffer), "%.5f", hub.mapPosition.y);
            out += buffer;
            out += ", \"closed\": ";
            out += hub.closed ? "true" : "false";
            out += " }";
        }

        out += "\n    ] }";
    }

    out += "\n  ]\n}\n";
    std::vector<char> buffer(out.begin(), out.end());
    buffer.push_back('\0');
    return SaveFileText(path, buffer.data());
}

// ============================================================================
// Regions and daily simulation
// ============================================================================


static Color InfectionColor(float infectionRate) {
    if (infectionRate <= 0.001f) return {50, 220, 90, 60};
    if (infectionRate < 0.30f) return {255, 225, 65, 80};
    if (infectionRate < 0.70f) return {255, 135, 35, 100};
    return {235, 35, 35, 120};
}

static bool IsPlaceholderRegion(const Region& region) {
    return std::strcmp(region.name, "ASD") == 0;
}

// ─── Minimal JSON loader ─────────────────────────────────────
// Parses array of { "name": "...", "x": 0.0, "y": 0.0, "population": 0 }
bool LoadRegionsFromJSON(std::vector<Region>& regions, const char* path) {
    char* text = LoadFileText(path);
    if (!text) return false;

    regions.clear();
    const char* cur = text;

    while ((cur = strstr(cur, "\"name\"")) != nullptr) {
        Region r = {};

        // name
        const char* colon = strchr(cur, ':');
        if (!colon) break;
        const char* q1 = strchr(colon, '"');
        if (!q1) break;
        q1++;
        const char* q2 = strchr(q1, '"');
        if (!q2) break;
        int len = (int)(q2 - q1);
        if (len > 127) len = 127;
        strncpy(r.name, q1, len);
        r.name[len] = '\0';
        cur = q2 + 1;

        // find closing brace of this object
        const char* objEnd = strchr(cur, '}');
        if (!objEnd) break;

        // x
        const char* xp = strstr(cur, "\"x\"");
        if (xp && xp < objEnd) {
            xp = strchr(xp, ':');
            if (xp) sscanf(xp + 1, " %f", &r.mapPosition.x);
        }

        // y
        const char* yp = strstr(cur, "\"y\"");
        if (yp && yp < objEnd) {
            yp = strchr(yp, ':');
            if (yp) sscanf(yp + 1, " %f", &r.mapPosition.y);
        }

        // population
        const char* pp = strstr(cur, "\"population\"");
        if (pp && pp < objEnd) {
            pp = strchr(pp, ':');
            if (pp) sscanf(pp + 1, " %d", &r.population);
        }

        r.infectionRate = 0.0f;
        r.cureProgress  = 0.0f;
        r.infected      = 0;
        r.dead          = 0;
        r.isSelected    = false;
        r.overlayColor  = {0};

        regions.push_back(r);
        cur = objEnd + 1;
    }

    UnloadFileText(text);
    return !regions.empty();
}

static void AddHardcodedRegion(std::vector<Region>& regions,
                               const char* name, float x, float y, int pop) {
    Region r = {};
    strncpy(r.name, name, 127);
    r.name[127] = '\0';
    r.mapPosition = {x, y};
    r.population  = pop;
    regions.push_back(r);
}

static int PickRandomIndex(int count) {
    if (count <= 0) return -1;
    thread_local static std::mt19937 rng([] {
        std::random_device rd;
        auto seed = (unsigned int)rd() ^
                    (unsigned int)std::random_device{}() ^
                    (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::mt19937(seed);
    }());
    std::uniform_int_distribution<int> dist(0, count - 1);
    return dist(rng);
}

void InitializeRegions(std::vector<Region>& regions) {
    std::string jsonPath = ResourcePath("regions.json");
    if (!LoadRegionsFromJSON(regions, jsonPath.c_str())) {
        // fallback
        regions.clear();
        AddHardcodedRegion(regions, u8"Северная Америка",   0.17f, 0.28f, 590);
        AddHardcodedRegion(regions, u8"Центральная Америка",0.19f, 0.45f, 180);
        AddHardcodedRegion(regions, u8"Южная Америка",      0.27f, 0.68f, 430);
        AddHardcodedRegion(regions, u8"Зап. Европа",        0.47f, 0.26f, 420);
        AddHardcodedRegion(regions, u8"Вост. Европа",       0.54f, 0.22f, 290);
        AddHardcodedRegion(regions, u8"Сев. Африка",        0.50f, 0.48f, 250);
        AddHardcodedRegion(regions, u8"Центр. Африка",      0.52f, 0.64f, 1100);
        AddHardcodedRegion(regions, u8"Ближний Восток",     0.58f, 0.42f, 270);
        AddHardcodedRegion(regions, u8"Центр. Азия",        0.65f, 0.28f, 260);
        AddHardcodedRegion(regions, u8"Южная Азия",         0.68f, 0.47f, 1900);
        AddHardcodedRegion(regions, u8"Восточная Азия",     0.78f, 0.32f, 1650);
        AddHardcodedRegion(regions, u8"Океания",            0.83f, 0.72f, 45);
    }

    std::vector<int> candidates;
    for (int i = 0; i < (int)regions.size(); ++i) {
        if (regions[i].population > 0 && !IsPlaceholderRegion(regions[i])) {
            candidates.push_back(i);
        }
    }

    int starter = candidates.empty() ? 0 : candidates[PickRandomIndex((int)candidates.size())];
    if (starter < 0 || starter >= (int)regions.size()) starter = 0;
    regions[starter].infectionRate = 0.15f;
    regions[starter].infected = (int)((double)regions[starter].population * (double)regions[starter].infectionRate);
    UpdateAllRegionOverlayColors(regions);
}

void UpdateRegionOverlayColor(Region& region) {
    region.overlayColor = InfectionColor(region.infectionRate);
}

void UpdateAllRegionOverlayColors(std::vector<Region>& regions) {
    for (Region& region : regions) UpdateRegionOverlayColor(region);
}

void SimulateRegionDay(Region& region, float spreadMultiplier, float cureMultiplier, float mortalityMultiplier, bool vaccineReady) {
    if (region.infectionRate <= 0.0f) return;

    if (vaccineReady) {
        region.cureProgress = 1.0f;
        int recovered = std::max(1, (int)std::ceil((double)region.infected * 0.085 * (double)cureMultiplier));
        region.infected = std::max(0, region.infected - recovered);
        region.infectionRate = region.population > 0 ? (float)((double)region.infected / (double)region.population) : 0.0f;
        if (region.infected == 0) region.infectionRate = 0.0f;
        UpdateRegionOverlayColor(region);
        return;
    }

    float naturalSpread = region.infectionRate * 0.016f * spreadMultiplier * (1.0f - region.cureProgress);
    region.infectionRate = std::min(1.0f, region.infectionRate + naturalSpread);

    int newInfected = (int)((double)region.population * (double)naturalSpread);
    region.infected = std::min(region.population, region.infected + std::max(0, newInfected));
    region.dead = std::min(region.infected, region.dead + std::max(0, (int)((double)region.infected * 0.001 * mortalityMultiplier)));
    region.cureProgress = std::min(1.0f, region.cureProgress + 0.0025f * cureMultiplier);
    UpdateRegionOverlayColor(region);
}

float GetRegionRadius(const Region& region, float screenScale) {
    float normalizedPopulation = std::min(1.0f, (float)region.population / 340110988.0f);
    normalizedPopulation = sqrtf(normalizedPopulation);
    return (22.0f + normalizedPopulation * 34.0f) * screenScale;
}

int GetRegionAtScreenPosition(const std::vector<Region>& regions, Vector2 mouse, int screenWidth, int screenHeight) {
    float screenScale = std::min((float)screenWidth / 1920.0f, (float)screenHeight / 1080.0f);
    if (screenScale < 0.65f) screenScale = 0.65f;

    for (int i = (int)regions.size() - 1; i >= 0; --i) {
        Vector2 center = {regions[i].mapPosition.x * screenWidth, regions[i].mapPosition.y * screenHeight};
        float radius = GetRegionRadius(regions[i], screenScale) + 8.0f;
        if (CheckCollisionPointCircle(mouse, center, radius)) return i;
    }

    return -1;
}

long long GetTotalPopulationPeople(const std::vector<Region>& regions) {
    long long total = 0;
    for (const Region& region : regions) total += region.population;
    return total;
}

long long GetTotalInfected(const std::vector<Region>& regions) {
    long long total = 0;
    for (const Region& region : regions) total += region.infected;
    return total;
}

long long GetTotalDead(const std::vector<Region>& regions) {
    long long total = 0;
    for (const Region& region : regions) total += region.dead;
    return total;
}

bool AreAllRegionsCured(const std::vector<Region>& regions) {
    for (const Region& region : regions) {
        if (region.cureProgress < 1.0f) return false;
    }
    return true;
}

// ============================================================================
// Vehicles and sea routes
// ============================================================================


static bool HasOpenHub(const Region& region, TransportHubType type) {
    return CountTransportHubs(region, type, true) > 0;
}

static Vector2 GetHubPositionOrCenter(const Region& region, TransportHubType type, int hubIndex) {
    if (hubIndex >= 0 && hubIndex < (int)region.transportHubs.size()) {
        return region.transportHubs[hubIndex].mapPosition;
    }
    return region.mapPosition;
}

static std::vector<int> RegionsWithOpenHub(const std::vector<Region>& regions, TransportHubType type) {
    std::vector<int> candidates;
    for (int i = 0; i < (int)regions.size(); ++i) {
        if (HasOpenHub(regions[i], type)) candidates.push_back(i);
    }
    return candidates;
}

static bool PointInPolygon(const std::vector<Vector2>& polygon, Vector2 point) {
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vector2& a = polygon[i];
        const Vector2& b = polygon[j];
        bool intersects = ((a.y > point.y) != (b.y > point.y)) &&
                          (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 1e-6f) + a.x);
        if (intersects) inside = !inside;
    }
    return inside;
}

static bool IsLand(Vector2 point, const std::vector<std::vector<Vector2>>& landPolygons) {
    for (const std::vector<Vector2>& polygon : landPolygons) {
        if (polygon.size() >= 3 && PointInPolygon(polygon, point)) return true;
    }
    return false;
}

static std::vector<Vector2> BuildSeaRoute(Vector2 start, Vector2 goal, const std::vector<std::vector<Vector2>>& landPolygons) {
    constexpr int gridW = 96;
    constexpr int gridH = 54;
    constexpr int cellCount = gridW * gridH;
    auto index = [gridW](int x, int y) { return y * gridW + x; };
    auto center = [gridW, gridH](int x, int y) {
        return Vector2{((float)x + 0.5f) / (float)gridW, ((float)y + 0.5f) / (float)gridH};
    };

    std::vector<unsigned char> blocked(cellCount, 0);
    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            if (IsLand(center(x, y), landPolygons)) blocked[index(x, y)] = 1;
        }
    }

    // Keep ships visibly away from coastlines.
    std::vector<unsigned char> inflated = blocked;
    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            if (!blocked[index(x, y)]) continue;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < gridW && ny >= 0 && ny < gridH) inflated[index(nx, ny)] = 1;
                }
            }
        }
    }
    blocked.swap(inflated);

    auto nearestWater = [&](Vector2 point) {
        int baseX = std::clamp((int)(point.x * gridW), 0, gridW - 1);
        int baseY = std::clamp((int)(point.y * gridH), 0, gridH - 1);
        for (int radius = 0; radius < std::max(gridW, gridH); ++radius) {
            int best = -1;
            float bestDistance = std::numeric_limits<float>::max();
            for (int y = std::max(0, baseY - radius); y <= std::min(gridH - 1, baseY + radius); ++y) {
                for (int x = std::max(0, baseX - radius); x <= std::min(gridW - 1, baseX + radius); ++x) {
                    if (blocked[index(x, y)]) continue;
                    Vector2 c = center(x, y);
                    float dx = c.x - point.x;
                    float dy = c.y - point.y;
                    float distance = dx * dx + dy * dy;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = index(x, y);
                    }
                }
            }
            if (best >= 0) return best;
        }
        return -1;
    };

    int startCell = nearestWater(start);
    int goalCell = nearestWater(goal);
    if (startCell < 0 || goalCell < 0) return {};

    struct OpenNode {
        float score;
        int cell;
        bool operator<(const OpenNode& other) const { return score > other.score; }
    };

    std::priority_queue<OpenNode> open;
    std::vector<float> cost(cellCount, std::numeric_limits<float>::max());
    std::vector<int> previous(cellCount, -1);
    cost[startCell] = 0.0f;
    open.push({0.0f, startCell});

    static const int directions[8][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    };

    while (!open.empty()) {
        int current = open.top().cell;
        open.pop();
        if (current == goalCell) break;
        int cx = current % gridW;
        int cy = current / gridW;

        for (const auto& direction : directions) {
            int nx = cx + direction[0];
            int ny = cy + direction[1];
            if (nx < 0 || nx >= gridW || ny < 0 || ny >= gridH) continue;
            int next = index(nx, ny);
            if (blocked[next]) continue;

            float step = (direction[0] != 0 && direction[1] != 0) ? 1.4142f : 1.0f;
            float nextCost = cost[current] + step;
            if (nextCost >= cost[next]) continue;
            cost[next] = nextCost;
            previous[next] = current;

            int gx = goalCell % gridW;
            int gy = goalCell / gridW;
            float heuristic = std::hypot((float)(gx - nx), (float)(gy - ny));
            open.push({nextCost + heuristic, next});
        }
    }

    if (previous[goalCell] < 0 && startCell != goalCell) return {};

    std::vector<Vector2> reversed;
    for (int cell = goalCell; cell >= 0; cell = previous[cell]) {
        reversed.push_back(center(cell % gridW, cell / gridW));
        if (cell == startCell) break;
    }
    std::reverse(reversed.begin(), reversed.end());

    std::vector<Vector2> route;
    route.push_back(start);
    for (size_t i = 0; i < reversed.size(); i += 3) route.push_back(reversed[i]);
    if (route.size() < 2 || route.back().x != reversed.back().x || route.back().y != reversed.back().y) {
        route.push_back(reversed.back());
    }
    route.push_back(goal);
    return route;
}

static void SetCurrentRouteSegment(Vehicle& vehicle) {
    if (vehicle.routeSegment < 0 || vehicle.routeSegment + 1 >= (int)vehicle.route.size()) return;
    vehicle.origin = vehicle.route[vehicle.routeSegment];
    vehicle.destination = vehicle.route[vehicle.routeSegment + 1];
}

static bool SpawnVehicle(Vehicle& vehicle, const std::vector<Region>& regions, const std::vector<std::vector<Vector2>>& landPolygons, bool isPlane) {
    vehicle = {};
    vehicle.isPlane = isPlane;
    vehicle.active = false;
    vehicle.progress = 0.0f;
    vehicle.routeSegment = 0;
    vehicle.fromHub = -1;
    vehicle.toHub = -1;
    vehicle.fromRegion = -1;
    vehicle.toRegion = -1;

    if (regions.empty()) return false;

    TransportHubType hubType = isPlane ? HUB_AIRPORT : HUB_PORT;
    std::vector<int> candidates = RegionsWithOpenHub(regions, hubType);
    if (candidates.size() < 2) return false;

    vehicle.fromRegion = candidates[GetRandomValue(0, (int)candidates.size() - 1)];
    do {
        vehicle.toRegion = candidates[GetRandomValue(0, (int)candidates.size() - 1)];
    } while (vehicle.toRegion == vehicle.fromRegion);

    vehicle.fromHub = PickRandomTransportHubIndex(regions[vehicle.fromRegion], hubType, true);
    vehicle.toHub = PickRandomTransportHubIndex(regions[vehicle.toRegion], hubType, true);
    if (vehicle.fromHub < 0 || vehicle.toHub < 0) return false;

    Vector2 start = GetHubPositionOrCenter(regions[vehicle.fromRegion], hubType, vehicle.fromHub);
    Vector2 goal = GetHubPositionOrCenter(regions[vehicle.toRegion], hubType, vehicle.toHub);
    vehicle.route = isPlane ? std::vector<Vector2>{start, goal} : BuildSeaRoute(start, goal, landPolygons);
    if (vehicle.route.size() < 2) return false;
    SetCurrentRouteSegment(vehicle);

    float routeLength = 0.0f;
    for (size_t i = 1; i < vehicle.route.size(); ++i) {
        routeLength += std::hypot(vehicle.route[i].x - vehicle.route[i - 1].x,
                                  vehicle.route[i].y - vehicle.route[i - 1].y);
    }
    if (routeLength <= 0.0001f) return false;

    if (isPlane) {
        vehicle.speed = routeLength / (float)GetRandomValue(8, 12);
    } else {
        vehicle.speed = routeLength / (float)GetRandomValue(20, 30);
    }

    float randomScale = (float)GetRandomValue(80, 120) / 100.0f;
    vehicle.speed *= randomScale;
    vehicle.carryingVirus = regions[vehicle.fromRegion].infectionRate > 0.30f;
    vehicle.position = {
        vehicle.origin.x + (vehicle.destination.x - vehicle.origin.x) * vehicle.progress,
        vehicle.origin.y + (vehicle.destination.y - vehicle.origin.y) * vehicle.progress
    };
    vehicle.active = true;
    return true;
}

static void ApplyVehicleTransmission(const Vehicle& vehicle, std::vector<Region>& regions, float transmissionMultiplier) {
    if (!vehicle.active) return;
    if (!vehicle.carryingVirus) return;
    if (vehicle.toRegion < 0 || vehicle.toRegion >= (int)regions.size()) return;
    if (regions[vehicle.toRegion].cureProgress >= 1.0f) return;
    if (vehicle.toHub >= 0 && vehicle.toHub < (int)regions[vehicle.toRegion].transportHubs.size()) {
        if (regions[vehicle.toRegion].transportHubs[vehicle.toHub].closed) return;
    }

    int transmissionChance = std::clamp((int)(30.0f * transmissionMultiplier), 1, 95);
    if (GetRandomValue(0, 100) < transmissionChance) {
        regions[vehicle.toRegion].infectionRate += 0.05f * transmissionMultiplier;
        if (regions[vehicle.toRegion].infectionRate > 1.0f) regions[vehicle.toRegion].infectionRate = 1.0f;
        int maxPeople = regions[vehicle.toRegion].population;
        regions[vehicle.toRegion].infected += std::max(1000, (int)((double)regions[vehicle.toRegion].population * 0.005));
        if (regions[vehicle.toRegion].infected > maxPeople) regions[vehicle.toRegion].infected = maxPeople;
        UpdateRegionOverlayColor(regions[vehicle.toRegion]);
    }
}

void InitializeVehiclePool(std::vector<Vehicle>& vehicles, const std::vector<Region>& regions, const std::vector<std::vector<Vector2>>& landPolygons) {
    int planeRegions = CountRegionsWithOpenHub(regions, HUB_AIRPORT);
    int portRegions = CountRegionsWithOpenHub(regions, HUB_PORT);
    int planeCount = planeRegions >= 2 ? std::min(20, planeRegions * 3) : 0;
    int shipCount = portRegions >= 2 ? std::min(15, portRegions * 2) : 0;
    int vehiclePoolSize = planeCount + shipCount;

    vehicles.clear();
    if (vehiclePoolSize <= 0) return;
    vehicles.resize(vehiclePoolSize);

    for (int i = 0; i < vehiclePoolSize; ++i) {
        SpawnVehicle(vehicles[i], regions, landPolygons, i < planeCount);
    }
}

void UpdateVehicles(std::vector<Vehicle>& vehicles, std::vector<Region>& regions, const std::vector<std::vector<Vector2>>& landPolygons, float delta, int speedMultiplier, float transmissionMultiplier) {
    for (Vehicle& vehicle : vehicles) {
        if (!vehicle.active) continue;
        float segmentLength = std::hypot(vehicle.destination.x - vehicle.origin.x,
                                         vehicle.destination.y - vehicle.origin.y);
        if (segmentLength <= 0.0001f) segmentLength = 0.0001f;
        vehicle.progress += (vehicle.speed / segmentLength) * delta * (float)speedMultiplier;

        if (vehicle.progress >= 1.0f) {
            vehicle.progress -= 1.0f;
            ++vehicle.routeSegment;
            if (vehicle.routeSegment + 1 >= (int)vehicle.route.size()) {
                ApplyVehicleTransmission(vehicle, regions, transmissionMultiplier);
                bool isPlane = vehicle.isPlane;
                if (!SpawnVehicle(vehicle, regions, landPolygons, isPlane)) {
                    vehicle.active = false;
                    continue;
                }
            } else {
                SetCurrentRouteSegment(vehicle);
            }
        }

        vehicle.position = {
            vehicle.origin.x + (vehicle.destination.x - vehicle.origin.x) * vehicle.progress,
            vehicle.origin.y + (vehicle.destination.y - vehicle.origin.y) * vehicle.progress
        };
    }
}

static void DrawPlaneFallback(Vector2 pos, float angle, Color color) {
    DrawPoly(pos, 3, 12.0f, angle * RAD2DEG + 90.0f, color);
    DrawLineEx(pos, {pos.x - cosf(angle) * 16.0f, pos.y - sinf(angle) * 16.0f}, 3.0f, color);
}

static void DrawShipFallback(Vector2 pos, float angle, Color color) {
    Vector2 a = {pos.x + cosf(angle) * 12.0f, pos.y + sinf(angle) * 12.0f};
    Vector2 b = {pos.x + cosf(angle + 2.35f) * 10.0f, pos.y + sinf(angle + 2.35f) * 10.0f};
    Vector2 c = {pos.x + cosf(angle - 2.35f) * 10.0f, pos.y + sinf(angle - 2.35f) * 10.0f};
    DrawTriangle(a, b, c, color);
}

void DrawVehicles(const std::vector<Vehicle>& vehicles, Texture2D planeIcon, bool planeIconLoaded, Texture2D shipIcon, bool shipIconLoaded, Rectangle mapRect) {
    float sw = mapRect.width;
    float sh = mapRect.height;

    for (const Vehicle& vehicle : vehicles) {
        if (!vehicle.active) continue;
        Vector2 pos = {mapRect.x + vehicle.position.x * sw, mapRect.y + vehicle.position.y * sh};
        Vector2 dest = {mapRect.x + vehicle.destination.x * sw, mapRect.y + vehicle.destination.y * sh};
        Vector2 origin = {mapRect.x + vehicle.origin.x * sw, mapRect.y + vehicle.origin.y * sh};
        float angle = atan2f(dest.y - origin.y, dest.x - origin.x);
        Color color = vehicle.carryingVirus ? Color{255, 90, 90, 245} : Color{255, 255, 255, 230};

        for (int i = 1; i <= 5; ++i) {
            float trailProgress = vehicle.progress - 0.012f * (float)i;
            if (trailProgress < 0.0f) continue;
            Vector2 trail = {
                mapRect.x + (vehicle.origin.x + (vehicle.destination.x - vehicle.origin.x) * trailProgress) * sw,
                mapRect.y + (vehicle.origin.y + (vehicle.destination.y - vehicle.origin.y) * trailProgress) * sh
            };
            DrawCircleV(trail, vehicle.isPlane ? 2.5f : 2.0f, {255, 255, 255, (unsigned char)(70 - i * 11)});
        }

        if (vehicle.isPlane) {
            if (planeIconLoaded) {
                Rectangle src = {0, 0, (float)planeIcon.width, (float)planeIcon.height};
                Rectangle dst = {pos.x, pos.y, 28.0f, 28.0f};
                DrawTexturePro(planeIcon, src, dst, {14.0f, 14.0f}, angle * RAD2DEG + 90.0f, color);
            } else {
                DrawPlaneFallback(pos, angle, color);
            }
        } else {
            if (shipIconLoaded) {
                Rectangle src = {0, 0, (float)shipIcon.width, (float)shipIcon.height};
                Rectangle dst = {pos.x, pos.y, 24.0f, 24.0f};
                DrawTexturePro(shipIcon, src, dst, {12.0f, 12.0f}, angle * RAD2DEG + 90.0f, color);
            } else {
                DrawShipFallback(pos, angle, color);
            }
        }
    }
}

// ============================================================================
// Gameplay, laboratory, HUD and input
// ============================================================================


static Rectangle GetMapRect(const GameplayState& state);
static Vector2 MapPointToScreen(Vector2 mapPoint, Rectangle mapRect);

// ─── User-drawn polygons loader and hit-test ─────────────────────

static bool PointInPolygon(const std::vector<Vector2>& poly, float x, float y) {
    if (poly.size() < 3) return false;
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = poly[i].x, yi = poly[i].y;
        float xj = poly[j].x, yj = poly[j].y;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

static void LoadUserPolygons(GameplayState& state) {
    if (state.polygonsLoaded) return;
    state.polygonsLoaded = true;
    std::string path = ResourcePath("regions_drawn.json");
    if (!FileExists(path.c_str())) return;

    char* text = LoadFileText(path.c_str());
    if (!text) return;
    const char* cur = text;

    state.regionPolygons.assign(state.regions.size(), {});

    auto matchName = [&](const char* name) -> int {
        for (int i = 0; i < (int)state.regions.size(); ++i) {
            if (_stricmp(state.regions[i].name, name) == 0) return i;
        }
        return -1;
    };

    while ((cur = strstr(cur, "\"name\"")) != nullptr) {
        const char* colon = strchr(cur, ':');
        if (!colon) break;
        const char* q1 = strchr(colon, '"');
        if (!q1) break;
        q1++;
        const char* q2 = strchr(q1, '"');
        if (!q2) break;
        std::string name(q1, q2 - q1);
        cur = q2 + 1;

        int idx = matchName(name.c_str());

        const char* pts = strstr(cur, "points");
        if (!pts) continue;
        const char* bracket = strchr(pts, '[');
        if (!bracket) continue;
        const char* end = strchr(bracket, ']');
        if (!end) break;

        std::vector<Vector2> poly;
        const char* p = bracket;
        while ((p = strstr(p, "\"x\"")) && p < end) {
            const char* xcol = strchr(p, ':');
            if (!xcol) break;
            float x = 0, y = 0;
            sscanf(xcol + 1, " %f", &x);
            const char* ykey = strstr(p, "\"y\"");
            if (!ykey || ykey > end) break;
            const char* ycol = strchr(ykey, ':');
            if (!ycol) break;
            sscanf(ycol + 1, " %f", &y);
            poly.push_back({x, y});
            p = ycol + 1;
        }

        if (poly.size() >= 3) {
            if (idx >= 0 && idx < (int)state.regionPolygons.size()) {
                state.regionPolygons[idx] = poly;

                // Update region center to polygon centroid for tooltips/markers
                float cx = 0.0f, cy = 0.0f;
                for (auto& pnt : poly) { cx += pnt.x; cy += pnt.y; }
                cx /= (float)poly.size();
                cy /= (float)poly.size();
                state.regions[idx].mapPosition = {cx, cy};
            }
        }
        cur = end;
    }

    UnloadFileText(text);
}

static void LoadSeaObstacles(GameplayState& state) {
    std::string path = ResourcePath("sea_obstacles.json");
    if (!FileExists(path.c_str())) return;

    char* text = LoadFileText(path.c_str());
    if (!text) return;
    const char* cur = text;

    while ((cur = strstr(cur, "\"points\"")) != nullptr) {
        const char* bracket = strchr(cur, '[');
        if (!bracket) break;
        const char* end = strchr(bracket, ']');
        if (!end) break;

        std::vector<Vector2> polygon;
        const char* p = bracket;
        while ((p = strstr(p, "\"x\"")) && p < end) {
            const char* xColon = strchr(p, ':');
            const char* yKey = strstr(p, "\"y\"");
            const char* yColon = yKey ? strchr(yKey, ':') : nullptr;
            if (!xColon || !yKey || yKey >= end || !yColon) break;

            Vector2 point = {};
            sscanf(xColon + 1, " %f", &point.x);
            sscanf(yColon + 1, " %f", &point.y);
            polygon.push_back(point);
            p = yColon + 1;
        }

        if (polygon.size() >= 3) state.seaObstacles.push_back(polygon);
        cur = end + 1;
    }

    UnloadFileText(text);
}

static const std::vector<std::vector<Vector2>>& GetSeaObstacles(const GameplayState& state) {
    return state.seaObstacles.empty() ? state.regionPolygons : state.seaObstacles;
}

// ═══════════════════════════════════════════════════════════════
// PROCEDURAL WORLD MAP — simplified continent polygons
// ═══════════════════════════════════════════════════════════════

struct ContinentPoly {
    Vector2 points[32];
    int count;
    Color color;
};

static ContinentPoly GetNorthAmerica() {
    return {{
        {0.05f,0.18f},{0.08f,0.14f},{0.14f,0.10f},{0.22f,0.08f},{0.26f,0.12f},
        {0.28f,0.18f},{0.27f,0.24f},{0.26f,0.30f},{0.24f,0.36f},{0.22f,0.40f},
        {0.20f,0.42f},{0.18f,0.38f},{0.15f,0.34f},{0.12f,0.30f},{0.10f,0.26f},
        {0.07f,0.22f}
    }, 16, {95, 115, 65, 255}};
}

static ContinentPoly GetCentralAm() {
    return {{
        {0.18f,0.42f},{0.20f,0.42f},{0.22f,0.45f},{0.21f,0.50f},{0.20f,0.54f},
        {0.18f,0.56f},{0.16f,0.53f},{0.15f,0.48f},{0.16f,0.45f}
    }, 9, {105, 125, 68, 255}};
}

static ContinentPoly GetSouthAmerica() {
    return {{
        {0.20f,0.56f},{0.23f,0.54f},{0.27f,0.56f},{0.30f,0.60f},{0.32f,0.66f},
        {0.31f,0.72f},{0.29f,0.78f},{0.27f,0.84f},{0.25f,0.88f},{0.23f,0.90f},
        {0.22f,0.86f},{0.21f,0.80f},{0.20f,0.74f},{0.19f,0.66f},{0.19f,0.60f}
    }, 15, {85, 120, 55, 255}};
}

static ContinentPoly GetEurope() {
    return {{
        {0.44f,0.22f},{0.46f,0.18f},{0.49f,0.16f},{0.52f,0.14f},{0.56f,0.15f},
        {0.58f,0.18f},{0.56f,0.22f},{0.54f,0.26f},{0.52f,0.30f},{0.50f,0.32f},
        {0.48f,0.34f},{0.46f,0.32f},{0.44f,0.28f},{0.43f,0.25f}
    }, 14, {100, 118, 70, 255}};
}

static ContinentPoly GetAfrica() {
    return {{
        {0.44f,0.38f},{0.47f,0.36f},{0.50f,0.38f},{0.54f,0.40f},{0.57f,0.44f},
        {0.58f,0.50f},{0.57f,0.56f},{0.56f,0.62f},{0.54f,0.68f},{0.52f,0.74f},
        {0.50f,0.76f},{0.48f,0.74f},{0.46f,0.70f},{0.44f,0.64f},{0.43f,0.56f},
        {0.42f,0.48f},{0.43f,0.42f}
    }, 17, {140, 130, 85, 255}};
}

static ContinentPoly GetMiddleEastAsia() {
    return {{
        {0.58f,0.26f},{0.62f,0.22f},{0.66f,0.18f},{0.72f,0.16f},{0.78f,0.14f},
        {0.82f,0.16f},{0.85f,0.20f},{0.86f,0.26f},{0.84f,0.32f},{0.82f,0.36f},
        {0.78f,0.40f},{0.74f,0.44f},{0.70f,0.48f},{0.66f,0.50f},{0.62f,0.48f},
        {0.60f,0.44f},{0.58f,0.38f},{0.57f,0.32f}
    }, 18, {110, 128, 72, 255}};
}

static ContinentPoly GetSouthAsia() {
    return {{
        {0.62f,0.38f},{0.66f,0.36f},{0.70f,0.38f},{0.73f,0.42f},{0.74f,0.48f},
        {0.72f,0.52f},{0.69f,0.54f},{0.66f,0.52f},{0.64f,0.48f},{0.62f,0.44f}
    }, 10, {90, 125, 60, 255}};
}

static ContinentPoly GetOceania() {
    return {{
        {0.78f,0.60f},{0.82f,0.58f},{0.86f,0.60f},{0.90f,0.64f},{0.91f,0.70f},
        {0.89f,0.76f},{0.86f,0.78f},{0.82f,0.76f},{0.79f,0.72f},{0.77f,0.66f}
    }, 10, {140, 135, 85, 255}};
}

static void DrawFilledPolygon(const Vector2* points, int count, Color color, float sw, float sh) {
    if (count < 3) return;
    Vector2 center = {0, 0};
    for (int i = 0; i < count; ++i) {
        center.x += points[i].x * sw;
        center.y += points[i].y * sh;
    }
    center.x /= (float)count;
    center.y /= (float)count;

    for (int i = 0; i < count; ++i) {
        int next = (i + 1) % count;
        Vector2 a = {points[i].x * sw, points[i].y * sh};
        Vector2 b = {points[next].x * sw, points[next].y * sh};
        DrawTriangle(center, a, b, color);
        DrawTriangle(center, b, a, color);
    }
}

static void DrawPanel(Rectangle panel, Color accent = {70, 195, 255, 255}, bool selected = false) {
    float radius = 0.12f;
    Color shadow = selected ? Color{0, 0, 0, 110} : Color{0, 0, 0, 82};
    Color border = selected ? Color{accent.r, accent.g, accent.b, 110} : Color{255, 255, 255, 38};
    Color fill = selected ? Color{18, 23, 30, 226} : Color{14, 18, 24, 206};

    DrawRectangleRounded({panel.x, panel.y + 6.0f, panel.width, panel.height}, radius, 18, shadow);
    DrawRectangleRounded(panel, radius, 24, border);
    Rectangle inner = {panel.x + 1.0f, panel.y + 1.0f, panel.width - 2.0f, panel.height - 2.0f};
    DrawRectangleRounded(inner, radius, 24, fill);

    DrawLineEx({inner.x + 14.0f, inner.y + 1.0f}, {inner.x + inner.width - 14.0f, inner.y + 1.0f},
               1.0f, {255, 255, 255, selected ? (unsigned char)42 : (unsigned char)24});
    if (selected) {
        DrawRectangleRounded({inner.x + 10.0f, inner.y + 10.0f, 3.0f, inner.height - 20.0f},
                             0.9f, 8, {accent.r, accent.g, accent.b, 150});
    }
}

static void DrawMiniBar(Rectangle rect, float value, Color color) {
    value = std::clamp(value, 0.0f, 1.0f);
    DrawRectangleRounded(rect, 0.45f, 12, {255, 255, 255, 32});
    Rectangle inner = {rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f};
    DrawRectangleRounded(inner, 0.45f, 12, {8, 12, 17, 218});
    if (value > 0.01f) {
        Color muted = {
            (unsigned char)((float)color.r * 0.82f + 24.0f),
            (unsigned char)((float)color.g * 0.82f + 24.0f),
            (unsigned char)((float)color.b * 0.82f + 24.0f),
            225
        };
        DrawRectangleRounded({inner.x, inner.y, inner.width * value, inner.height}, 0.45f, 12, muted);
    }
}

static void DrawMetric(const char* label, const char* value, Vector2 pos, Color valueColor) {
    DrawTextUI(label, pos, 12.5f, {150, 158, 170, 238});
    DrawTextUI(value, {pos.x, pos.y + 17.0f}, 20.0f, valueColor);
}

static float EaseOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static float DrawWrappedTextUI(const char* text, Rectangle bounds, float fontSize, Color color, float lineGap = 7.0f) {
    std::string source = text ? text : "";
    std::string line;
    float y = bounds.y;
    size_t pos = 0;

    auto flushLine = [&]() {
        if (!line.empty()) {
            DrawTextUI(line.c_str(), {bounds.x, y}, fontSize, color);
            y += fontSize + lineGap;
            line.clear();
        }
    };

    while (pos < source.size()) {
        while (pos < source.size() && source[pos] == ' ') ++pos;
        size_t next = source.find(' ', pos);
        std::string word = source.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        if (word.empty()) break;

        std::string candidate = line.empty() ? word : line + " " + word;
        if (MeasureTextUI(candidate.c_str(), fontSize).x > bounds.width && !line.empty()) {
            flushLine();
            line = word;
        } else {
            line = candidate;
        }

        if (next == std::string::npos) break;
        pos = next + 1;
    }

    flushLine();
    return y - bounds.y;
}

static void DrawProceduralMap(int sw, int sh) {
    DrawRectangleGradientV(0, 0, sw, sh, {28, 75, 100, 255}, {15, 40, 62, 255});

    float fSw = (float)sw;
    float fSh = (float)sh;

    ContinentPoly continents[] = {
        GetNorthAmerica(), GetCentralAm(), GetSouthAmerica(),
        GetEurope(), GetAfrica(), GetMiddleEastAsia(),
        GetSouthAsia(), GetOceania()
    };

    for (auto& continent : continents) {
        DrawFilledPolygon(continent.points, continent.count, continent.color, fSw, fSh);
        for (int i = 0; i < continent.count; ++i) {
            int next = (i + 1) % continent.count;
            DrawLineEx(
                {continent.points[i].x * fSw, continent.points[i].y * fSh},
                {continent.points[next].x * fSw, continent.points[next].y * fSh},
                1.5f, {60, 80, 45, 180}
            );
        }
    }

    for (int i = 0; i < 6; ++i) {
        float x = fSw * (0.1f + 0.15f * i);
        DrawLineEx({x, 0}, {x, fSh}, 0.5f, {255, 255, 255, 12});
    }
    for (int i = 0; i < 4; ++i) {
        float y = fSh * (0.2f + 0.2f * i);
        DrawLineEx({0, y}, {fSw, y}, 0.5f, {255, 255, 255, 12});
    }
}

// ═══════════════════════════════════════════════════════════════
// NEWS
// ═══════════════════════════════════════════════════════════════

static std::string CurrentNewsText() {
    const char* newsMessages[] = {
        u8"Новая вспышка обнаружена в {region}!",
        u8"Учёные близки к созданию вакцины...",
        u8"ВОЗ объявила чрезвычайное положение",
        u8"Аэропорты {region} закрыты на карантин",
        u8"Поставки медикаментов задержаны"
    };

    GameplayState& state = gGameState.gameplay;
    std::string text = newsMessages[state.newsIndex % 5];
    std::string region = state.regions.empty() ? u8"МИР" : state.regions[state.newsIndex % state.regions.size()].name;
    size_t marker = text.find("{region}");
    if (marker != std::string::npos) text.replace(marker, 8, region);
    return text;
}

static void PushNotification(const char* text, Color accent, float duration = 6.0f) {
    GameplayState& state = gGameState.gameplay;
    Notification item = {};
    std::strncpy(item.text, text, sizeof(item.text) - 1);
    item.text[sizeof(item.text) - 1] = '\0';
    item.age = 0.0f;
    item.duration = duration;
    item.accent = accent;

    state.notifications.insert(state.notifications.begin(), item);
    if (state.notifications.size() > 5) state.notifications.pop_back();
}

static void PushNewsNotification() {
    std::string text = CurrentNewsText();
    PushNotification(text.c_str(), {70, 195, 255, 255}, 6.5f);
}

static void UpdateNotifications(float delta) {
    GameplayState& state = gGameState.gameplay;
    for (Notification& item : state.notifications) item.age += delta;
    state.notifications.erase(
        std::remove_if(state.notifications.begin(), state.notifications.end(),
            [](const Notification& item) { return item.age >= item.duration; }),
        state.notifications.end()
    );
}

static void PushModalAlert(const char* title, const char* body, Color accent) {
    GameplayState& state = gGameState.gameplay;
    if (state.modalAlert.active) return;

    state.modalAlert = {};
    state.modalAlert.active = true;
    state.modalAlert.previousPaused = state.paused;
    state.modalAlert.closing = false;
    state.modalAlert.animation = 0.0f;
    state.modalAlert.accent = accent;
    std::strncpy(state.modalAlert.title, title, sizeof(state.modalAlert.title) - 1);
    std::strncpy(state.modalAlert.body, body, sizeof(state.modalAlert.body) - 1);
    state.paused = true;
}

static void CloseModalAlert() {
    GameplayState& state = gGameState.gameplay;
    if (!state.modalAlert.active) return;
    state.modalAlert.closing = true;
}

static void UpdateModalAlert(float delta) {
    GameplayState& state = gGameState.gameplay;
    if (!state.modalAlert.active) return;

    float direction = state.modalAlert.closing ? -1.0f : 1.0f;
    state.modalAlert.animation += delta * 6.5f * direction;
    state.modalAlert.animation = std::clamp(state.modalAlert.animation, 0.0f, 1.0f);

    if (state.modalAlert.closing && state.modalAlert.animation <= 0.0f) {
        bool previousPaused = state.modalAlert.previousPaused;
        state.modalAlert = {};
        state.paused = previousPaused;
    }
}

static float GetAverageCureProgress(const std::vector<Region>& regions) {
    if (regions.empty()) return 0.0f;
    float total = 0.0f;
    for (const Region& region : regions) total += region.cureProgress;
    return total / (float)regions.size();
}

static void CheckMilestoneAlerts() {
    GameplayState& state = gGameState.gameplay;
    if (state.modalAlert.active || state.regions.empty()) return;

    long long infected = GetTotalInfected(state.regions);
    long long dead = GetTotalDead(state.regions);
    float cure = GetAverageCureProgress(state.regions);

    enum AlertBits {
        ALERT_ONE_MILLION = 1 << 0,
        ALERT_FIFTY_MILLION = 1 << 1,
        ALERT_COMMON_COLD = 1 << 2,
        ALERT_DEATHS = 1 << 3,
        ALERT_CURE = 1 << 4
    };

    if (!(state.milestoneAlerts & ALERT_ONE_MILLION) && infected >= 1000000LL) {
        state.milestoneAlerts |= ALERT_ONE_MILLION;
        PushModalAlert(u8"ПЕРВЫЙ МИЛЛИОН ЗАРАЖЁННЫХ",
                       u8"Вирус вышел за локальные рамки. ВОЗ требует срочного усиления мониторинга.",
                       {255, 196, 82, 255});
    } else if (!(state.milestoneAlerts & ALERT_FIFTY_MILLION) && infected >= 50000000LL) {
        state.milestoneAlerts |= ALERT_FIFTY_MILLION;
        PushModalAlert(u8"ИНФЕКЦИЯ СТАЛА ГЛОБАЛЬНОЙ",
                       u8"Количество заражённых превысило население крупной страны. Транспортные узлы становятся критической угрозой.",
                       {255, 128, 78, 255});
    } else if (!(state.milestoneAlerts & ALERT_COMMON_COLD) && infected >= 200000000LL) {
        state.milestoneAlerts |= ALERT_COMMON_COLD;
        PushModalAlert(u8"ВИРУС ОБОГНАЛ ОБЫЧНУЮ ПРОСТУДУ",
                       u8"Мировые СМИ сравнивают масштаб заражения с самыми распространёнными сезонными инфекциями.",
                       {245, 86, 86, 255});
    } else if (!(state.milestoneAlerts & ALERT_DEATHS) && dead >= 1000000LL) {
        state.milestoneAlerts |= ALERT_DEATHS;
        PushModalAlert(u8"СМЕРТНОСТЬ РАСТЁТ",
                       u8"Больницы перегружены. Без ограничений портов и аэропортов ситуация продолжит ухудшаться.",
                       {238, 86, 92, 255});
    } else if (!(state.milestoneAlerts & ALERT_CURE) && cure >= 0.70f) {
        state.milestoneAlerts |= ALERT_CURE;
        PushModalAlert(u8"ПРОРЫВ В ЛЕЧЕНИИ",
                       u8"Лаборатории сообщают о стабильных результатах. Завершение глобальной программы лечения близко.",
                       {84, 214, 145, 255});
    }
}

static const char* LabStageName(LabStage stage) {
    static const char* names[LAB_STAGE_COUNT] = {
        u8"Образец",
        u8"Исследования",
        u8"Испытания",
        u8"Производство",
        u8"Доставка",
        u8"Готово"
    };
    int index = std::clamp((int)stage, 0, LAB_STAGE_COUNT - 1);
    return names[index];
}

static const char* LabStageVerb(LabStage stage) {
    static const char* verbs[LAB_STAGE_COUNT - 1] = {
        u8"Изолировать штамм",
        u8"Ускорить исследования",
        u8"Провести испытания",
        u8"Развернуть производство",
        u8"Запустить доставку"
    };
    int index = std::clamp((int)stage, 0, LAB_STAGE_COUNT - 2);
    return verbs[index];
}

static float GetLabStageBaseMultiplier(LabStage stage) {
    static const float values[LAB_STAGE_COUNT] = {0.38f, 0.58f, 0.82f, 1.18f, 1.55f, 2.25f};
    int index = std::clamp((int)stage, 0, LAB_STAGE_COUNT - 1);
    return values[index];
}

struct LabUpgradeDefinition {
    const char* name;
    const char* description;
    const char* effect;
    int cost;
    int requiredTrust;
    LabUpgradeBranch branch;
};

static const LabUpgradeDefinition LAB_UPGRADES[LAB_UPGRADE_COUNT] = {
    {u8"Частная клиника", u8"Собственный приемный блок дает доступ к обезличенным карточкам пациентов и первичной диагностике.",
     u8"+18% исследования, открывает карточки пациентов", 8, 0, LAB_BRANCH_RESEARCH},
    {u8"Геномный секвенатор", u8"Лабораторный комплекс восстанавливает фрагменты цепочки и выявляет РНК или ДНК.",
     u8"+24% исследования, открывает анализ патогена", 16, 0, LAB_BRANCH_RESEARCH},
    {u8"Электронный микроскоп", u8"Высокоточное оборудование позволяет увидеть форму вириона и классифицировать его морфологию.",
     u8"+32% исследования, открывает микроскопию", 30, 42, LAB_BRANCH_RESEARCH},

    {u8"Страница донатов", u8"Открытый сбор помогает лаборатории пережить первые недели без спонсоров.",
     u8"+2 к ежедневному доходу", 6, 0, LAB_BRANCH_FUNDING},
    {u8"Регулярные доноры", u8"Сторонники оформляют ежемесячную поддержку независимых исследований.",
     u8"+4 к ежедневному доходу", 14, 16, LAB_BRANCH_FUNDING},
    {u8"Фонд спасения", u8"Крупные благотворители объединяют пожертвования в международный фонд.",
     u8"+7 к ежедневному доходу", 28, 38, LAB_BRANCH_FUNDING},

    {u8"Серологический центр", u8"Открытая клиническая программа исследует реакцию антител и публикует результаты.",
     u8"+12 доверия, открывает серологический тест", 7, 0, LAB_BRANCH_PUBLIC},
    {u8"Научная горячая линия", u8"Врачи и волонтёры отвечают на вопросы и опровергают опасные слухи.",
     u8"+16 доверия, +1 доход", 15, 12, LAB_BRANCH_PUBLIC},
    {u8"Глобальная кампания", u8"Независимые учёные становятся публичным голосом борьбы с эпидемией.",
     u8"+22 доверия, +10% лечение", 26, 34, LAB_BRANCH_PUBLIC},

    {u8"Криолаборатория", u8"Университеты предоставляют термокамеры для испытаний устойчивости вирусных культур.",
     u8"+12% исследования, +1 доход, открывает термотест", 10, 18, LAB_BRANCH_COOPERATION},
    {u8"Национальные гранты", u8"Первые страны признают эффективность лаборатории и направляют гранты.",
     u8"+5 дохода, +12% лечение", 22, 40, LAB_BRANCH_COOPERATION},
    {u8"Международный альянс", u8"Страны координируют финансирование, производство и доставку лечения.",
     u8"+10 дохода, +28% лечение", 38, 68, LAB_BRANCH_COOPERATION}
};

static int GetUpgradeIndex(LabUpgradeBranch branch, int tier) {
    return (int)branch * LAB_UPGRADES_PER_BRANCH + tier;
}

static bool ArePatientCardsUnlocked(const GameplayState& state) {
    return state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_RESEARCH, 0)];
}

static bool IsPathogenSequencerUnlocked(const GameplayState& state) {
    return state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_RESEARCH, 1)];
}

static bool IsElectronMicroscopeUnlocked(const GameplayState& state) {
    return state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_RESEARCH, 2)];
}

static bool IsSerologyAssayUnlocked(const GameplayState& state) {
    return state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_PUBLIC, 0)];
}

static bool IsThermalChamberUnlocked(const GameplayState& state) {
    return state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_COOPERATION, 0)];
}

static int GetBranchLevel(const LabState& lab, LabUpgradeBranch branch) {
    int level = 0;
    for (int tier = 0; tier < LAB_UPGRADES_PER_BRANCH; ++tier) {
        if (lab.upgrades[GetUpgradeIndex(branch, tier)]) ++level;
    }
    return level;
}

static bool IsUpgradeAvailable(const LabState& lab, int upgradeIndex) {
    if (upgradeIndex < 0 || upgradeIndex >= LAB_UPGRADE_COUNT) return false;
    const LabUpgradeDefinition& upgrade = LAB_UPGRADES[upgradeIndex];
    int tier = upgradeIndex % LAB_UPGRADES_PER_BRANCH;
    if (lab.upgrades[upgradeIndex] || lab.resources < upgrade.cost || lab.trust < upgrade.requiredTrust) return false;
    return tier == 0 || lab.upgrades[upgradeIndex - 1];
}

static void BuyLabUpgrade(GameplayState& state, int upgradeIndex) {
    if (!IsUpgradeAvailable(state.lab, upgradeIndex)) return;
    const LabUpgradeDefinition& upgrade = LAB_UPGRADES[upgradeIndex];
    state.lab.resources -= upgrade.cost;
    state.lab.upgrades[upgradeIndex] = true;

    int tier = upgradeIndex % LAB_UPGRADES_PER_BRANCH;
    if (upgrade.branch == LAB_BRANCH_PUBLIC) {
        static const int trustGain[LAB_UPGRADES_PER_BRANCH] = {12, 16, 22};
        state.lab.trust = std::min(100, state.lab.trust + trustGain[tier]);
    } else {
        state.lab.trust = std::min(100, state.lab.trust + 2);
    }

    char message[192];
    std::snprintf(message, sizeof(message), u8"Лаборатория улучшена: %s", upgrade.name);
    PushNotification(message, {84, 214, 145, 255}, 5.0f);
}

static float GetLabCureMultiplier(const GameplayState& state) {
    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    float stageBoost = GetLabStageBaseMultiplier(state.lab.stage);
    float trustBoost = 0.70f + 0.30f * difficulty.scienceTrust;
    float cooperationBoost = 0.72f + 0.28f * difficulty.internationalCooperation;
    float complianceBoost = 0.80f + 0.20f * difficulty.publicCompliance;
    float progressBoost = 0.85f + 0.45f * state.lab.stageProgress;
    float researchBoost = 1.0f + 0.18f * GetBranchLevel(state.lab, LAB_BRANCH_RESEARCH);
    if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_PUBLIC, 2)]) researchBoost += 0.10f;
    if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_COOPERATION, 0)]) researchBoost += 0.12f;
    if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_COOPERATION, 1)]) researchBoost += 0.12f;
    if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_COOPERATION, 2)]) researchBoost += 0.28f;
    return difficulty.cureMultiplier * stageBoost * trustBoost * cooperationBoost * complianceBoost * progressBoost * researchBoost;
}

static int GetLabDailyIncome(const GameplayState& state) {
    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    long long infected = GetTotalInfected(state.regions);
    long long population = GetTotalPopulationPeople(state.regions);
    float infectionPressure = population > 0 ? (float)infected / (float)population : 0.0f;
    float cooperation = 0.85f + 0.35f * difficulty.internationalCooperation;
    float science = 0.80f + 0.40f * difficulty.scienceTrust;
    int baseIncome = 1 + (int)std::round(0.90f * cooperation + 0.65f * science + 0.80f * infectionPressure);
    static const int fundingIncome[LAB_UPGRADES_PER_BRANCH] = {1, 3, 5};
    static const int cooperationIncome[LAB_UPGRADES_PER_BRANCH] = {1, 3, 6};
    for (int tier = 0; tier < LAB_UPGRADES_PER_BRANCH; ++tier) {
        if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_FUNDING, tier)]) baseIncome += fundingIncome[tier];
        if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_COOPERATION, tier)]) baseIncome += cooperationIncome[tier];
    }
    if (state.lab.upgrades[GetUpgradeIndex(LAB_BRANCH_PUBLIC, 1)]) baseIncome += 1;
    baseIncome += state.lab.trust / 40;
    return std::max(1, baseIncome);
}

static void SyncLabStageProgress(GameplayState& state) {
    if (state.lab.stage == LAB_STAGE_COMPLETE) {
        state.lab.overallProgress = 1.0f;
        state.lab.stageProgress = 1.0f;
        return;
    }

    state.lab.overallProgress = ((float)state.lab.stage + state.lab.stageProgress) / (float)LAB_STAGE_COMPLETE;
    state.lab.overallProgress = std::clamp(state.lab.overallProgress, 0.0f, 1.0f);
}

static void AdvanceLabStage(GameplayState& state) {
    if (state.lab.stage >= LAB_STAGE_COMPLETE) {
        state.lab.stage = LAB_STAGE_COMPLETE;
        state.lab.stageProgress = 1.0f;
        state.lab.overallProgress = 1.0f;
        return;
    }

    state.lab.stage = (LabStage)((int)state.lab.stage + 1);
    state.lab.stageProgress = 0.0f;
    SyncLabStageProgress(state);

    char message[160];
    std::snprintf(message, sizeof(message), u8"Лаборатория: этап \"%s\" завершён", LabStageName((LabStage)((int)state.lab.stage - 1)));
    PushNotification(message, {82, 205, 137, 255}, 5.0f);
}

static void ApplyLabBoost(GameplayState& state) {
    if (state.labPlacementActive || state.labRegionIndex < 0) return;
    if (state.lab.stage >= LAB_STAGE_COMPLETE) return;

    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    int cost = std::max(1, (int)std::round(4.0f * difficulty.actionCostMultiplier));
    if (state.lab.resources < cost) {
        PushNotification(u8"Недостаточно ресурсов для ускорения", {244, 164, 73, 255}, 3.5f);
        return;
    }

    state.lab.resources -= cost;
    float bonus = 0.12f + 0.03f * (float)state.lab.stage;
    state.lab.stageProgress = std::min(1.0f, state.lab.stageProgress + bonus);
    SyncLabStageProgress(state);
    if (state.lab.stageProgress >= 0.999f) AdvanceLabStage(state);
}

static Rectangle GetLabPanelRect() {
    float sh = (float)GetScreenHeight();
    return {16.0f, sh - 176.0f, 286.0f, 156.0f};
}

static Rectangle GetLabButtonRect(const Rectangle& panel) {
    float gap = 8.0f;
    float width = (panel.width - 36.0f - gap) * 0.5f;
    return {panel.x + 18.0f, panel.y + panel.height - 46.0f, width, 30.0f};
}

static Rectangle GetJournalButtonRect(const Rectangle& panel) {
    Rectangle labButton = GetLabButtonRect(panel);
    return {labButton.x + labButton.width + 8.0f, labButton.y, labButton.width, labButton.height};
}

static Rectangle GetLabMenuRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    return {sw * 0.5f - 560.0f, sh * 0.5f - 340.0f, 1120.0f, 680.0f};
}

static Rectangle GetLabHeaderRect(const Rectangle& menu) {
    return {menu.x + 28.0f, menu.y + 24.0f, menu.width - 56.0f, 70.0f};
}

static Rectangle GetLabStatusRect(const Rectangle& menu) {
    return {menu.x + 28.0f, menu.y + 112.0f, 248.0f, 520.0f};
}

static Rectangle GetLabTechRect(const Rectangle& menu) {
    return {menu.x + 296.0f, menu.y + 112.0f, 448.0f, 520.0f};
}

static Rectangle GetLabInfoRect(const Rectangle& menu) {
    return {menu.x + 764.0f, menu.y + 112.0f, 328.0f, 520.0f};
}

static Rectangle GetLabMenuCloseRect(const Rectangle& menu) {
    return {menu.x + menu.width - 60.0f, menu.y + 24.0f, 36.0f, 36.0f};
}

static Rectangle GetLabMenuBuyRect(const Rectangle& menu) {
    Rectangle info = GetLabInfoRect(menu);
    return {info.x + 18.0f, info.y + info.height - 78.0f, info.width - 36.0f, 48.0f};
}

static Rectangle GetLabBranchTabRect(const Rectangle& menu, int branch) {
    Rectangle tech = GetLabTechRect(menu);
    branch = std::clamp(branch, 0, LAB_BRANCH_COUNT - 1);
    float gap = 8.0f;
    float width = (tech.width - gap * (LAB_BRANCH_COUNT - 1)) / LAB_BRANCH_COUNT;
    return {tech.x + branch * (width + gap), menu.y + 46.0f, width, 34.0f};
}

static Rectangle GetLabUpgradeListItemRect(const Rectangle& menu, int tier) {
    Rectangle tech = GetLabTechRect(menu);
    tier = std::clamp(tier, 0, LAB_UPGRADES_PER_BRANCH - 1);
    return {tech.x + 22.0f, tech.y + 74.0f + tier * 122.0f, tech.width - 44.0f, 96.0f};
}

static Vector2 GetLabUpgradeNodeCenter(const Rectangle& menu, LabUpgradeBranch branch, int tier) {
    static const Vector2 offsets[LAB_BRANCH_COUNT][LAB_UPGRADES_PER_BRANCH] = {
        {{124.0f, 146.0f}, {124.0f, 302.0f}, {124.0f, 458.0f}},
        {{330.0f, 146.0f}, {330.0f, 302.0f}, {330.0f, 458.0f}},
        {{536.0f, 146.0f}, {536.0f, 302.0f}, {536.0f, 458.0f}},
        {{742.0f, 146.0f}, {742.0f, 302.0f}, {742.0f, 458.0f}}
    };
    Rectangle tech = GetLabTechRect(menu);
    branch = (LabUpgradeBranch)std::clamp((int)branch, 0, LAB_BRANCH_COUNT - 1);
    tier = std::clamp(tier, 0, LAB_UPGRADES_PER_BRANCH - 1);
    return {tech.x + offsets[(int)branch][tier].x, tech.y + offsets[(int)branch][tier].y};
}

static void CloseLabMenu(GameplayState& state) {
    state.lab.menuOpen = false;
    state.paused = state.lab.previousPaused;
}

static void UpdateLabMenu(GameplayState& state) {
    Vector2 mouse = GetMousePosition();
    Rectangle menu = GetLabMenuRect();
    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseLabMenu(state);
        return;
    }
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;
    if (CheckCollisionPointRec(mouse, GetLabMenuCloseRect(menu))) {
        CloseLabMenu(state);
        return;
    }
    for (int branch = 0; branch < LAB_BRANCH_COUNT; ++branch) {
        if (CheckCollisionPointRec(mouse, GetLabBranchTabRect(menu, branch))) {
            state.lab.activeBranch = (LabUpgradeBranch)branch;
            state.lab.selectedUpgrade = GetUpgradeIndex(state.lab.activeBranch, 0);
            return;
        }
    }
    for (int tier = 0; tier < LAB_UPGRADES_PER_BRANCH; ++tier) {
        if (CheckCollisionPointRec(mouse, GetLabUpgradeListItemRect(menu, tier))) {
            state.lab.selectedUpgrade = GetUpgradeIndex(state.lab.activeBranch, tier);
            return;
        }
    }
    if (CheckCollisionPointRec(mouse, GetLabMenuBuyRect(menu))) {
        BuyLabUpgrade(state, state.lab.selectedUpgrade);
    }
}

static std::mt19937& GetGameplayRng() {
    static std::mt19937 rng = [] {
        std::random_device rd;
        auto seed = (unsigned int)rd() ^
                    (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::mt19937(seed);
    }();
    return rng;
}

static bool RollChance(float chance) {
    if (chance <= 0.0f) return false;
    if (chance >= 1.0f) return true;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(GetGameplayRng()) < chance;
}

static void TryTriggerVirusMutation(GameplayState& state) {
    if (state.virusMutationTriggered || state.difficulty != DIFFICULTY_COLLAPSE) return;

    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    long long infected = GetTotalInfected(state.regions);
    long long population = GetTotalPopulationPeople(state.regions);
    if (population <= 0 || infected <= 0) return;

    float infectedShare = (float)infected / (float)population;
    if (infectedShare < difficulty.mutationMinimumInfectedShare) return;

    float normalized = (infectedShare - difficulty.mutationMinimumInfectedShare) /
                       std::max(0.001f, 1.0f - difficulty.mutationMinimumInfectedShare);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    float chance = difficulty.mutationChancePerDay * (0.35f + 0.65f * normalized);
    if (!RollChance(chance)) return;

    state.virusMutationTriggered = true;
    PushNotification(u8"МУТАЦИЯ ВИРУСА: смертность резко выросла", {255, 92, 92, 255}, 7.0f);
}

static void SpawnResourceMarker(GameplayState& state, int regionIndex) {
    if (state.resourceMarker.active) return;
    if (regionIndex < 0 || regionIndex >= (int)state.regions.size()) return;

    std::uniform_int_distribution<int> reward(3, 6);
    state.resourceMarker.active = true;
    state.resourceMarker.regionIndex = regionIndex;
    state.resourceMarker.reward = reward(GetGameplayRng());
    state.resourceMarker.age = 0.0f;
    state.resourceMarker.lifetime = 5.0f;
}

static void DetectResourceMarkerEvents(GameplayState& state) {
    int count = (int)state.regions.size();
    if ((int)state.observedInfectionRates.size() != count) {
        state.observedInfectionRates.assign(count, 0.0f);
    }
    if ((int)state.infectionRewardMilestones.size() != count) {
        state.infectionRewardMilestones.assign(count, 0);
    }

    std::vector<int> candidates;
    for (int i = 0; i < count; ++i) {
        float previous = state.observedInfectionRates[i];
        float current = state.regions[i].infectionRate;
        unsigned char& milestones = state.infectionRewardMilestones[i];
        bool triggered = false;

        if (!(milestones & 1) && previous <= 0.0001f && current > 0.0001f) {
            milestones |= 1;
            triggered = true;
        }
        if (!(milestones & 2) && previous < 0.30f && current >= 0.30f) {
            milestones |= 2;
            triggered = true;
        }
        if (!(milestones & 4) && previous < 0.65f && current >= 0.65f) {
            milestones |= 4;
            triggered = true;
        }

        if (triggered) candidates.push_back(i);
        state.observedInfectionRates[i] = current;
    }

    if (!state.resourceMarker.active && !candidates.empty()) {
        std::uniform_int_distribution<int> pick(0, (int)candidates.size() - 1);
        SpawnResourceMarker(state, candidates[pick(GetGameplayRng())]);
    }
}

static Vector2 GetResourceMarkerPosition(const GameplayState& state) {
    if (!state.resourceMarker.active ||
        state.resourceMarker.regionIndex < 0 ||
        state.resourceMarker.regionIndex >= (int)state.regions.size()) {
        return {-1000.0f, -1000.0f};
    }

    Rectangle mapRect = GetMapRect(state);
    Vector2 base = MapPointToScreen(state.regions[state.resourceMarker.regionIndex].mapPosition, mapRect);
    float rise = std::clamp(state.resourceMarker.age / 0.45f, 0.0f, 1.0f);
    rise = EaseOutCubic(rise);
    base.y += 38.0f * (1.0f - rise);
    return base;
}

static bool UpdateResourceMarker(GameplayState& state, float delta, Vector2 mouse) {
    if (!state.resourceMarker.active) return false;

    if (!state.paused) state.resourceMarker.age += delta;
    if (state.resourceMarker.age >= state.resourceMarker.lifetime) {
        state.resourceMarker = {};
        return false;
    }

    Vector2 position = GetResourceMarkerPosition(state);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointCircle(mouse, position, 30.0f)) {
        int reward = state.resourceMarker.reward;
        state.lab.resources += reward;

        char message[160];
        std::snprintf(message, sizeof(message), u8"Экстренное финансирование: +$%d", reward);
        PushNotification(message, {242, 210, 88, 255}, 4.5f);
        state.resourceMarker = {};
        return true;
    }
    return false;
}

static Rectangle GetLabPlacementCardRect() {
    float sw = (float)GetScreenWidth();
    return {sw * 0.5f - 390.0f, 82.0f, 780.0f, 154.0f};
}

static void PlaceLaboratoryInRegion(int regionIndex) {
    GameplayState& state = gGameState.gameplay;
    if (regionIndex < 0 || regionIndex >= (int)state.regions.size()) return;

    for (Region& region : state.regions) region.isSelected = false;
    state.labRegionIndex = regionIndex;
    state.labPlacementActive = false;
    state.paused = false;
    state.selectedRegion = regionIndex;
    state.regions[regionIndex].isSelected = true;

    char message[192];
    std::snprintf(message, sizeof(message), u8"Лаборатория развернута: %s в %s", GetLabDisplayName(), state.regions[regionIndex].name);
    PushNotification(message, {84, 214, 145, 255}, 6.0f);
    state.lab.previousPaused = state.paused;
    state.lab.menuOpen = true;
    state.paused = true;
}

// ═══════════════════════════════════════════════════════════════
// SIMULATION
// ═══════════════════════════════════════════════════════════════

static void AdvanceDate() {
    GameplayState& state = gGameState.gameplay;
    state.day++;
    if (state.day > 30) {
        state.day = 1;
        state.month++;
    }
    if (state.month > 12) {
        state.month = 1;
        state.year++;
    }
}

static void SimulateDay() {
    GameplayState& state = gGameState.gameplay;
    state.lab.resources += GetLabDailyIncome(state);
    if (state.lab.stage < LAB_STAGE_COMPLETE) {
        const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
        float infectedShare = std::min(
            1.0f,
            (float)GetTotalInfected(state.regions) /
                std::max(1.0f, (float)GetTotalPopulationPeople(state.regions)));
        float passiveGain = 0.0025f * difficulty.scienceTrust * difficulty.internationalCooperation;
        passiveGain += 0.0006f * state.lab.stageProgress;
        passiveGain += 0.0004f * infectedShare;
        if (state.diagnosisConfirmed) passiveGain *= 4.0f;

        float progressLimit = state.diagnosisConfirmed ? 1.0f : 0.95f;
        state.lab.stageProgress = std::min(progressLimit, state.lab.stageProgress + passiveGain);
        SyncLabStageProgress(state);
        if (state.diagnosisConfirmed && state.lab.stageProgress >= 0.999f) AdvanceLabStage(state);
    }

    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    TryTriggerVirusMutation(state);
    float spreadMultiplier = difficulty.spreadMultiplier;
    float mortalityMultiplier = difficulty.mortalityMultiplier;
    if (state.virusMutationTriggered) {
        spreadMultiplier *= difficulty.mutationSpreadBoost;
        mortalityMultiplier *= difficulty.mutationMortalityBoost;
    }
    bool vaccineReady = state.lab.stage >= LAB_STAGE_COMPLETE || state.lab.overallProgress >= 1.0f;
    for (Region& region : state.regions) {
        SimulateRegionDay(region, spreadMultiplier, GetLabCureMultiplier(state), mortalityMultiplier, vaccineReady);
    }
    CheckMilestoneAlerts();
    if (vaccineReady && GetTotalInfected(state.regions) <= 0) state.gameWon = true;
    if (GetTotalDead(state.regions) > 500000000LL) state.gameLost = true;
}

// ═══════════════════════════════════════════════════════════════
// LOAD / UNLOAD
// ═══════════════════════════════════════════════════════════════

static void LoadTextureIfExists(Texture2D& texture, bool& loaded, const char* fileName) {
    std::string path = ResourcePath(fileName);
    if (!FileExists(path.c_str())) {
        loaded = false;
        return;
    }
    texture = LoadTexture(path.c_str());
    loaded = texture.id != 0;
    if (loaded) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
}

// ═══════════════════════════════════════════════════════════════
// MASK: load & precompute border pixels
// ═══════════════════════════════════════════════════════════════

static void BuildBorderPixels(GameplayState& state) {
    if (!state.maskImageLoaded) return;
    int W = state.maskImage.width;
    int H = state.maskImage.height;
    int nRegions = (int)state.regions.size();
    state.regionBorderPixels.assign(nRegions, {});

    // maskImage is RGB (r = region_index+1, 0 = ocean)
    auto getR = [&](int x, int y) -> int {
        if (x < 0 || x >= W || y < 0 || y >= H) return 0;
        Color c = GetImageColor(state.maskImage, x, y);
        return (int)c.r;
    };

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int r = getR(x, y);
            if (r == 0) continue;
            int idx = r - 1;
            if (idx >= nRegions) continue;
            // border if any 4-neighbor differs
            if (getR(x-1,y) != r || getR(x+1,y) != r ||
                getR(x,y-1) != r || getR(x,y+1) != r) {
                state.regionBorderPixels[idx].push_back(y * W + x);
            }
        }
    }

    // Create RGBA border overlay image (all transparent)
    state.borderImage = GenImageColor(W, H, BLANK);
    state.borderTexture = LoadTextureFromImage(state.borderImage);
    state.borderTexLoaded = true;
    state.borderLastHovered  = -2;
    state.borderLastSelected = -2;
}

static void UpdateBorderOverlay(GameplayState& state) {
    if (!state.borderTexLoaded || !state.maskImageLoaded) return;
    int h = state.hoveredRegion;
    int s = state.selectedRegion;
    if (h == state.borderLastHovered && s == state.borderLastSelected) return;

    int W = state.borderImage.width;
    unsigned char* px = (unsigned char*)state.borderImage.data;

    auto paintPixels = [&](int regionIdx, unsigned char r, unsigned char g,
                            unsigned char b, unsigned char a) {
        if (regionIdx < 0 || regionIdx >= (int)state.regionBorderPixels.size()) return;
        for (int off : state.regionBorderPixels[regionIdx]) {
            int base = off * 4;
            px[base]   = r;
            px[base+1] = g;
            px[base+2] = b;
            px[base+3] = a;
        }
    };

    // Clear previously painted regions
    paintPixels(state.borderLastHovered,  0, 0, 0, 0);
    paintPixels(state.borderLastSelected, 0, 0, 0, 0);

    // Paint selected first (hover draws on top)
    paintPixels(s, 255, 200,  60, 220); // gold border for selected
    paintPixels(h, 120, 210, 255, 190); // blue-white for hover

    UpdateTexture(state.borderTexture, state.borderImage.data);
    state.borderLastHovered  = h;
    state.borderLastSelected = s;
}

static Rectangle GetMapRect(const GameplayState& state) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    if (state.mapLoaded && state.mapTexture.id != 0) {
        float tw = (float)state.mapTexture.width;
        float th = (float)state.mapTexture.height;
        // Use max to fill screen (crop excess), avoids letterbox bands
        float scale = std::max(sw / tw, sh / th);
        float dw = tw * scale;
        float dh = th * scale;
        return { (sw - dw) * 0.5f, (sh - dh) * 0.5f, dw, dh };
    }
    return {0, 0, sw, sh};
}

static Vector2 ScreenToMapPoint(Vector2 mouse, Rectangle mapRect) {
    if (mapRect.width <= 0.0f || mapRect.height <= 0.0f) return {0.0f, 0.0f};
    return {
        std::clamp((mouse.x - mapRect.x) / mapRect.width, 0.0f, 1.0f),
        std::clamp((mouse.y - mapRect.y) / mapRect.height, 0.0f, 1.0f)
    };
}

static Vector2 MapPointToScreen(Vector2 mapPoint, Rectangle mapRect) {
    return {
        mapRect.x + mapPoint.x * mapRect.width,
        mapRect.y + mapPoint.y * mapRect.height
    };
}

static std::string MakeTransportHubName(const Region& region, TransportHubType type) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%s %s %d", region.name, TransportHubTypeToString(type), CountTransportHubs(region, type) + 1);
    return buffer;
}

static std::string TransportHubsDataPath() {
    if (FileExists("../res/regions.json")) return "../res/transport_hubs.json";
    if (FileExists("res/regions.json")) return "res/transport_hubs.json";
    return ResourcePath("transport_hubs.json");
}

static void SaveTransportHubState() {
    std::string hubsPath = TransportHubsDataPath();
    if (!SaveTransportHubsToJSON(gGameState.gameplay.regions, hubsPath.c_str())) {
        TraceLog(LOG_WARNING, "TRANSPORT: failed to save hubs to %s", hubsPath.c_str());
    } else {
        TraceLog(LOG_INFO, "TRANSPORT: saved hubs to %s", hubsPath.c_str());
    }
}

static void RebuildVehiclePool() {
    InitializeVehiclePool(gGameState.gameplay.vehicles, gGameState.gameplay.regions, GetSeaObstacles(gGameState.gameplay));
}

static void AddTransportHubToSelectedRegion(TransportHubType type, Vector2 mouse) {
    GameplayState& state = gGameState.gameplay;
    int regionIndex = state.selectedRegion;
    if (regionIndex < 0 || regionIndex >= (int)state.regions.size()) {
        regionIndex = state.hoveredRegion;
    }
    if (regionIndex < 0 || regionIndex >= (int)state.regions.size()) return;

    Rectangle mapRect = GetMapRect(state);
    Vector2 point = ScreenToMapPoint(mouse, mapRect);
    Region& region = state.regions[regionIndex];
    AddTransportHub(region, type, point, MakeTransportHubName(region, type).c_str(), false);
    state.selectedRegion = regionIndex;
    state.debugSelectedHub = (int)region.transportHubs.size() - 1;
    SaveTransportHubState();
    RebuildVehiclePool();
}

static bool SelectTransportHubAtMouse(Vector2 mouse) {
    GameplayState& state = gGameState.gameplay;
    if (state.selectedRegion < 0 || state.selectedRegion >= (int)state.regions.size()) return false;

    Rectangle mapRect = GetMapRect(state);
    Region& region = state.regions[state.selectedRegion];
    int bestIndex = -1;
    float bestDistance = 18.0f * 18.0f;

    for (int i = 0; i < (int)region.transportHubs.size(); ++i) {
        Vector2 hubScreen = MapPointToScreen(region.transportHubs[i].mapPosition, mapRect);
        float dx = mouse.x - hubScreen.x;
        float dy = mouse.y - hubScreen.y;
        float distance = dx * dx + dy * dy;
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    state.debugSelectedHub = bestIndex;
    return bestIndex >= 0;
}

static void ToggleSelectedTransportHub() {
    GameplayState& state = gGameState.gameplay;
    if (state.selectedRegion < 0 || state.selectedRegion >= (int)state.regions.size()) return;
    Region& region = state.regions[state.selectedRegion];
    if (state.debugSelectedHub < 0 || state.debugSelectedHub >= (int)region.transportHubs.size()) return;
    region.transportHubs[state.debugSelectedHub].closed = !region.transportHubs[state.debugSelectedHub].closed;
    SaveTransportHubState();
    RebuildVehiclePool();
}

static void DeleteSelectedTransportHub() {
    GameplayState& state = gGameState.gameplay;
    if (state.selectedRegion < 0 || state.selectedRegion >= (int)state.regions.size()) return;
    Region& region = state.regions[state.selectedRegion];
    if (state.debugSelectedHub < 0 || state.debugSelectedHub >= (int)region.transportHubs.size()) return;
    region.transportHubs.erase(region.transportHubs.begin() + state.debugSelectedHub);
    if (region.transportHubs.empty()) state.debugSelectedHub = -1;
    else if (state.debugSelectedHub >= (int)region.transportHubs.size()) state.debugSelectedHub = (int)region.transportHubs.size() - 1;
    SaveTransportHubState();
    RebuildVehiclePool();
}

static Rectangle GetTransportDebugPanelRect() {
    float sw = (float)GetScreenWidth();
    return {sw - 332.0f, 66.0f, 320.0f, 246.0f};
}

static Rectangle GetTransportDebugButtonRect(const Rectangle& panel, int index) {
    float pad = 12.0f;
    float gap = 8.0f;
    float bw = (panel.width - pad * 2.0f - gap) * 0.5f;
    float bh = 28.0f;
    int row = index / 2;
    int col = index % 2;
    return {
        panel.x + pad + col * (bw + gap),
        panel.y + 110.0f + row * (bh + gap),
        bw,
        bh
    };
}

static Rectangle GetEditorModeButtonRect(const Rectangle& panel, int index) {
    float pad = 12.0f;
    float gap = 8.0f;
    float bw = (panel.width - pad * 2.0f - gap) * 0.5f;
    float bh = 28.0f;
    return {
        panel.x + pad + index * (bw + gap),
        panel.y + 12.0f,
        bw,
        bh
    };
}

static int GetRegionAtMask(const GameplayState& state, Vector2 mouse, int sw, int sh) {
    Rectangle mapRect = GetMapRect(state);
    if (mouse.x < mapRect.x || mouse.x > mapRect.x + mapRect.width ||
        mouse.y < mapRect.y || mouse.y > mapRect.y + mapRect.height) return -1;

    float u = (mouse.x - mapRect.x) / mapRect.width;
    float v = (mouse.y - mapRect.y) / mapRect.height;

    // Prefer user polygons if available
    for (int i = (int)state.regionPolygons.size() - 1; i >= 0; --i) {
        if (!state.regionPolygons[i].empty() && PointInPolygon(state.regionPolygons[i], u, v)) {
            return i;
        }
    }

    if (!state.maskImageLoaded) {
        // Fallback: nearest center using mapRect
        int best = -1;
        float bestD = 999999.f;
        for (int i = 0; i < (int)state.regions.size(); ++i) {
            Vector2 c = {mapRect.x + state.regions[i].mapPosition.x * mapRect.width,
                         mapRect.y + state.regions[i].mapPosition.y * mapRect.height};
            float d = (mouse.x-c.x)*(mouse.x-c.x)+(mouse.y-c.y)*(mouse.y-c.y);
            if (d < bestD) { bestD = d; best = i; }
        }
        return (bestD < 40.f*40.f) ? best : -1;
    }

    int mx = (int)(u * (float)state.maskImage.width);
    int my = (int)(v * (float)state.maskImage.height);
    if (mx < 0 || mx >= state.maskImage.width || my < 0 || my >= state.maskImage.height)
        return -1;
    Color c = GetImageColor(state.maskImage, mx, my);
    if (c.r == 0) return -1;
    int idx = (int)c.r - 1;
    return (idx >= 0 && idx < (int)state.regions.size()) ? idx : -1;
}

static void DrawTransportHubMarker(const TransportHub& hub, Rectangle mapRect, bool selected) {
    Vector2 pos = MapPointToScreen(hub.mapPosition, mapRect);
    bool airport = hub.type == HUB_AIRPORT;
    Color base = hub.closed
        ? Color{120, 125, 135, 210}
        : (airport ? Color{70, 195, 255, 240} : Color{255, 176, 86, 240});
    Color outline = selected ? Color{255, 255, 255, 240} : Color{10, 16, 24, 220};

    if (airport) {
        if (gGameState.gameplay.airportIconLoaded && gGameState.gameplay.airportIcon.id != 0) {
            Rectangle src = {0, 0, (float)gGameState.gameplay.airportIcon.width, (float)gGameState.gameplay.airportIcon.height};
            Rectangle dst = {pos.x, pos.y, 22.0f, 22.0f};
            DrawTexturePro(gGameState.gameplay.airportIcon, src, dst, {11.0f, 11.0f}, 0.0f, base);
        } else {
            DrawPoly(pos, 3, 8.0f, -90.0f, base);
        }
    } else {
        if (gGameState.gameplay.portIconLoaded && gGameState.gameplay.portIcon.id != 0) {
            Rectangle src = {0, 0, (float)gGameState.gameplay.portIcon.width, (float)gGameState.gameplay.portIcon.height};
            Rectangle dst = {pos.x, pos.y, 22.0f, 22.0f};
            DrawTexturePro(gGameState.gameplay.portIcon, src, dst, {11.0f, 11.0f}, 0.0f, base);
        } else {
            DrawRectanglePro({pos.x, pos.y, 14.0f, 14.0f}, {7.0f, 7.0f}, 12.0f, base);
        }
    }

    if (hub.closed) {
        DrawLineEx({pos.x - 7.0f, pos.y - 7.0f}, {pos.x + 7.0f, pos.y + 7.0f}, 2.0f, {255, 70, 70, 240});
        DrawLineEx({pos.x - 7.0f, pos.y + 7.0f}, {pos.x + 7.0f, pos.y - 7.0f}, 2.0f, {255, 70, 70, 240});
    }

    DrawCircleLines((int)pos.x, (int)pos.y, selected ? 18.0f : 15.0f, outline);
}

static void DrawLaboratoryMarker(const Region& region, Rectangle mapRect) {
    Vector2 pos = MapPointToScreen(region.mapPosition, mapRect);
    Color glow = {70, 195, 255, 70};
    Color body = {235, 246, 255, 245};
    Color accent = {70, 195, 255, 255};
    Color shadow = {0, 0, 0, 120};

    DrawCircleV(pos, 24.0f, glow);
    DrawCircleV(pos, 17.0f, {8, 14, 22, 225});
    DrawCircleLines((int)pos.x, (int)pos.y, 18.0f, accent);

    Rectangle base = {pos.x - 9.0f, pos.y - 4.0f, 18.0f, 13.0f};
    DrawRectangleRounded({base.x + 2.0f, base.y + 3.0f, base.width, base.height}, 0.22f, 6, shadow);
    DrawRectangleRounded(base, 0.22f, 6, body);
    DrawRectangleRounded({pos.x - 5.0f, pos.y - 13.0f, 10.0f, 11.0f}, 0.18f, 5, body);
    DrawLineEx({pos.x - 4.0f, pos.y + 2.5f}, {pos.x + 4.0f, pos.y + 2.5f}, 2.0f, accent);
    DrawLineEx({pos.x, pos.y - 1.5f}, {pos.x, pos.y + 6.5f}, 2.0f, accent);
}

void LoadGameplayAssets() {
    GameplayState& state = gGameState.gameplay;
    LoadTextureIfExists(state.mapTexture, state.mapLoaded, "map.png");
    LoadTextureIfExists(state.planeIcon, state.planeIconLoaded, "icon_plane.png");
    LoadTextureIfExists(state.shipIcon, state.shipIconLoaded, "icon_ship.png");
    LoadTextureIfExists(state.airportIcon, state.airportIconLoaded, "icon_airport.png");
    LoadTextureIfExists(state.portIcon, state.portIconLoaded, "icon_port.png");

    // Load mask image for country selection
    std::string maskPath = ResourcePath("regions_mask.png");
    if (FileExists(maskPath.c_str())) {
        state.maskImage = LoadImage(maskPath.c_str());
        state.maskImageLoaded = (state.maskImage.data != nullptr);
        // Resize mask to map texture size so hit test aligns with stretched map
        if (state.maskImageLoaded && state.mapLoaded &&
            (state.maskImage.width  != state.mapTexture.width ||
             state.maskImage.height != state.mapTexture.height)) {
            ImageResize(&state.maskImage, state.mapTexture.width, state.mapTexture.height);
        }
    }

    ResetGameplaySession();
    BuildBorderPixels(state);
}

void ResetGameplaySession() {
    GameplayState& state = gGameState.gameplay;
    state.regions.clear();
    state.vehicles.clear();
    state.regionPolygons.clear();
    state.seaObstacles.clear();
    state.polygonsLoaded = false;

    InitializeRegions(state.regions);
    std::string hubsPath = TransportHubsDataPath();
    LoadTransportHubsFromJSON(state.regions, hubsPath.c_str());
    LoadUserPolygons(state);
    LoadSeaObstacles(state);
    InitializeVehiclePool(state.vehicles, state.regions, GetSeaObstacles(state));

    state.selectedRegion = -1;
    state.hoveredRegion = -1;
    state.day = 25;
    state.month = 5;
    state.year = 2026;
    state.dayAccumulator = 0.0f;
    state.newsOffset = (float)GetScreenWidth();
    state.newsIndex = 0;
    state.notificationTimer = 1.5f;
    state.notifications.clear();
    state.modalAlert = {};
    state.patientCardGame.SelectRandomVirus();
    state.patientCardPreviousPaused = false;
    state.journal.Reset();
    state.showJournal = false;
    state.journalPreviousPaused = false;
    state.diagnosisConfirmed = false;
    state.lab = {};
    state.lab.resources = GetDifficultyProfile(state.difficulty).startingResources;
    state.lab.boostCost = std::max(1, (int)std::round(4.0f * GetDifficultyProfile(state.difficulty).actionCostMultiplier));
    state.resourceMarker = {};
    state.observedInfectionRates.resize(state.regions.size());
    state.infectionRewardMilestones.assign(state.regions.size(), 0);
    for (int i = 0; i < (int)state.regions.size(); ++i) {
        float rate = state.regions[i].infectionRate;
        state.observedInfectionRates[i] = rate;
        if (rate > 0.0001f) state.infectionRewardMilestones[i] |= 1;
        if (rate >= 0.30f) state.infectionRewardMilestones[i] |= 2;
        if (rate >= 0.65f) state.infectionRewardMilestones[i] |= 4;
    }
    state.labRegionIndex = -1;
    state.labPlacementActive = true;
    state.milestoneAlerts = 0;
    state.speedMultiplier = GetDefaultGameplaySpeed();
    state.paused = true;
    state.pauseMenuOpen = false;
    state.gameWon = false;
    state.gameLost = false;
    state.virusMutationTriggered = false;
    state.debugEditorMode = 0;
    state.debugTransportMode = 0;
    state.debugSelectedHub = -1;
}

void UnloadGameplayAssets() {
    GameplayState& state = gGameState.gameplay;
    if (state.mapLoaded && state.mapTexture.id != 0) UnloadTexture(state.mapTexture);
    if (state.planeIconLoaded && state.planeIcon.id != 0) UnloadTexture(state.planeIcon);
    if (state.shipIconLoaded && state.shipIcon.id != 0) UnloadTexture(state.shipIcon);
    if (state.airportIconLoaded && state.airportIcon.id != 0) UnloadTexture(state.airportIcon);
    if (state.portIconLoaded && state.portIcon.id != 0) UnloadTexture(state.portIcon);
    if (state.maskImageLoaded) { UnloadImage(state.maskImage); state.maskImage = {0}; }
    if (state.borderTexLoaded) {
        UnloadTexture(state.borderTexture);
        UnloadImage(state.borderImage);
        state.borderTexture = {0}; state.borderImage = {0};
    }
    state.mapTexture = {0};
    state.planeIcon = {0};
    state.shipIcon = {0};
    state.airportIcon = {0};
    state.portIcon = {0};
    state.mapLoaded = false;
    state.planeIconLoaded = false;
    state.shipIconLoaded = false;
    state.airportIconLoaded = false;
    state.portIconLoaded = false;
    state.maskImageLoaded = false;
    state.borderTexLoaded = false;
    state.regionBorderPixels.clear();
    state.regionPolygons.clear();
    state.seaObstacles.clear();
    state.notifications.clear();
    state.modalAlert = {};
    state.showJournal = false;
    state.milestoneAlerts = 0;
    state.pauseMenuOpen = false;
    state.regions.clear();
    state.vehicles.clear();
}

// ═══════════════════════════════════════════════════════════════
// UPDATE
// ═══════════════════════════════════════════════════════════════

static void UpdateTopBarControls() {
    GameplayState& state = gGameState.gameplay;
    float sw = (float)GetScreenWidth();
    Vector2 mouse = GetMousePosition();

    float ctrlX = sw - 200.0f;
    Rectangle btnPause   = {ctrlX,        22.0f, 28.0f, 28.0f};
    Rectangle btnPlay    = {ctrlX + 34.0f, 22.0f, 28.0f, 28.0f};
    Rectangle btnFast    = {ctrlX + 68.0f, 22.0f, 34.0f, 28.0f};
    Rectangle btnFastest = {ctrlX + 108.0f, 22.0f, 40.0f, 28.0f};
    Rectangle btnClose   = {sw - 48.0f, 22.0f, 32.0f, 28.0f};

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;
    if (CheckCollisionPointRec(mouse, btnPause)) state.paused = true;
    if (CheckCollisionPointRec(mouse, btnPlay)) { state.paused = false; state.speedMultiplier = GetDefaultGameplaySpeed(); }
    if (CheckCollisionPointRec(mouse, btnFast)) { state.paused = false; state.speedMultiplier = state.speedMultiplier >= 4 ? 2 : 4; }
    if (CheckCollisionPointRec(mouse, btnFastest)) { state.paused = false; state.speedMultiplier = state.speedMultiplier == 8 ? 1 : 8; }
    if (CheckCollisionPointRec(mouse, btnClose)) gGameState.currentScreen = SCREEN_MENU;
}

static Rectangle GetPauseMenuRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float width = 420.0f;
    float height = 430.0f;
    return {sw * 0.5f - width * 0.5f, sh * 0.5f - height * 0.5f, width, height};
}

static Rectangle GetPauseMenuButtonRect(Rectangle menu, int index) {
    return {menu.x + 62.0f, menu.y + 210.0f + (float)index * 58.0f, menu.width - 124.0f, 44.0f};
}

static void ResumeFromPauseMenu(GameplayState& state) {
    state.pauseMenuOpen = false;
    state.paused = state.labPlacementActive || state.gameWon || state.gameLost;
}

static void OpenPauseMenu(GameplayState& state) {
    state.pauseMenuOpen = true;
    state.paused = true;
}

static void UpdatePauseMenu(GameplayState& state) {
    Rectangle menu = GetPauseMenuRect();
    Vector2 mouse = GetMousePosition();

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        ResumeFromPauseMenu(state);
        return;
    }

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;

    if (CheckCollisionPointRec(mouse, GetPauseMenuButtonRect(menu, 0))) {
        ResumeFromPauseMenu(state);
    } else if (CheckCollisionPointRec(mouse, GetPauseMenuButtonRect(menu, 1))) {
        ResetGameplaySession();
    } else if (CheckCollisionPointRec(mouse, GetPauseMenuButtonRect(menu, 2))) {
        state.pauseMenuOpen = false;
        gGameState.currentScreen = SCREEN_MENU;
    }
}

void UpdateGameplay(float delta) {
    GameplayState& state = gGameState.gameplay;
    Vector2 mouse = GetMousePosition();
    bool escapeConsumed = false;
    state.hoveredRegion = GetRegionAtMask(state, mouse, GetScreenWidth(), GetScreenHeight());
    UpdateNotifications(delta);
    UpdateModalAlert(delta);

    if (state.showJournal) {
        state.paused = true;
        JournalResult result = state.journal.Update(
            state.patientCardGame.GetActiveVirus(),
            ArePatientCardsUnlocked(state),
            IsPathogenSequencerUnlocked(state),
            IsElectronMicroscopeUnlocked(state),
            IsSerologyAssayUnlocked(state),
            IsThermalChamberUnlocked(state)
        );
        if (result == JournalResult::Closed) {
            state.showJournal = false;
            state.paused = state.journalPreviousPaused;
        } else if (result == JournalResult::LaunchPatientCards) {
            state.showJournal = false;
            state.patientCardPreviousPaused = state.journalPreviousPaused;
            state.patientCardGame.Start();
            state.paused = true;
        } else if (result == JournalResult::LaunchPathogenSequencer) {
            state.showJournal = false;
            state.pathogenSequencerGame.Start(state.patientCardGame.GetActiveVirus().pathogen);
            state.paused = true;
        } else if (result == JournalResult::LaunchElectronMicroscope) {
            state.showJournal = false;
            state.electronMicroscopeGame.Start(state.patientCardGame.GetActiveVirus().morphology);
            state.paused = true;
        } else if (result == JournalResult::LaunchSerologyAssay) {
            state.showJournal = false;
            state.serologyAssayGame.Start(state.patientCardGame.GetActiveVirus().immuneResponse);
            state.paused = true;
        } else if (result == JournalResult::LaunchThermalChamber) {
            state.showJournal = false;
            state.thermalChamberGame.Start(state.patientCardGame.GetActiveVirus().thermalStability);
            state.paused = true;
        } else if (result == JournalResult::CorrectDiagnosis) {
            state.diagnosisConfirmed = true;
            state.showJournal = false;
            state.paused = state.journalPreviousPaused;
            if (state.lab.overallProgress < 0.50f) {
                state.lab.stage = LAB_STAGE_TRIALS;
                state.lab.stageProgress = 0.50f;
                SyncLabStageProgress(state);
            }
            PushNotification(u8"ДИАГНОЗ ПОДТВЕРЖДЕН: вакцина готова на 50%, дальнейшая разработка ускорена",
                             {84, 214, 145, 255}, 7.0f);
        } else if (result == JournalResult::WrongDiagnosis) {
            const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
            int penalty = std::max(1, (int)std::round(6.0f * difficulty.actionCostMultiplier));
            state.lab.resources = std::max(0, state.lab.resources - penalty);
            PushNotification(u8"ОШИБОЧНЫЙ ДИАГНОЗ: ресурсы потеряны, расследование продолжается",
                             {244, 112, 105, 255}, 6.0f);
        }
        return;
    }

    if (state.patientCardGame.IsActive()) {
        state.paused = true;
        PatientCardResult result = state.patientCardGame.Update();
        if (result == PatientCardResult::Correct) {
            char message[192];
            std::snprintf(message, sizeof(message), u8"Клиническая улика получена: %s",
                          state.patientCardGame.GetLastChosenSymptomName());
            PushNotification(message, {75, 194, 255, 255}, 6.0f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == PatientCardResult::Wrong) {
            state.lab.resources = std::max(0, state.lab.resources - 4);
            AdvanceDate();
            PushNotification(u8"Ошибка клинического анализа: потеряны ресурсы и один день",
                             {244, 112, 105, 255}, 5.0f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == PatientCardResult::Cancelled) {
            state.showJournal = true;
            state.paused = true;
        }
        return;
    }

    if (state.pathogenSequencerGame.IsActive()) {
        state.paused = true;
        PathogenSequencerResult result = state.pathogenSequencerGame.Update(delta);
        if (result == PathogenSequencerResult::Correct) {
            char message[192];
            std::snprintf(message, sizeof(message), u8"Генетическая улика получена: %s",
                          PathogenEvidenceName(state.patientCardGame.GetActiveVirus().pathogen));
            PushNotification(message, {74, 218, 187, 255}, 6.0f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == PathogenSequencerResult::Wrong) {
            state.lab.resources = std::max(0, state.lab.resources - 5);
            AdvanceDate();
            PushNotification(u8"Ошибка секвенирования: образец испорчен, потеряны ресурсы и один день",
                             {244, 112, 105, 255}, 5.5f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == PathogenSequencerResult::Cancelled) {
            state.showJournal = true;
            state.paused = true;
        }
        return;
    }

    if (state.electronMicroscopeGame.IsActive()) {
        state.paused = true;
        ElectronMicroscopeResult result = state.electronMicroscopeGame.Update(delta);
        if (result == ElectronMicroscopeResult::Correct) {
            char message[192];
            std::snprintf(message, sizeof(message), u8"Морфологическая улика получена: %s",
                          MorphologyEvidenceName(state.patientCardGame.GetActiveVirus().morphology));
            PushNotification(message, {194, 119, 255, 255}, 6.0f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == ElectronMicroscopeResult::Wrong) {
            state.lab.resources = std::max(0, state.lab.resources - 6);
            AdvanceDate();
            PushNotification(u8"Ошибка микроскопии: препарат поврежден, потеряны ресурсы и один день",
                             {244, 112, 105, 255}, 5.5f);
            state.showJournal = true;
            state.paused = true;
        } else if (result == ElectronMicroscopeResult::Cancelled) {
            state.showJournal = true;
            state.paused = true;
        }
        return;
    }

    if (state.serologyAssayGame.IsActive()) {
        state.paused = true;
        SerologyAssayResult result = state.serologyAssayGame.Update();
        if (result == SerologyAssayResult::Correct) {
            char message[192];
            std::snprintf(message, sizeof(message), u8"Серологическая улика получена: %s",
                          ImmuneResponseEvidenceName(state.patientCardGame.GetActiveVirus().immuneResponse));
            PushNotification(message, {255, 122, 170, 255}, 6.0f);
            state.showJournal = true;
        } else if (result == SerologyAssayResult::Wrong) {
            state.lab.resources = std::max(0, state.lab.resources - 5);
            AdvanceDate();
            PushNotification(u8"Ошибка серологического теста: потеряны ресурсы и один день",
                             {244, 112, 105, 255}, 5.5f);
            state.showJournal = true;
        } else if (result == SerologyAssayResult::Cancelled) {
            state.showJournal = true;
        }
        return;
    }

    if (state.thermalChamberGame.IsActive()) {
        state.paused = true;
        ThermalChamberResult result = state.thermalChamberGame.Update(delta);
        if (result == ThermalChamberResult::Correct) {
            char message[192];
            std::snprintf(message, sizeof(message), u8"Температурная улика получена: %s",
                          ThermalStabilityEvidenceName(state.patientCardGame.GetActiveVirus().thermalStability));
            PushNotification(message, {255, 155, 73, 255}, 6.0f);
            state.showJournal = true;
        } else if (result == ThermalChamberResult::Wrong) {
            state.lab.resources = std::max(0, state.lab.resources - 5);
            AdvanceDate();
            PushNotification(u8"Ошибка термотеста: культура потеряна, ресурсы и день списаны",
                             {244, 112, 105, 255}, 5.5f);
            state.showJournal = true;
        } else if (result == ThermalChamberResult::Cancelled) {
            state.showJournal = true;
        }
        return;
    }

    if (state.modalAlert.active) {
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        Rectangle box = {sw * 0.5f - 270.0f, sh * 0.5f - 110.0f, 540.0f, 220.0f};
        Rectangle button = {box.x + box.width - 158.0f, box.y + box.height - 48.0f, 132.0f, 32.0f};
        bool closePressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, button)) closePressed = true;
        if (closePressed && !state.modalAlert.closing) CloseModalAlert();
        return;
    }

    if (state.lab.menuOpen) {
        UpdateLabMenu(state);
        return;
    }

    if (state.pauseMenuOpen) {
        UpdatePauseMenu(state);
        return;
    }

    if (state.labPlacementActive) {
        bool cardClicked = CheckCollisionPointRec(mouse, GetLabPlacementCardRect());
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !cardClicked && state.hoveredRegion >= 0) {
            PlaceLaboratoryInRegion(state.hoveredRegion);
        }
        if (IsKeyPressed(KEY_ESCAPE)) OpenPauseMenu(state);
        return;
    }

    bool resourceMarkerClicked = UpdateResourceMarker(state, delta, mouse);
    bool clickedBottomUI = mouse.y >= (float)GetScreenHeight() - 260.0f;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !clickedBottomUI && !resourceMarkerClicked) {
        if (state.hoveredRegion >= 0) {
            for (Region& region : state.regions) region.isSelected = false;
            state.selectedRegion = state.hoveredRegion;
            state.regions[state.selectedRegion].isSelected = true;
            state.debugSelectedHub = -1;
        }
    }

    UpdateTopBarControls();

    if (IsKeyPressed(KEY_J)) {
        state.journalPreviousPaused = state.paused;
        state.showJournal = true;
        state.paused = true;
        return;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, GetLabButtonRect(GetLabPanelRect()))) {
        state.lab.previousPaused = state.paused;
        state.lab.menuOpen = true;
        state.paused = true;
        return;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, GetJournalButtonRect(GetLabPanelRect()))) {
        state.journalPreviousPaused = state.paused;
        state.showJournal = true;
        state.paused = true;
        return;
    }

    if (!state.paused && !state.gameWon && !state.gameLost) {
        const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
        UpdateVehicles(state.vehicles, state.regions, GetSeaObstacles(state), delta, state.speedMultiplier,
                       difficulty.transportTransmissionMultiplier);
        state.dayAccumulator += delta * (float)state.speedMultiplier;
        while (state.dayAccumulator >= 1.0f) {
            state.dayAccumulator -= 1.0f;
            AdvanceDate();
            SimulateDay();
        }
        DetectResourceMarkerEvents(state);
    }

    state.notificationTimer -= delta;
    if (state.notificationTimer <= 0.0f) {
        state.notificationTimer = 7.5f;
        PushNewsNotification();
        state.newsIndex = (state.newsIndex + 1) % 5;
    }

    UpdateBorderOverlay(state);

    if (state.debugDrawMode && !state.debugTypingName) {
        Rectangle panel = GetTransportDebugPanelRect();
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mousePos, GetEditorModeButtonRect(panel, 0))) {
                state.debugEditorMode = 0;
                state.debugTypingName = false;
                state.debugSelectedHub = -1;
            } else if (CheckCollisionPointRec(mousePos, GetEditorModeButtonRect(panel, 1))) {
                state.debugEditorMode = 1;
                state.debugTypingName = false;
                state.debugSelectedHub = -1;
            } else if (state.debugEditorMode == 1) {
                if (CheckCollisionPointRec(mousePos, GetTransportDebugButtonRect(panel, 0))) {
                    state.debugTransportMode = HUB_AIRPORT;
                } else if (CheckCollisionPointRec(mousePos, GetTransportDebugButtonRect(panel, 1))) {
                    state.debugTransportMode = HUB_PORT;
                } else if (CheckCollisionPointRec(mousePos, GetTransportDebugButtonRect(panel, 2))) {
                    AddTransportHubToSelectedRegion((TransportHubType)state.debugTransportMode, mousePos);
                } else if (CheckCollisionPointRec(mousePos, GetTransportDebugButtonRect(panel, 3))) {
                    ToggleSelectedTransportHub();
                } else if (CheckCollisionPointRec(mousePos, GetTransportDebugButtonRect(panel, 4))) {
                    DeleteSelectedTransportHub();
                }
            }
        }

        if (state.debugEditorMode == 1) {
            if (IsKeyPressed(KEY_A)) AddTransportHubToSelectedRegion(HUB_AIRPORT, mousePos);
            if (IsKeyPressed(KEY_P)) AddTransportHubToSelectedRegion(HUB_PORT, mousePos);
            if (IsKeyPressed(KEY_T)) ToggleSelectedTransportHub();
            if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) DeleteSelectedTransportHub();
        }
    }

    if (state.debugDrawMode && state.debugEditorMode == 1 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && state.selectedRegion >= 0) {
        bool hubClicked = SelectTransportHubAtMouse(mouse);
        if (!hubClicked) state.debugSelectedHub = -1;
    }

    bool ctrlD = IsKeyPressed(KEY_D) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    bool shiftD = IsKeyPressed(KEY_D) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if (ctrlD || shiftD) {
        // Ctrl+D or Shift+D toggles draw mode
        state.debugDrawMode = !state.debugDrawMode;
        state.debugTypingName = false;
        state.debugPolygon.clear();
        state.debugRegionName[0] = '\0';
        state.debugNameLen = 0;
        state.debugSelectedHub = -1;
        state.debugEditorMode = 0;
    } else if (IsKeyPressed(KEY_D)) {
        state.debugShowCoords = !state.debugShowCoords;
    }

    if (state.debugDrawMode) {
        if (IsKeyPressed(KEY_ONE)) {
            state.debugEditorMode = 0;
            state.debugTypingName = false;
            state.debugSelectedHub = -1;
        }
        if (IsKeyPressed(KEY_TWO)) {
            state.debugEditorMode = 1;
            state.debugTypingName = false;
            state.debugSelectedHub = -1;
        }
    }

    bool startedTypingThisFrame = false;

    if (state.debugDrawMode && state.debugEditorMode == 0 && !state.debugTypingName && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Rectangle mapRect = GetMapRect(state);
        float u = (mouse.x - mapRect.x) / mapRect.width;
        float v = (mouse.y - mapRect.y) / mapRect.height;

        // Snap/close if near the first point
        if (state.debugPolygon.size() >= 3) {
            Vector2 first = {mapRect.x + state.debugPolygon[0].x * mapRect.width,
                             mapRect.y + state.debugPolygon[0].y * mapRect.height};
            float dx = mouse.x - first.x;
            float dy = mouse.y - first.y;
            const float snapRadius = 14.0f;
            if (dx*dx + dy*dy <= snapRadius * snapRadius) {
                state.debugTypingName = true;
                state.debugNameLen = 0;
                state.debugRegionName[0] = '\0';
                startedTypingThisFrame = true;
            }
        }

        if (!state.debugTypingName) {
            state.debugPolygon.push_back({u, v});
        }
    }

    // Finish polygon and start typing name
    if (state.debugDrawMode && state.debugEditorMode == 0 && !state.debugTypingName && IsKeyPressed(KEY_ENTER)) {
        state.debugTypingName = true;
        state.debugNameLen = 0;
        state.debugRegionName[0] = '\0';
        startedTypingThisFrame = true;
    }

    // Typing region name
    if (state.debugDrawMode && state.debugEditorMode == 0 && state.debugTypingName && !startedTypingThisFrame) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key == 8) { // backspace
                if (state.debugNameLen > 0) {
                    state.debugNameLen--;
                    state.debugRegionName[state.debugNameLen] = '\0';
                }
            } else if (key >= 32 && key < 127 && state.debugNameLen < 63) {
                state.debugRegionName[state.debugNameLen++] = (char)key;
                state.debugRegionName[state.debugNameLen] = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (state.debugNameLen > 0) {
                state.debugNameLen--;
                state.debugRegionName[state.debugNameLen] = '\0';
            }
        }
        if (IsKeyPressed(KEY_ENTER)) {
            // Save to JSON only if name and polygon are valid
            if (state.debugPolygon.size() >= 3 && state.debugNameLen > 0) {
                std::string outPath = ResourcePath("regions_drawn.json");
                FILE* f = fopen(outPath.c_str(), "a");
                if (f) {
                    fprintf(f, "{ \"name\": \"%s\", \"points\": [", state.debugRegionName);
                    for (size_t i = 0; i < state.debugPolygon.size(); ++i) {
                        fprintf(f, "%s{ \"x\": %.5f, \"y\": %.5f }", (i==0?"":" ,"), state.debugPolygon[i].x, state.debugPolygon[i].y);
                    }
                    fprintf(f, " ] }\n");
                    fclose(f);
                }
                state.debugPolygon.clear();
                state.debugRegionName[0] = '\0';
                state.debugNameLen = 0;
                state.debugTypingName = false;
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            state.debugTypingName = false;
            escapeConsumed = true;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) && !state.debugTypingName && !escapeConsumed) {
        OpenPauseMenu(state);
    }
}

// ═══════════════════════════════════════════════════════════════
// DRAW — MAP
// ═══════════════════════════════════════════════════════════════

static void DrawMap() {
    GameplayState& state = gGameState.gameplay;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Rectangle mapRect = GetMapRect(state);

    if (state.mapLoaded && state.mapTexture.id != 0) {
        DrawTexturePro(
            state.mapTexture,
            {0, 0, (float)state.mapTexture.width, (float)state.mapTexture.height},
            mapRect, {0, 0}, 0.0f, WHITE);
    } else {
        DrawProceduralMap(sw, sh);
    }

    DrawVignette(sw, sh, 60);
}

static void DrawResourceMarker() {
    const GameplayState& state = gGameState.gameplay;
    if (!state.resourceMarker.active) return;

    Vector2 position = GetResourceMarkerPosition(state);
    float appear = std::clamp(state.resourceMarker.age / 0.45f, 0.0f, 1.0f);
    appear = EaseOutCubic(appear);
    float remaining = std::clamp(
        1.0f - state.resourceMarker.age / state.resourceMarker.lifetime,
        0.0f, 1.0f);
    float pulse = 1.0f + std::sin(state.resourceMarker.age * 7.0f) * 0.08f;
    float radius = 22.0f * appear * pulse;
    Color gold = {242, 210, 88, 255};

    DrawLineEx({position.x, position.y + 14.0f},
               {position.x, position.y + 48.0f},
               3.0f * appear, {gold.r, gold.g, gold.b, (unsigned char)(90.0f * appear)});
    DrawEllipse((int)position.x, (int)(position.y + 48.0f),
                25.0f * appear, 8.0f * appear,
                {gold.r, gold.g, gold.b, (unsigned char)(45.0f * appear)});
    DrawCircleV(position, radius + 10.0f, {gold.r, gold.g, gold.b, (unsigned char)(34.0f * appear)});
    DrawCircleV(position, radius, {38, 31, 18, (unsigned char)(245.0f * appear)});
    DrawCircleLines((int)position.x, (int)position.y, radius, gold);
    DrawRing(position, radius + 5.0f, radius + 8.0f, -90.0f,
             -90.0f + 360.0f * remaining, 48,
             {gold.r, gold.g, gold.b, (unsigned char)(235.0f * appear)});
    DrawCenteredTextUI("$",
                       {position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f},
                       22.0f * appear, gold);

    char reward[32];
    std::snprintf(reward, sizeof(reward), "+$%d", state.resourceMarker.reward);
    Rectangle label = {position.x - 34.0f, position.y - 48.0f, 68.0f, 22.0f};
    DrawRectangleRounded(label, 0.35f, 8, {12, 16, 24, (unsigned char)(220.0f * appear)});
    DrawCenteredTextUI(reward, label, 13.0f, gold);
}

// ═══════════════════════════════════════════════════════════════
// DRAW — REGIONS (border overlay + infection tint)
// ═══════════════════════════════════════════════════════════════

static void DrawRegions() {
    GameplayState& state = gGameState.gameplay;
    Rectangle mapRect = GetMapRect(state);
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // Draw infection tint circles (small, subtle) — just a faint indicator
    for (int i = 0; i < (int)state.regions.size(); ++i) {
        const Region& region = state.regions[i];
        if (region.infectionRate <= 0.001f) continue;
        Vector2 center = {mapRect.x + region.mapPosition.x * mapRect.width,
                          mapRect.y + region.mapPosition.y * mapRect.height};
        Color c = region.overlayColor;
        c.a = (unsigned char)std::min(160, (int)c.a + 30);
        DrawCircleV(center, 12.0f, c);
    }

    // Draw border overlay texture over the map
    if (state.borderTexLoaded) {
        BeginBlendMode(BLEND_ALPHA);
        DrawTexturePro(
            state.borderTexture,
            {0, 0, (float)state.borderTexture.width, (float)state.borderTexture.height},
            mapRect, {0, 0}, 0.0f, WHITE);
        EndBlendMode();
    }

    for (int i = 0; i < (int)state.regions.size(); ++i) {
        const Region& region = state.regions[i];
        for (int j = 0; j < (int)region.transportHubs.size(); ++j) {
            bool selected = state.debugDrawMode && i == state.selectedRegion && j == state.debugSelectedHub;
            DrawTransportHubMarker(region.transportHubs[j], mapRect, selected);
        }
    }

    if (state.labRegionIndex >= 0 && state.labRegionIndex < (int)state.regions.size()) {
        DrawLaboratoryMarker(state.regions[state.labRegionIndex], mapRect);
    }

    DrawResourceMarker();

    if (state.labPlacementActive && state.hoveredRegion >= 0 && state.hoveredRegion < (int)state.regions.size()) {
        const Region& region = state.regions[state.hoveredRegion];
        Vector2 center = {mapRect.x + region.mapPosition.x * mapRect.width,
                          mapRect.y + region.mapPosition.y * mapRect.height};
        DrawCircleV(center, 28.0f, {70, 195, 255, 36});
        DrawCircleLines((int)center.x, (int)center.y, 28.0f, {120, 220, 255, 220});
        DrawCircleLines((int)center.x, (int)center.y, 34.0f, {120, 220, 255, 100});
    }

    // Tooltip
    if (state.hoveredRegion >= 0) {
        Vector2 mouse = GetMousePosition();
        const char* name = state.regions[state.hoveredRegion].name;
        Vector2 textSize = MeasureTextUI(name, 20.0f);
        Rectangle tip = {mouse.x + 16.0f, mouse.y - 34.0f, textSize.x + 20.0f, 30.0f};
        if (tip.y < 10.0f) tip.y = mouse.y + 10.0f;
        DrawRectangleRounded(tip, 0.25f, 8, {12, 16, 26, 225});
        DrawTextUI(name, {tip.x + 10.0f, tip.y + 5.0f}, 20.0f, WHITE);
    }

    // Debug overlay for coordinates
    if (state.debugShowCoords) {
        Vector2 mouse = GetMousePosition();
        Rectangle mapRect = GetMapRect(state);
        float u = (mouse.x - mapRect.x) / mapRect.width;
        float v = (mouse.y - mapRect.y) / mapRect.height;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "mouse u=%.3f v=%.3f", u, v);
        DrawRectangleRounded({10, 60, 240, 46}, 0.1f, 6, {8,12,18,200});
        DrawTextUI(buf, {18, 70}, 16, WHITE);

        if (state.hoveredRegion >= 0) {
            const Region& r = state.regions[state.hoveredRegion];
            std::snprintf(buf, sizeof(buf), "%s -> x=%.3f y=%.3f", r.name, r.mapPosition.x, r.mapPosition.y);
            DrawRectangleRounded({10, 110, 300, 46}, 0.1f, 6, {8,12,18,200});
            DrawTextUI(buf, {18, 120}, 16, {200,220,255,255});
            // mark region center
            Vector2 c = {mapRect.x + r.mapPosition.x * mapRect.width,
                         mapRect.y + r.mapPosition.y * mapRect.height};
            DrawCircleLines((int)c.x, (int)c.y, 6.0f, {255, 200, 80, 240});
            DrawLine((int)c.x - 8, (int)c.y, (int)c.x + 8, (int)c.y, {255,200,80,240});
            DrawLine((int)c.x, (int)c.y - 8, (int)c.x, (int)c.y + 8, {255,200,80,240});
        }
    }

    // Debug draw mode visualization
    if (state.debugDrawMode) {
        bool countryMode = state.debugEditorMode == 0;
        DrawRectangleRounded({10, 170, 420, countryMode ? 110.0f : 126.0f}, 0.1f, 6, {8,12,18,220});
        DrawTextUI("DRAW MODE (Ctrl+D or Shift+D)", {18, 180}, 16, {255,220,120,255});
        DrawTextUI("1 = countries   2 = airports", {18, 200}, 16, {180,220,255,255});
        DrawTextUI(countryMode ? "MODE 1: DRAW COUNTRIES" : "MODE 2: AIRPORTS / PORTS", {18, 220}, 16, WHITE);
        DrawTextUI(countryMode ? "LMB: add point | Enter: finish" : "LMB: select region or hub", {18, 240}, 16, WHITE);
        DrawTextUI(countryMode ? "Click start point to close (snap)" : "A: airport | P: port | T: toggle | Del: delete", {18, 260}, 16, {200,220,255,255});
        Vector2 mouse = GetMousePosition();
        Rectangle mapRect = GetMapRect(state);
        float u = (mouse.x - mapRect.x) / mapRect.width;
        float v = (mouse.y - mapRect.y) / mapRect.height;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "u=%.3f v=%.3f", u, v);
        DrawTextUI(buf, {18, 280}, 16, {180,200,255,255});

        // Draw polygon lines
        for (size_t i = 0; i < state.debugPolygon.size(); ++i) {
            Vector2 a = {mapRect.x + state.debugPolygon[i].x * mapRect.width,
                         mapRect.y + state.debugPolygon[i].y * mapRect.height};
            DrawCircleV(a, 4.0f, {255, 200, 80, 255});
            if (i > 0) {
                Vector2 b = {mapRect.x + state.debugPolygon[i-1].x * mapRect.width,
                             mapRect.y + state.debugPolygon[i-1].y * mapRect.height};
                DrawLineEx(a, b, 2.0f, {255, 200, 80, 220});
            }
        }

        // closing hint line
        if (state.debugPolygon.size() >= 2) {
            Vector2 first = {mapRect.x + state.debugPolygon[0].x * mapRect.width,
                             mapRect.y + state.debugPolygon[0].y * mapRect.height};
            Vector2 last  = {mapRect.x + state.debugPolygon.back().x * mapRect.width,
                             mapRect.y + state.debugPolygon.back().y * mapRect.height};
            DrawLineEx(last, first, 1.5f, {255, 160, 80, 180});

            // snap highlight when cursor near first point
            float dx = mouse.x - first.x;
            float dy = mouse.y - first.y;
            const float snapRadius = 14.0f;
            if (dx*dx + dy*dy <= snapRadius * snapRadius) {
                DrawCircleLines((int)first.x, (int)first.y, snapRadius, {120, 220, 255, 200});
                DrawTextUI("Snap: click to close", {first.x + 10.0f, first.y - 18.0f}, 14, {200,240,255,255});
            }
        }

        // Typing name overlay
        if (state.debugTypingName) {
            DrawRectangleRounded({10, 240, 340, 70}, 0.1f, 6, {12,16,26,230});
            DrawTextUI("Enter region name, Enter to save", {18, 250}, 16, WHITE);
            DrawRectangleRounded({18, 272, 320, 26}, 0.2f, 6, {4,8,12,240});
            DrawTextUI(state.debugRegionName[0] ? state.debugRegionName : "", {24, 276}, 16, {200,220,255,255});
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// DRAW — TOP BAR (like reference: NEWS | date | speed | X)
// ═══════════════════════════════════════════════════════════════

static void DrawSpeedButton(Rectangle rect, const char* label, bool active) {
    Color border = active ? Color{235, 242, 250, 135} : Color{255, 255, 255, 36};
    Color fill = active ? Color{220, 228, 236, 218} : Color{22, 27, 34, 196};
    DrawRectangleRounded(rect, 0.18f, 14, border);
    Rectangle inner = {rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f};
    DrawRectangleRounded(inner, 0.18f, 14, fill);
    DrawCenteredTextUI(label, inner, 14.5f, active ? Color{10, 14, 20, 255} : Color{205, 214, 225, 240});
}

static void DrawTopBar() {
    GameplayState& state = gGameState.gameplay;
    const DifficultyProfile& difficulty = GetDifficultyProfile(state.difficulty);
    float sw = (float)GetScreenWidth();
    Rectangle bar = {12.0f, 12.0f, sw - 24.0f, 48.0f};
    DrawPanel(bar, {70, 195, 255, 255});

    DrawTextUI("ANTIVIRUS OPS", {bar.x + 18.0f, bar.y + 8.0f}, 16.5f, {148, 210, 242, 255});
    DrawTextUI(u8"глобальный мониторинг", {bar.x + 18.0f, bar.y + 29.0f}, 14.5f, {168, 182, 202, 255});
    DrawTextUI(difficulty.name, {bar.x + 235.0f, bar.y + 17.0f}, 13.5f, {202, 214, 228, 220});
    if (state.virusMutationTriggered) {
        Rectangle badge = {bar.x + 350.0f, bar.y + 13.0f, 108.0f, 22.0f};
        DrawRectangleRounded(badge, 0.45f, 12, {120, 20, 24, 210});
        DrawTextUI(u8"МУТАЦИЯ", {badge.x + 16.0f, badge.y + 4.0f}, 12.5f, {255, 200, 200, 255});
    }

    char date[32];
    std::snprintf(date, sizeof(date), "%02d.%02d.%d", state.day, state.month, state.year);
    Vector2 dateSize = MeasureTextUI(date, 22.0f);
    DrawTextUI(date, {sw * 0.5f - dateSize.x * 0.5f, bar.y + 13.0f}, 22.0f, WHITE);

    float ctrlX = sw - 200.0f;
    DrawSpeedButton({ctrlX,         22.0f, 28.0f, 28.0f}, "||", state.paused);
    DrawSpeedButton({ctrlX + 34.0f, 22.0f, 28.0f, 28.0f}, ">",  !state.paused && state.speedMultiplier == 1);
    DrawSpeedButton({ctrlX + 68.0f, 22.0f, 34.0f, 28.0f}, ">>", !state.paused && (state.speedMultiplier == 2 || state.speedMultiplier == 4));
    DrawSpeedButton({ctrlX + 108.0f,22.0f, 40.0f, 28.0f}, ">>>",!state.paused && state.speedMultiplier == 8);

    DrawSpeedButton({sw - 48.0f, 22.0f, 32.0f, 28.0f}, "X", false);
}

static void DrawNotifications() {
    GameplayState& state = gGameState.gameplay;
    float sw = (float)GetScreenWidth();
    float y = 78.0f;

    for (const Notification& item : state.notifications) {
        float fadeIn = std::clamp(item.age / 0.22f, 0.0f, 1.0f);
        float fadeOut = std::clamp((item.duration - item.age) / 0.45f, 0.0f, 1.0f);
        unsigned char alpha = (unsigned char)(220.0f * std::min(fadeIn, fadeOut));
        Rectangle card = {sw - 390.0f, y, 372.0f, 58.0f};
        DrawRectangleRounded({card.x, card.y + 5.0f, card.width, card.height}, 0.12f, 18, {0, 0, 0, (unsigned char)(alpha / 3)});
        DrawRectangleRounded(card, 0.12f, 20, {255, 255, 255, (unsigned char)(alpha * 0.18f)});
        DrawRectangleRounded({card.x + 1.0f, card.y + 1.0f, card.width - 2.0f, card.height - 2.0f},
                             0.12f, 20, {14, 18, 24, (unsigned char)(alpha * 0.92f)});
        DrawCircleV({card.x + 19.0f, card.y + 29.0f}, 4.0f, {item.accent.r, item.accent.g, item.accent.b, alpha});
        DrawTextUI("NOTICE", {card.x + 34.0f, card.y + 8.0f}, 12.5f, {165, 176, 188, alpha});
        DrawTextUI(item.text, {card.x + 34.0f, card.y + 28.0f}, 16.5f, {232, 238, 245, alpha});
        y += 66.0f;
    }
}

static void DrawTransportDebugPanel() {
    GameplayState& state = gGameState.gameplay;
    if (!state.debugDrawMode) return;

    Rectangle panel = GetTransportDebugPanelRect();
    DrawRectangleRounded(panel, 0.08f, 8, {18, 24, 36, 225});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 1.0f, {80, 110, 150, 90});
    DrawTextUI("DEBUG EDITOR", {panel.x + 12.0f, panel.y + 10.0f}, 16.0f, {165, 175, 195, 255});

    const char* editorLabels[2] = {"1 COUNTRIES", "2 AIRPORTS"};
    for (int i = 0; i < 2; ++i) {
        Rectangle rect = GetEditorModeButtonRect(panel, i);
        bool active = (state.debugEditorMode == i);
        Color top = active ? Color{80, 140, 210, 255} : Color{45, 58, 76, 255};
        Color bottom = active ? Color{50, 98, 168, 255} : Color{28, 36, 50, 255};
        DrawCapsuleGradient(rect, top, bottom, {255, 255, 255, 28}, {0, 0, 0, 65});
        DrawCenteredTextUI(editorLabels[i], rect, 12.0f, WHITE);
    }

    if (state.debugEditorMode == 0) {
        DrawTextUI("DRAW COUNTRY POLYGONS", {panel.x + 12.0f, panel.y + 50.0f}, 14.0f, {110, 180, 220, 255});
        DrawTextUI("LMB: add point | Enter: finish", {panel.x + 12.0f, panel.y + 66.0f}, 12.0f, {190, 200, 215, 255});
        DrawTextUI("Click first point to close", {panel.x + 12.0f, panel.y + 80.0f}, 12.0f, {190, 200, 215, 255});
        DrawTextUI("1 switches here", {panel.x + 12.0f, panel.y + 96.0f}, 12.0f, {130, 210, 255, 255});
    } else {
        DrawTextUI("AIRPORT / PORT EDITOR", {panel.x + 12.0f, panel.y + 50.0f}, 14.0f, {110, 180, 220, 255});
        DrawTextUI("LMB: select region or hub", {panel.x + 12.0f, panel.y + 66.0f}, 12.0f, {190, 200, 215, 255});
        DrawTextUI("A = airport, P = port", {panel.x + 12.0f, panel.y + 80.0f}, 12.0f, {190, 200, 215, 255});
        DrawTextUI("2 switches here", {panel.x + 12.0f, panel.y + 94.0f}, 12.0f, {130, 210, 255, 255});
    }

    if (state.selectedRegion >= 0 && state.selectedRegion < (int)state.regions.size()) {
        const Region& region = state.regions[state.selectedRegion];
        DrawTextUI(region.name, {panel.x + 12.0f, panel.y + 112.0f}, 18.0f, WHITE);

        char counts[128];
        std::snprintf(counts, sizeof(counts), "A %d  P %d", CountTransportHubs(region, HUB_AIRPORT), CountTransportHubs(region, HUB_PORT));
        DrawTextUI(counts, {panel.x + 12.0f, panel.y + 134.0f}, 15.0f, {210, 220, 235, 255});

        const char* modeText = state.debugTransportMode == HUB_AIRPORT ? "mode: airport" : "mode: port";
        DrawTextUI(modeText, {panel.x + 12.0f, panel.y + 148.0f}, 13.0f, {130, 210, 255, 255});

        if (state.debugSelectedHub >= 0 && state.debugSelectedHub < (int)region.transportHubs.size()) {
            const TransportHub& hub = region.transportHubs[state.debugSelectedHub];
            char hubInfo[160];
            std::snprintf(hubInfo, sizeof(hubInfo), "%s [%s]", hub.name, hub.closed ? "closed" : "open");
            DrawTextUI(hubInfo, {panel.x + 12.0f, panel.y + 164.0f}, 13.0f, hub.closed ? Color{255, 120, 120, 255} : Color{180, 220, 255, 255});
            DrawTextUI("T: toggle  Del: delete", {panel.x + 12.0f, panel.y + 210.0f}, 12.0f, {190, 200, 215, 255});
        }
    } else {
        DrawTextUI("no region selected", {panel.x + 12.0f, panel.y + 112.0f}, 16.0f, {180, 190, 205, 255});
    }

    if (state.debugEditorMode == 1) {
        const char* labels[5] = {"AIRPORT", "PORT", "ADD", "TOGGLE", "DELETE"};
        for (int i = 0; i < 5; ++i) {
            Rectangle rect = GetTransportDebugButtonRect(panel, i);
            bool active = (i == 0 && state.debugTransportMode == HUB_AIRPORT) ||
                          (i == 1 && state.debugTransportMode == HUB_PORT) ||
                          (i == 3 && state.debugSelectedHub >= 0) ||
                          (i == 4 && state.debugSelectedHub >= 0);
            Color top = active ? Color{80, 140, 210, 255} : Color{45, 58, 76, 255};
            Color bottom = active ? Color{50, 98, 168, 255} : Color{28, 36, 50, 255};
            DrawCapsuleGradient(rect, top, bottom, {255, 255, 255, 28}, {0, 0, 0, 65});
            DrawCenteredTextUI(labels[i], rect, 13.0f, WHITE);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// DRAW — BOTTOM LEFT: DISEASE panel (Reputation, Resources)
// ═══════════════════════════════════════════════════════════════

static void DrawDiseasePanel() {
    const DifficultyProfile& difficulty = GetDifficultyProfile(gGameState.gameplay.difficulty);
    float sh = (float)GetScreenHeight();
    Rectangle panel = {16.0f, sh - 126.0f, 242.0f, 106.0f};
    DrawPanel(panel, {84, 214, 145, 255});

    DrawTextUI("RESPONSE", {panel.x + 18.0f, panel.y + 10.0f}, 15.5f, {155, 224, 186, 255});
    DrawTextUI(u8"оперативный штаб", {panel.x + 18.0f, panel.y + 29.0f}, 13.5f, {150, 165, 185, 255});
    DrawMetric(u8"Репутация", "100", {panel.x + 18.0f, panel.y + 54.0f}, {100, 224, 150, 255});
    char resources[16];
    std::snprintf(resources, sizeof(resources), "%d", difficulty.startingResources);
    DrawMetric(u8"Ресурсы", resources, {panel.x + 134.0f, panel.y + 54.0f}, {242, 210, 88, 255});
    DrawMiniBar({panel.x + 18.0f, panel.y + 94.0f, panel.width - 36.0f, 5.0f}, 0.52f, {70, 195, 255, 255});
}

// ═══════════════════════════════════════════════════════════════
// DRAW — BOTTOM CENTER: Region Info Panel
// ═══════════════════════════════════════════════════════════════

static void DrawLabPanel() {
    GameplayState& state = gGameState.gameplay;
    Rectangle panel = GetLabPanelRect();
    DrawPanel(panel, {70, 195, 255, 255});

    DrawTextUI(GetLabDisplayName(), {panel.x + 18.0f, panel.y + 10.0f}, 15.0f, {155, 224, 186, 255});
    DrawTextUI(LabStageName(state.lab.stage), {panel.x + 18.0f, panel.y + 31.0f}, 20.0f, WHITE);
    DrawMiniBar({panel.x + 18.0f, panel.y + 61.0f, panel.width - 36.0f, 7.0f},
                state.lab.overallProgress, {70, 195, 255, 255});

    char status[96];
    std::snprintf(status, sizeof(status), u8"$ %d    Доверие %d    +%d/день",
                  state.lab.resources, state.lab.trust, GetLabDailyIncome(state));
    DrawTextUI(status, {panel.x + 18.0f, panel.y + 78.0f}, 14.0f, {205, 216, 230, 255});

    Rectangle labButton = GetLabButtonRect(panel);
    Rectangle journalButton = GetJournalButtonRect(panel);
    Vector2 mouse = GetMousePosition();
    bool labHovered = CheckCollisionPointRec(mouse, labButton);
    bool journalHovered = CheckCollisionPointRec(mouse, journalButton);

    DrawRectangleRounded(labButton, 0.18f, 10, {105, 196, 255, 170});
    DrawRectangleRounded({labButton.x + 1.0f, labButton.y + 1.0f, labButton.width - 2.0f, labButton.height - 2.0f},
                         0.18f, 10, labHovered ? Color{42, 102, 145, 245} : Color{34, 82, 120, 235});
    DrawCenteredTextUI(u8"ЛАБОРАТОРИЯ", labButton, 11.5f, WHITE);

    DrawRectangleRounded(journalButton, 0.18f, 10, {80, 218, 187, 170});
    DrawRectangleRounded({journalButton.x + 1.0f, journalButton.y + 1.0f,
                          journalButton.width - 2.0f, journalButton.height - 2.0f},
                         0.18f, 10, journalHovered ? Color{35, 116, 107, 245} : Color{27, 88, 84, 235});
    DrawCenteredTextUI(u8"БЛОКНОТ  [J]", journalButton, 11.5f, WHITE);
}

static Color GetLabBranchColor(LabUpgradeBranch branch) {
    static const Color colors[LAB_BRANCH_COUNT] = {
        {66, 205, 255, 255}, {255, 190, 62, 255}, {190, 105, 255, 255}, {65, 230, 164, 255}
    };
    return colors[(int)branch];
}

static void DrawLabMenuPanel(Rectangle panel, Color accent) {
    DrawRectangleRounded({panel.x, panel.y + 6.0f, panel.width, panel.height}, 0.08f, 18, {0, 0, 0, 92});
    DrawRectangleRounded(panel, 0.08f, 18, {accent.r, accent.g, accent.b, 80});
    DrawRectangleRounded({panel.x + 1.0f, panel.y + 1.0f, panel.width - 2.0f, panel.height - 2.0f},
                         0.08f, 18, {15, 23, 32, 238});
    DrawLineEx({panel.x + 18.0f, panel.y + 1.0f}, {panel.x + panel.width - 18.0f, panel.y + 1.0f},
               1.0f, {255, 255, 255, 24});
}

static void DrawLabGrid(Rectangle area, Color accent) {
    DrawRectangleRounded(area, 0.04f, 18, {9, 14, 21, 228});
    DrawRectangleRoundedLinesEx(area, 0.04f, 18, 1.0f, {accent.r, accent.g, accent.b, 78});

    for (int x = (int)area.x; x <= (int)(area.x + area.width); x += 34) {
        DrawLine(x, (int)area.y, x, (int)(area.y + area.height), {255, 255, 255, 10});
    }
    for (int y = (int)area.y; y <= (int)(area.y + area.height); y += 34) {
        DrawLine((int)area.x, y, (int)(area.x + area.width), y, {255, 255, 255, 10});
    }
    DrawLineEx({area.x + 18.0f, area.y + 24.0f}, {area.x + area.width - 18.0f, area.y + 24.0f}, 1.0f, {255, 255, 255, 18});
    DrawLineEx({area.x + 18.0f, area.y + area.height - 26.0f}, {area.x + area.width - 18.0f, area.y + area.height - 26.0f}, 1.0f, {0, 0, 0, 55});
}

static void DrawTechHex(Vector2 center, float radius, Color fill, Color outline, bool selected, bool available) {
    if (selected) {
        DrawPoly(center, 6, radius + 12.0f, -90.0f, {outline.r, outline.g, outline.b, 28});
        DrawPolyLinesEx(center, 6, radius + 7.0f, -90.0f, 2.0f, {outline.r, outline.g, outline.b, 110});
    }
    if (available) {
        DrawPoly(center, 6, radius + 4.0f, -90.0f, {outline.r, outline.g, outline.b, 30});
    }
    DrawPoly(center, 6, radius, -90.0f, fill);
    DrawPolyLinesEx(center, 6, radius, -90.0f, selected ? 3.0f : 1.5f, outline);
}

static void DrawLabBranchHeader(Rectangle area, const char* name, int level, Color accent, bool active) {
    Rectangle chip = {area.x + 18.0f, area.y + 16.0f, area.width - 36.0f, 42.0f};
    DrawRectangleRounded(chip, 0.16f, 10, active ? Color{accent.r, accent.g, accent.b, 190} : Color{20, 28, 38, 224});
    DrawRectangleRoundedLinesEx(chip, 0.16f, 10, 1.0f, {accent.r, accent.g, accent.b, (unsigned char)(active ? 200 : 70)});
    DrawTextUI(name, {chip.x + 14.0f, chip.y + 10.0f}, 12.5f, active ? Color{4, 20, 28, 255} : Color{220, 230, 238, 255});
    char progress[16];
    std::snprintf(progress, sizeof(progress), "%d/3", level);
    DrawTextUI(progress, {chip.x + chip.width - 42.0f, chip.y + 10.0f}, 12.5f, active ? Color{4, 20, 28, 255} : accent);
}

static void DrawLabNodeLabel(Vector2 center, const char* label, Color color) {
    DrawCenteredTextUI(label, {center.x - 58.0f, center.y + 64.0f, 116.0f, 22.0f}, 11.0f, color);
}

static void DrawLabUpgradeNode(const Rectangle& menu, int index) {
    const LabUpgradeDefinition& upgrade = LAB_UPGRADES[index];
    int tier = index % LAB_UPGRADES_PER_BRANCH;
    Vector2 center = GetLabUpgradeNodeCenter(menu, upgrade.branch, tier);
    bool bought = gGameState.gameplay.lab.upgrades[index];
    bool available = IsUpgradeAvailable(gGameState.gameplay.lab, index);
    bool selected = gGameState.gameplay.lab.selectedUpgrade == index;
    Color accent = GetLabBranchColor(upgrade.branch);
    Color fill = bought ? Color{accent.r, accent.g, accent.b, 220}
                        : (available ? Color{18, 72, 98, 245} : Color{17, 28, 40, 245});
    Color outline = bought || available ? accent : Color{72, 90, 105, 150};

    DrawTechHex(center, selected ? 61.0f : 54.0f, fill, outline, selected, available);
    DrawCenteredTextUI(bought ? "OK" : TextFormat("%d", tier + 1),
                       {center.x - 32.0f, center.y - 18.0f, 64.0f, 36.0f}, 20.0f, WHITE);
    DrawLabNodeLabel(center, upgrade.name, bought || available ? Color{230, 242, 248, 255} : Color{112, 130, 145, 255});
}

static void DrawLabMenu() {
    GameplayState& state = gGameState.gameplay;
    Rectangle menu = GetLabMenuRect();
    static const char* branchNames[LAB_BRANCH_COUNT] = {u8"Исследования", u8"Финансы", u8"Общество", u8"Кооперация"};
    static const char* branchNotes[LAB_BRANCH_COUNT] = {u8"скорость лечения", u8"ежедневный доход", u8"доверие", u8"международная помощь"};
    Color accent = GetLabBranchColor(state.lab.activeBranch);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 4, 9, 206});
    DrawRectangleRounded({menu.x + 10.0f, menu.y + 12.0f, menu.width, menu.height}, 0.035f, 16, {0, 0, 0, 125});
    DrawRectangleRounded(menu, 0.035f, 16, {7, 18, 29, 252});
    DrawRectangleRoundedLinesEx(menu, 0.035f, 16, 1.5f, {70, 205, 240, 130});

    Rectangle header = GetLabHeaderRect(menu);
    DrawTextUI(u8"Лаборатория", {header.x, header.y}, 15.0f, {130, 205, 230, 255});
    DrawTextUI(GetLabDisplayName(), {header.x, header.y + 24.0f}, 30.0f, WHITE);
    DrawTextUI(u8"Покупайте улучшения веток и ускоряйте разработку лекарства", {header.x + 330.0f, header.y + 34.0f}, 15.0f, {175, 188, 200, 255});

    Rectangle close = GetLabMenuCloseRect(menu);
    DrawRectangleRounded(close, 0.18f, 8, {12, 35, 50, 255});
    DrawRectangleRoundedLinesEx(close, 0.18f, 8, 1.0f, {70, 205, 240, 130});
    DrawCenteredTextUI("X", close, 18.0f, WHITE);

    for (int branch = 0; branch < LAB_BRANCH_COUNT; ++branch) {
        Rectangle tab = GetLabBranchTabRect(menu, branch);
        bool active = state.lab.activeBranch == (LabUpgradeBranch)branch;
        Color tabAccent = GetLabBranchColor((LabUpgradeBranch)branch);
        Color fill = active ? Color{tabAccent.r, tabAccent.g, tabAccent.b, 210} : Color{13, 31, 44, 245};
        DrawRectangleRounded(tab, 0.20f, 10, fill);
        DrawRectangleRoundedLinesEx(tab, 0.20f, 10, 1.0f, {tabAccent.r, tabAccent.g, tabAccent.b, (unsigned char)(active ? 210 : 70)});
        DrawCenteredTextUI(branchNames[branch], tab, 13.0f, active ? Color{4, 20, 28, 255} : Color{205, 216, 226, 255});
    }

    Rectangle statusArea = GetLabStatusRect(menu);
    Rectangle techArea = GetLabTechRect(menu);
    Rectangle infoArea = GetLabInfoRect(menu);
    DrawLabMenuPanel(statusArea, {84, 214, 145, 255});
    DrawLabMenuPanel(techArea, accent);
    DrawLabMenuPanel(infoArea, accent);

    DrawTextUI(u8"Состояние", {statusArea.x + 18.0f, statusArea.y + 16.0f}, 18.0f, {230, 242, 248, 255});
    DrawTextUI(u8"ресурсы и темп лаборатории", {statusArea.x + 18.0f, statusArea.y + 42.0f}, 13.0f, {150, 165, 185, 255});

    char resources[64];
    std::snprintf(resources, sizeof(resources), "$%d", state.lab.resources);
    DrawTextUI(u8"Деньги", {statusArea.x + 18.0f, statusArea.y + 86.0f}, 13.0f, {145, 156, 170, 255});
    DrawTextUI(resources, {statusArea.x + 18.0f, statusArea.y + 108.0f}, 28.0f, {242, 210, 88, 255});

    char trust[64];
    std::snprintf(trust, sizeof(trust), "%d/100", state.lab.trust);
    DrawTextUI(u8"Доверие", {statusArea.x + 18.0f, statusArea.y + 166.0f}, 13.0f, {145, 156, 170, 255});
    DrawTextUI(trust, {statusArea.x + 18.0f, statusArea.y + 188.0f}, 24.0f, {165, 220, 245, 255});
    DrawMiniBar({statusArea.x + 18.0f, statusArea.y + 224.0f, statusArea.width - 36.0f, 8.0f}, state.lab.trust / 100.0f, accent);

    char income[64];
    std::snprintf(income, sizeof(income), "+%d / день", GetLabDailyIncome(state));
    DrawTextUI(u8"Доход", {statusArea.x + 18.0f, statusArea.y + 264.0f}, 13.0f, {145, 156, 170, 255});
    DrawTextUI(income, {statusArea.x + 18.0f, statusArea.y + 286.0f}, 22.0f, WHITE);

    DrawTextUI(u8"Этап", {statusArea.x + 18.0f, statusArea.y + 340.0f}, 13.0f, {145, 156, 170, 255});
    DrawTextUI(LabStageName(state.lab.stage), {statusArea.x + 18.0f, statusArea.y + 362.0f}, 23.0f, accent);
    DrawMiniBar({statusArea.x + 18.0f, statusArea.y + 400.0f, statusArea.width - 36.0f, 8.0f}, state.lab.overallProgress, accent);

    DrawTextUI(u8"Подсказка", {statusArea.x + 18.0f, statusArea.y + 444.0f}, 13.0f, {145, 156, 170, 255});
    DrawWrappedTextUI(u8"Выберите ветку сверху, затем улучшение в списке. Кнопка покупки появится справа.",
                      {statusArea.x + 18.0f, statusArea.y + 466.0f, statusArea.width - 36.0f, 46.0f},
                      13.5f, {205, 216, 228, 255}, 4.0f);

    DrawTextUI(branchNames[(int)state.lab.activeBranch], {techArea.x + 22.0f, techArea.y + 18.0f}, 22.0f, WHITE);
    DrawTextUI(branchNotes[(int)state.lab.activeBranch], {techArea.x + 22.0f, techArea.y + 46.0f}, 13.5f, {160, 174, 188, 255});

    for (int tier = 0; tier < LAB_UPGRADES_PER_BRANCH; ++tier) {
        int index = GetUpgradeIndex(state.lab.activeBranch, tier);
        const LabUpgradeDefinition& item = LAB_UPGRADES[index];
        Rectangle row = GetLabUpgradeListItemRect(menu, tier);
        bool bought = state.lab.upgrades[index];
        bool available = IsUpgradeAvailable(state.lab, index);
        bool selected = state.lab.selectedUpgrade == index;
        Color rowAccent = GetLabBranchColor(item.branch);
        Color fill = selected ? Color{22, 48, 64, 248} : Color{12, 25, 36, 235};
        if (bought) fill = {17, 54, 42, 244};

        DrawRectangleRounded(row, 0.10f, 12, fill);
        DrawRectangleRoundedLinesEx(row, 0.10f, 12, selected ? 2.0f : 1.0f,
                                    bought || available || selected ? rowAccent : Color{75, 91, 105, 120});
        DrawCircleV({row.x + 34.0f, row.y + row.height * 0.5f}, 21.0f,
                    bought ? Color{rowAccent.r, rowAccent.g, rowAccent.b, 230} : Color{20, 37, 50, 255});
        DrawCenteredTextUI(bought ? "OK" : TextFormat("%d", tier + 1),
                           {row.x + 12.0f, row.y + row.height * 0.5f - 16.0f, 44.0f, 32.0f},
                           15.0f, bought ? Color{4, 20, 28, 255} : Color{220, 232, 240, 255});
        DrawTextUI(item.name, {row.x + 72.0f, row.y + 17.0f}, 18.0f,
                   bought || available ? WHITE : Color{150, 160, 172, 255});
        DrawWrappedTextUI(item.effect, {row.x + 72.0f, row.y + 46.0f, row.width - 150.0f, 30.0f},
                          13.0f, bought || available ? Color{174, 222, 194, 255} : Color{118, 132, 144, 255}, 3.0f);

        const char* stateText = bought ? u8"куплено" : (available ? u8"доступно" : u8"закрыто");
        DrawTextUI(stateText, {row.x + row.width - 82.0f, row.y + 18.0f}, 12.5f,
                   bought ? Color{105, 225, 165, 255} : (available ? Color{242, 210, 88, 255} : Color{145, 154, 164, 255}));
        char cost[32];
        std::snprintf(cost, sizeof(cost), "$%d", item.cost);
        DrawTextUI(cost, {row.x + row.width - 82.0f, row.y + 48.0f}, 17.0f,
                   available ? Color{242, 210, 88, 255} : Color{150, 160, 172, 255});
    }

    int selected = std::clamp(state.lab.selectedUpgrade, 0, LAB_UPGRADE_COUNT - 1);
    const LabUpgradeDefinition& upgrade = LAB_UPGRADES[selected];
    Color selectedAccent = GetLabBranchColor(upgrade.branch);
    bool bought = state.lab.upgrades[selected];
    bool available = IsUpgradeAvailable(state.lab, selected);

    DrawTextUI(u8"Детали", {infoArea.x + 20.0f, infoArea.y + 18.0f}, 18.0f, {210, 230, 240, 255});
    DrawTextUI(upgrade.name, {infoArea.x + 20.0f, infoArea.y + 52.0f}, 23.0f, WHITE);
    DrawTextUI(branchNames[(int)upgrade.branch], {infoArea.x + 20.0f, infoArea.y + 88.0f}, 13.0f, selectedAccent);
    DrawTextUI(bought ? u8"куплено" : (available ? u8"можно купить" : u8"недоступно"),
               {infoArea.x + 150.0f, infoArea.y + 88.0f}, 13.0f,
               bought ? Color{105, 225, 165, 255} : (available ? Color{242, 210, 88, 255} : Color{150, 160, 172, 255}));

    DrawWrappedTextUI(upgrade.description, {infoArea.x + 20.0f, infoArea.y + 128.0f, infoArea.width - 40.0f, 94.0f},
                      15.0f, {205, 216, 228, 255}, 5.0f);

    DrawRectangleRounded({infoArea.x + 20.0f, infoArea.y + 244.0f, infoArea.width - 40.0f, 72.0f}, 0.10f, 8, {18, 34, 44, 220});
    DrawTextUI(u8"Эффект", {infoArea.x + 36.0f, infoArea.y + 256.0f}, 12.0f, {145, 156, 170, 255});
    DrawWrappedTextUI(upgrade.effect, {infoArea.x + 36.0f, infoArea.y + 278.0f, infoArea.width - 72.0f, 28.0f},
                      15.0f, {105, 225, 165, 255}, 4.0f);

    char req[128];
    std::snprintf(req, sizeof(req), u8"Стоимость: $%d\nДоверие: %d", upgrade.cost, upgrade.requiredTrust);
    DrawRectangleRounded({infoArea.x + 20.0f, infoArea.y + 332.0f, infoArea.width - 40.0f, 82.0f}, 0.10f, 8, {17, 24, 34, 226});
    DrawTextUI(u8"Требования", {infoArea.x + 36.0f, infoArea.y + 346.0f}, 12.0f, {145, 156, 170, 255});
    DrawWrappedTextUI(req, {infoArea.x + 36.0f, infoArea.y + 368.0f, infoArea.width - 72.0f, 30.0f},
                      15.0f, {215, 225, 235, 255}, 4.0f);

    Rectangle buy = GetLabMenuBuyRect(menu);
    DrawRectangleRounded(buy, 0.16f, 10, available ? Color{selectedAccent.r, selectedAccent.g, selectedAccent.b, 220} : Color{28, 42, 52, 255});
    DrawRectangleRoundedLinesEx(buy, 0.16f, 10, 1.0f, available ? selectedAccent : Color{90, 110, 125, 100});
    const char* buyLabel = bought ? u8"УЖЕ КУПЛЕНО" : (available ? u8"КУПИТЬ УЛУЧШЕНИЕ" : u8"ЗАКРЫТО");
    DrawCenteredTextUI(buyLabel, buy, 14.0f, available ? Color{4, 20, 28, 255} : Color{190, 205, 215, 255});
}

static void DrawLabPlacementOverlay() {
    GameplayState& state = gGameState.gameplay;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 72});

    Rectangle card = GetLabPlacementCardRect();
    DrawPanel(card, {70, 195, 255, 255}, true);
    DrawTextUI(u8"РАЗВЕРНИТЕ ГЛОБАЛЬНУЮ ЛАБОРАТОРИЮ", {card.x + 28.0f, card.y + 20.0f}, 23.0f, WHITE);
    DrawWrappedTextUI(
        u8"Выберите страну, где будет расположен первый исследовательский штаб. Отсюда начнётся сбор образцов, координация учёных и будущая вакцинационная программа.",
        {card.x + 28.0f, card.y + 55.0f, card.width - 56.0f, 48.0f},
        16.0f, {205, 216, 230, 255}, 4.0f
    );

    const char* target = state.hoveredRegion >= 0 ? state.regions[state.hoveredRegion].name : u8"наведите курсор на страну";
    char hint[192];
    std::snprintf(hint, sizeof(hint), u8"ЛКМ: основать лабораторию  •  сейчас: %s", target);
    DrawTextUI(hint, {card.x + 28.0f, card.y + 118.0f}, 17.0f, {105, 205, 255, 255});
}

static void DrawRegionInfoPanel() {
    GameplayState& state = gGameState.gameplay;
    int selected = state.selectedRegion;
    if (selected < 0 || selected >= (int)state.regions.size()) return;

    const Region& region = state.regions[selected];
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle panel = {sw * 0.5f - 300.0f, sh - 126.0f, 600.0f, 106.0f};
    DrawPanel(panel, {70, 195, 255, 255}, true);

    DrawCircleV({panel.x + 48.0f, panel.y + 52.0f}, 28.0f, region.overlayColor);
    DrawCircleLines((int)(panel.x + 48.0f), (int)(panel.y + 52.0f), 29.0f, {140, 210, 240, 120});
    DrawTextUI(region.name, {panel.x + 92.0f, panel.y + 12.0f}, 24.0f, WHITE);

    std::string infected = FormatNumber(region.infected);
    std::string dead = FormatNumber(region.dead);

    DrawMetric(u8"Заражено", infected.c_str(), {panel.x + 92.0f, panel.y + 46.0f}, {255, 205, 88, 255});
    DrawMetric(u8"Погибло", dead.c_str(), {panel.x + 210.0f, panel.y + 46.0f}, {236, 92, 92, 255});

    int airports = CountTransportHubs(region, HUB_AIRPORT);
    int ports = CountTransportHubs(region, HUB_PORT);
    int closed = 0;
    for (const TransportHub& hub : region.transportHubs) {
        if (hub.closed) ++closed;
    }
    char hubsLine[96];
    std::snprintf(hubsLine, sizeof(hubsLine), "AIR %d   PORT %d   CLOSED %d", airports, ports, closed);
    DrawTextUI(hubsLine, {panel.x + 330.0f, panel.y + 18.0f}, 14.0f, {180, 195, 215, 255});
    DrawTextUI(u8"Лечение", {panel.x + 330.0f, panel.y + 52.0f}, 13.0f, {145, 158, 178, 255});
    DrawMiniBar({panel.x + 330.0f, panel.y + 75.0f, 230.0f, 7.0f}, region.cureProgress, {70, 195, 255, 255});
}

// ═══════════════════════════════════════════════════════════════
// DRAW — BOTTOM RIGHT: Global Stats
// ═══════════════════════════════════════════════════════════════

static void DrawGlobalStats() {
    const GameplayState& state = gGameState.gameplay;
    float cureProgress = std::clamp(state.lab.overallProgress, 0.0f, 1.0f);
    float percent = cureProgress * 100.0f;
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    Rectangle panel = {sw - 242.0f, sh - 126.0f, 226.0f, 106.0f};
    Color progressColor = percent < 35.0f ? Color{70, 195, 255, 255} : (percent < 75.0f ? Color{105, 225, 165, 255} : Color{242, 210, 88, 255});
    DrawPanel(panel, progressColor);
    DrawTextUI("WORLD", {panel.x + 18.0f, panel.y + 12.0f}, 14.0f, {165, 178, 198, 255});
    DrawTextUI(u8"разработка лекарства", {panel.x + 18.0f, panel.y + 29.0f}, 13.5f, {150, 165, 185, 255});

    char value[32];
    std::snprintf(value, sizeof(value), "%.1f%%", percent);
    DrawTextUI(value, {panel.x + 18.0f, panel.y + 50.0f}, 30.0f, progressColor);
    DrawMiniBar({panel.x + 18.0f, panel.y + 90.0f, panel.width - 36.0f, 5.0f}, cureProgress, progressColor);
}

// ═══════════════════════════════════════════════════════════════
// DRAW — STATUS OVERLAY (win/lose)
// ═══════════════════════════════════════════════════════════════

static void DrawStatusOverlay() {
    GameplayState& state = gGameState.gameplay;
    if (!state.gameWon && !state.gameLost) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 140});
    const char* title = state.gameWon ? u8"МИР СПАСЁН!" : u8"ПАНДЕМИЯ ПОБЕДИЛА";
    const char* sub = u8"ESC — вернуться в меню";
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle box = {sw * 0.5f - 280.0f, sh * 0.5f - 70.0f, 560.0f, 140.0f};
    DrawRectangleRounded(box, 0.08f, 12, {14, 20, 32, 240});
    DrawCenteredTextUI(title, {box.x, box.y + 20.0f, box.width, 60.0f}, 38.0f, WHITE);
    DrawCenteredTextUI(sub, {box.x, box.y + 85.0f, box.width, 30.0f}, 20.0f, {150, 180, 220, 255});
}

static void DrawModalAlert() {
    GameplayState& state = gGameState.gameplay;
    if (!state.modalAlert.active) return;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float t = EaseOutCubic(state.modalAlert.animation);
    unsigned char overlayAlpha = (unsigned char)(150.0f * t);
    DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, overlayAlpha});

    Rectangle base = {sw * 0.5f - 270.0f, sh * 0.5f - 110.0f, 540.0f, 220.0f};
    float scale = 0.965f + 0.035f * t;
    Rectangle box = {
        base.x + base.width * (1.0f - scale) * 0.5f,
        base.y + base.height * (1.0f - scale) * 0.5f - 10.0f * (1.0f - t),
        base.width * scale,
        base.height * scale
    };

    unsigned char panelAlpha = (unsigned char)(244.0f * t);
    unsigned char lineAlpha = (unsigned char)(120.0f * t);
    Color accent = state.modalAlert.accent;
    accent.a = (unsigned char)(255.0f * t);

    DrawRectangleRounded({box.x + 4.0f, box.y + 8.0f, box.width, box.height}, 0.055f, 14, {0, 0, 0, (unsigned char)(95.0f * t)});
    DrawRectangleRounded(box, 0.055f, 14, {9, 14, 24, panelAlpha});
    DrawRectangleRoundedLinesEx(box, 0.055f, 14, 1.0f, {110, 130, 155, lineAlpha});

    Rectangle imageBox = {box.x + 22.0f, box.y + 62.0f, 104.0f, 104.0f};
    DrawRectangleRounded(imageBox, 0.08f, 10, {16, 24, 36, (unsigned char)(230.0f * t)});
    DrawRectangleRoundedLinesEx(imageBox, 0.08f, 10, 1.0f, {accent.r, accent.g, accent.b, (unsigned char)(130.0f * t)});
    DrawCircleV({imageBox.x + imageBox.width * 0.5f, imageBox.y + imageBox.height * 0.5f}, 23.0f, {accent.r, accent.g, accent.b, (unsigned char)(55.0f * t)});
    DrawCircleLines((int)(imageBox.x + imageBox.width * 0.5f), (int)(imageBox.y + imageBox.height * 0.5f), 23.0f, {accent.r, accent.g, accent.b, (unsigned char)(160.0f * t)});

    Rectangle textBox = {box.x + 148.0f, box.y + 20.0f, box.width - 174.0f, box.height - 36.0f};
    DrawTextUI("GLOBAL EVENT", {textBox.x, textBox.y}, 14.0f, {150, 205, 230, (unsigned char)(255.0f * t)});
    DrawWrappedTextUI(state.modalAlert.title, {textBox.x, textBox.y + 27.0f, textBox.width, 60.0f}, 24.0f, {255, 255, 255, (unsigned char)(255.0f * t)}, 3.0f);
    DrawLineEx({textBox.x, textBox.y + 84.0f}, {textBox.x + textBox.width, textBox.y + 84.0f}, 1.0f, {80, 100, 130, (unsigned char)(110.0f * t)});

    DrawWrappedTextUI(state.modalAlert.body, {textBox.x, textBox.y + 100.0f, textBox.width, 58.0f}, 17.0f, {222, 232, 242, (unsigned char)(255.0f * t)}, 5.0f);

    Rectangle button = {box.x + box.width - 158.0f, box.y + box.height - 48.0f, 132.0f, 32.0f};
    bool hovered = CheckCollisionPointRec(GetMousePosition(), button);
    DrawRectangleRounded(button, 0.18f, 8, hovered ? Color{48, 105, 148, (unsigned char)(245.0f * t)} : Color{28, 42, 62, (unsigned char)(240.0f * t)});
    DrawRectangleRoundedLinesEx(button, 0.18f, 8, 1.0f, hovered ? Color{135, 218, 250, (unsigned char)(170.0f * t)} : Color{110, 130, 155, (unsigned char)(100.0f * t)});
    DrawCenteredTextUI(u8"ПРОДОЛЖИТЬ", button, 14.5f, {255, 255, 255, (unsigned char)(255.0f * t)});
}

static void DrawPauseMenuButton(Rectangle button, const char* label, Color accent) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), button);
    Color fill = hovered ? Color{34, 78, 112, 248} : Color{22, 32, 48, 242};
    Color border = hovered ? Color{accent.r, accent.g, accent.b, 210} : Color{105, 128, 155, 105};
    DrawRectangleRounded(button, 0.16f, 10, border);
    DrawRectangleRounded({button.x + 1.0f, button.y + 1.0f, button.width - 2.0f, button.height - 2.0f}, 0.16f, 10, fill);
    DrawCenteredTextUI(label, button, 16.0f, WHITE);
}

static void DrawPauseMenu() {
    const GameplayState& state = gGameState.gameplay;
    if (!state.pauseMenuOpen) return;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle menu = GetPauseMenuRect();
    Color accent = {70, 195, 255, 255};

    DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 150});
    DrawRectangleRounded({menu.x + 8.0f, menu.y + 12.0f, menu.width, menu.height}, 0.045f, 16, {0, 0, 0, 115});
    DrawRectangleRounded(menu, 0.045f, 16, {9, 14, 24, 246});
    DrawRectangleRoundedLinesEx(menu, 0.045f, 16, 1.0f, {105, 130, 158, 130});

    if (gGameState.menu.logoLoaded && gGameState.menu.logoTexture.id != 0) {
        Rectangle src = {96.0f, 152.0f, 692.0f, 176.0f};
        Rectangle dst = {menu.x + 70.0f, menu.y + 38.0f, menu.width - 140.0f, 72.0f};
        DrawTexturePro(gGameState.menu.logoTexture, src, dst, {0, 0}, 0.0f, WHITE);
    } else {
        DrawCenteredTextUI("ANTIVIRUS", {menu.x, menu.y + 42.0f, menu.width, 60.0f}, 34.0f, WHITE);
    }

    DrawCenteredTextUI(u8"ПАУЗА", {menu.x, menu.y + 132.0f, menu.width, 30.0f}, 20.0f, {165, 210, 235, 255});
    DrawCenteredTextUI(u8"операция временно остановлена", {menu.x, menu.y + 162.0f, menu.width, 24.0f}, 13.5f, {145, 160, 180, 255});

    DrawPauseMenuButton(GetPauseMenuButtonRect(menu, 0), u8"ПРОДОЛЖИТЬ", accent);
    DrawPauseMenuButton(GetPauseMenuButtonRect(menu, 1), u8"НАЧАТЬ ЗАНОВО", {242, 210, 88, 255});
    DrawPauseMenuButton(GetPauseMenuButtonRect(menu, 2), u8"ГЛАВНОЕ МЕНЮ", {238, 105, 105, 255});
}

// ═══════════════════════════════════════════════════════════════
// DRAW — MAIN
// ═══════════════════════════════════════════════════════════════

void DrawGameplay() {
    ClearBackground({6, 9, 18, 255});
    Rectangle mapRect = GetMapRect(gGameState.gameplay);
    DrawMap();
    DrawVehicles(gGameState.gameplay.vehicles, gGameState.gameplay.planeIcon, gGameState.gameplay.planeIconLoaded, gGameState.gameplay.shipIcon, gGameState.gameplay.shipIconLoaded, mapRect);
    DrawRegions();
    DrawTopBar();
    DrawNotifications();
    DrawTransportDebugPanel();
    if (!gGameState.gameplay.labPlacementActive) {
        DrawLabPanel();
    }
    DrawRegionInfoPanel();
    DrawGlobalStats();
    if (gGameState.gameplay.labPlacementActive) {
        DrawLabPlacementOverlay();
    }
    DrawModalAlert();
    DrawStatusOverlay();
    if (gGameState.gameplay.lab.menuOpen) {
        DrawLabMenu();
    }
    if (gGameState.gameplay.patientCardGame.IsActive()) {
        gGameState.gameplay.patientCardGame.Draw();
    }
    if (gGameState.gameplay.pathogenSequencerGame.IsActive()) {
        gGameState.gameplay.pathogenSequencerGame.Draw(gGameState.font);
    }
    if (gGameState.gameplay.electronMicroscopeGame.IsActive()) {
        gGameState.gameplay.electronMicroscopeGame.Draw(gGameState.font);
    }
    if (gGameState.gameplay.serologyAssayGame.IsActive()) {
        gGameState.gameplay.serologyAssayGame.Draw(gGameState.font);
    }
    if (gGameState.gameplay.thermalChamberGame.IsActive()) {
        gGameState.gameplay.thermalChamberGame.Draw(gGameState.font);
    }
    DrawPauseMenu();
    if (gGameState.gameplay.showJournal) {
        gGameState.gameplay.journal.Draw(
            gGameState.font,
            ArePatientCardsUnlocked(gGameState.gameplay),
            IsPathogenSequencerUnlocked(gGameState.gameplay),
            IsElectronMicroscopeUnlocked(gGameState.gameplay),
            IsSerologyAssayUnlocked(gGameState.gameplay),
            IsThermalChamberUnlocked(gGameState.gameplay)
        );
    }
}
