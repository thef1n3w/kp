#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

// ============================================================================
// Difficulty profiles
// ============================================================================


static const DifficultyProfile profiles[DIFFICULTY_COUNT] = {
    {
        u8"НАДЕЖДА", u8"Мир действует как единое целое",
        u8"Лучшие условия для знакомства с управлением глобальной эпидемией.",
        {
            u8"Страны открыто делятся исследованиями",
            u8"Люди доверяют врачам и носят маски",
            u8"Контакты добровольно сокращаются",
            u8"Медицинские системы быстро реагируют"
        },
        0.62f, 1.65f, 0.72f, 0.55f, 0.00f, 0.00f, 1.00f, 1.00f, 0.92f, 0.95f, 0.90f, 22, 0.80f
    },
    {
        u8"КОНТРОЛЬ", u8"Серьёзный, но управляемый кризис",
        u8"Сбалансированный режим: решения имеют вес, а ошибки оставляют последствия.",
        {
            u8"Большинство стран готово сотрудничать",
            u8"Общество в основном соблюдает меры",
            u8"Фейки существуют, но не доминируют",
            u8"Лекарство разрабатывается обычными темпами"
        },
        0.85f, 1.20f, 0.90f, 0.82f, 0.00f, 0.00f, 1.00f, 1.00f, 0.72f, 0.72f, 0.68f, 16, 1.00f
    },
    {
        u8"КРИЗИС", u8"Разобщённый и уставший мир",
        u8"Вирус получает преимущество из-за политики, недоверия и неравного доступа к медицине.",
        {
            u8"Страны конкурируют за технологии",
            u8"Общество устало от ограничений",
            u8"Дезинформация снижает доверие к врачам",
            u8"Бедные регионы получают помощь с задержкой"
        },
        1.15f, 0.85f, 1.15f, 1.20f, 0.00f, 0.00f, 1.00f, 1.00f, 0.48f, 0.42f, 0.44f, 12, 1.20f
    },
    {
        u8"КОЛЛАПС", u8"Правда проигрывает шуму",
        u8"Режим для опытных игроков: человечество активно мешает собственному спасению.",
        {
            u8"Фальшивым новостям верят больше учёных",
            u8"Опасность вируса массово отрицают",
            u8"Ограничения вызывают протесты и саботаж",
            u8"Исследования засекречены и раздроблены"
        },
        1.70f, 0.50f, 1.75f, 1.70f, 0.06f, 0.04f, 1.55f, 5.50f, 0.20f, 0.18f, 0.16f, 8, 1.45f
    }
};

const DifficultyProfile& GetDifficultyProfile(Difficulty difficulty) {
    int index = std::clamp((int)difficulty, 0, (int)DIFFICULTY_COUNT - 1);
    return profiles[index];
}

// ============================================================================
// Difficulty selection
// ============================================================================


namespace {

int selectedDifficulty = DIFFICULTY_CONTROL;
float tileAnimation[DIFFICULTY_COUNT] = {};
float panelAnimation = 0.0f;

float ScreenScale() {
    return std::clamp(std::min((float)GetScreenWidth() / 1920.0f,
                               (float)GetScreenHeight() / 1080.0f), 0.80f, 1.12f);
}

void DrawBackground() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (gGameState.menu.backgroundVideo.ready && gGameState.menu.backgroundVideo.texture.id != 0) {
        float vw = (float)gGameState.menu.backgroundVideo.width;
        float vh = (float)gGameState.menu.backgroundVideo.height;
        float scale = std::max((float)sw / vw, (float)sh / vh);
        Rectangle src = {0.0f, 0.0f, vw, vh};
        Rectangle dst = {(sw - vw * scale) * 0.5f, (sh - vh * scale) * 0.5f, vw * scale, vh * scale};
        DrawTexturePro(gGameState.menu.backgroundVideo.texture, src, dst, {0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground({6, 9, 18, 255});
        DrawRectangleGradientV(0, 0, sw, sh, {10, 16, 34, 255}, {22, 10, 29, 255});
    }
}

void DrawWrapped(const char* text, Rectangle bounds, float size, Color color, float lineGap = 3.0f) {
    const char* cursor = text;
    float y = bounds.y;
    while (*cursor && y + size <= bounds.y + bounds.height) {
        const char* lineEnd = cursor;
        const char* lastSpace = nullptr;
        while (*lineEnd && *lineEnd != '\n') {
            if (*lineEnd == ' ') lastSpace = lineEnd;
            std::string candidate(cursor, lineEnd + 1);
            if (MeasureTextUI(candidate.c_str(), size).x > bounds.width) {
                lineEnd = lastSpace ? lastSpace : lineEnd;
                break;
            }
            ++lineEnd;
        }
        std::string line(cursor, lineEnd);
        DrawTextUI(line.c_str(), {bounds.x, y}, size, color);
        y += size + lineGap;
        cursor = lineEnd;
        while (*cursor == ' ' || *cursor == '\n') ++cursor;
    }
}

Color DifficultyColor(int index) {
    static const Color colors[DIFFICULTY_COUNT] = {
        {74, 205, 137, 255},
        {71, 171, 238, 255},
        {241, 164, 73, 255},
        {232, 82, 99, 255}
    };
    return colors[index];
}

const char* DifficultyNumber(int index) {
    static const char* values[DIFFICULTY_COUNT] = {"I", "II", "III", "IV"};
    return values[index];
}

Rectangle GetPanelRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float scale = ScreenScale();
    float width = std::min(600.0f * scale, sw * 0.40f);
    float height = std::min(sh - 64.0f * scale, 760.0f * scale);
    float x = sw - width * panelAnimation;
    float y = (sh - height) * 0.5f;
    return {x, y, width, height};
}

Rectangle GetHeaderRect(Rectangle panel) {
    return {panel.x, panel.y, panel.width, 66.0f * ScreenScale()};
}

Rectangle GetDifficultyRowRect(int index) {
    Rectangle panel = GetPanelRect();
    float scale = ScreenScale();
    float rowH = 62.0f * scale;
    float gap = 10.0f * scale;
    float x = panel.x + 20.0f * scale;
    float y = panel.y + 86.0f * scale + index * (rowH + gap);
    return {x, y, panel.width - 40.0f * scale, rowH};
}

Rectangle GetStartRect(Rectangle panel) {
    float scale = ScreenScale();
    return {panel.x + panel.width - 186.0f * scale, panel.y + panel.height - 52.0f * scale,
            166.0f * scale, 38.0f * scale};
}

void DrawMetric(Rectangle rect, const char* label, float value, Color accent) {
    value = std::clamp(value, 0.0f, 1.0f);
    float scale = ScreenScale();
    DrawTextUI(label, {rect.x, rect.y}, 13.0f * scale, {171, 180, 194, 255});
    Rectangle track = {rect.x, rect.y + 20.0f * scale, rect.width, 5.0f * scale};
    DrawRectangleRounded(track, 1.0f, 8, {255, 255, 255, 20});
    track.width *= value;
    DrawRectangleRounded(track, 1.0f, 8, accent);
}

void DrawDifficultyRow(int index, Rectangle rect, bool selected, bool hovered) {
    const DifficultyProfile& profile = GetDifficultyProfile((Difficulty)index);
    Color accent = DifficultyColor(index);
    float scale = ScreenScale();
    float anim = tileAnimation[index];

    Rectangle body = rect;
    body.x += anim * 3.0f * scale;
    body.y -= anim * 1.5f * scale;

    Color fill = selected ? Color{255, 255, 255, 28}
                          : hovered ? Color{255, 255, 255, 16}
                                    : Color{255, 255, 255, 8};
    DrawRectangleRounded(body, 0.14f, 12, fill);
    DrawRectangleRoundedLinesEx(body, 0.14f, 12, 1.0f, {255, 255, 255, 18});
    if (selected) {
        DrawRectangleRounded({body.x + 4.0f * scale, body.y + 8.0f * scale, 4.0f * scale, body.height - 16.0f * scale},
                             1.0f, 8, accent);
    }

    DrawTextUI(DifficultyNumber(index), {body.x + 16.0f * scale, body.y + 16.0f * scale},
               14.0f * scale, selected ? accent : Color{120, 130, 143, 255});
    DrawTextUI(profile.name, {body.x + 52.0f * scale, body.y + 12.0f * scale},
               17.0f * scale, selected || hovered ? WHITE : Color{216, 221, 228, 255});
    DrawTextUI(profile.subtitle, {body.x + 52.0f * scale, body.y + 34.0f * scale},
               12.0f * scale, {141, 149, 160, 255});
}

void DrawDetailBlock(Rectangle rect, const char* title, const char* body, Color accent) {
    float scale = ScreenScale();
    DrawTextUI(title, {rect.x, rect.y}, 12.0f * scale, {153, 162, 174, 255});
    DrawTextUI(body, {rect.x, rect.y + 18.0f * scale}, 13.0f * scale, accent);
}

void DrawPanel() {
    Rectangle panel = GetPanelRect();
    float scale = ScreenScale();
    Color accent = DifficultyColor(selectedDifficulty);
    const DifficultyProfile& profile = GetDifficultyProfile((Difficulty)selectedDifficulty);

    DrawRectangleRounded(panel, 0.08f, 10, {18, 22, 30, 208});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 10, 1.0f, {255, 255, 255, 20});

    Rectangle header = GetHeaderRect(panel);
    DrawTextUI(u8"Сложность", {header.x + 20.0f * scale, header.y + 18.0f * scale},
               15.0f * scale, {210, 216, 223, 255});
    DrawTextUI(u8"Выбор режима", {header.x + 20.0f * scale, header.y + 38.0f * scale},
               11.5f * scale, {141, 149, 160, 255});

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < DIFFICULTY_COUNT; ++i) {
        Rectangle row = GetDifficultyRowRect(i);
        bool hovered = CheckCollisionPointRec(mouse, row);
        bool selected = selectedDifficulty == i;
        DrawDifficultyRow(i, row, selected, hovered);
    }

    Rectangle content = {panel.x + 20.0f * scale, panel.y + 374.0f * scale, panel.width - 40.0f * scale, 160.0f * scale};
    DrawTextUI(profile.name, {content.x, content.y}, 23.0f * scale, WHITE);
    DrawTextUI(profile.subtitle, {content.x, content.y + 27.0f * scale}, 13.5f * scale, accent);
    DrawWrapped(profile.description, {content.x, content.y + 48.0f * scale, content.width, 40.0f * scale},
                13.0f * scale, {215, 221, 229, 255}, 2.0f * scale);

    DrawDetailBlock({content.x, content.y + 75.0f * scale, content.width * 0.48f, 32.0f * scale},
                    u8"Старт", "ресурсы и темп", accent);
    DrawDetailBlock({content.x + content.width * 0.52f, content.y + 75.0f * scale, content.width * 0.48f, 32.0f * scale},
                    u8"Мир", "доверие и давление", accent);

    float metricY = panel.y + panel.height - 100.0f * scale;
    float metricWidth = (panel.width - 60.0f * scale) / 3.0f;
    float safety = std::clamp(1.35f - profile.spreadMultiplier, 0.0f, 1.0f);
    float travelSafety = std::clamp(1.35f - profile.transportTransmissionMultiplier, 0.0f, 1.0f);
    DrawMetric({panel.x + 20.0f * scale, metricY, metricWidth, 32.0f * scale},
               u8"Доверие", profile.publicCompliance, accent);
    DrawMetric({panel.x + 20.0f * scale + metricWidth + 10.0f * scale, metricY, metricWidth, 32.0f * scale},
               u8"Исследования", std::clamp(profile.cureMultiplier / 1.65f, 0.0f, 1.0f), accent);
    DrawMetric({panel.x + 20.0f * scale + (metricWidth + 10.0f * scale) * 2.0f, metricY, metricWidth, 32.0f * scale},
               u8"Контроль", (safety + travelSafety) * 0.5f, accent);

    char resources[64];
    std::snprintf(resources, sizeof(resources), u8"Старт: %d", profile.startingResources);
    DrawTextUI(resources, {panel.x + 20.0f * scale, panel.y + panel.height - 18.0f * scale},
               12.5f * scale, {157, 166, 178, 255});
}

void StartSelectedDifficulty() {
    gGameState.gameplay.difficulty = (Difficulty)selectedDifficulty;
    ResetGameplaySession();
    gGameState.currentScreen = SCREEN_GAMEPLAY;
}

} // namespace

void PrepareDifficultyScreen() {
    panelAnimation = 0.0f;
}

void UpdateDifficultyScreen(float delta) {
    UpdateMenuBackground(delta);
    panelAnimation += (1.0f - panelAnimation) * std::min(1.0f, delta * 8.0f);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < DIFFICULTY_COUNT; ++i) {
        Rectangle row = GetDifficultyRowRect(i);
        bool hovered = CheckCollisionPointRec(mouse, row);
        float target = hovered || selectedDifficulty == i ? 1.0f : 0.0f;
        tileAnimation[i] += (target - tileAnimation[i]) * std::min(1.0f, delta * 12.0f);
        if (hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) selectedDifficulty = i;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, GetStartRect(GetPanelRect()))) {
        StartSelectedDifficulty();
        return;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedDifficulty = std::max(0, selectedDifficulty - 1);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedDifficulty = std::min((int)DIFFICULTY_COUNT - 1, selectedDifficulty + 1);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) StartSelectedDifficulty();
    if (IsKeyPressed(KEY_ESCAPE)) gGameState.currentScreen = SCREEN_MENU;
}

void DrawDifficultyScreen() {
    DrawBackground();

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle panel = GetPanelRect();
    DrawRectangle(0, 0, (int)sw, (int)sh, {5, 7, 13, 92});

    DrawPanel();

    Rectangle start = GetStartRect(panel);
    Color accent = DifficultyColor(selectedDifficulty);
    Color fill = LerpColor({62, 120, 170, 255}, {84, 150, 210, 255}, panelAnimation);
    fill = LerpColor(fill, accent, 0.16f);
    DrawRectangleRounded(start, 0.12f, 8, fill);
    DrawCenteredTextUI(u8"Играть", start, 15.0f * ScreenScale(), WHITE);
}

// ============================================================================
// Settings and persistence
// ============================================================================


namespace {

struct SettingsCategory {
    const char* title;
    const char* subtitle;
    bool enabled;
};

static const SettingsCategory CATEGORIES[] = {
    {u8"Общие", u8"Сводка и профиль", true},
    {u8"Лаборатория", u8"Имя и идентичность", true},
    {u8"Геймплей", u8"Темп новой партии", true},
    {u8"Внешний вид", u8"Скоро", false},
    {u8"Уведомления", u8"Скоро", false},
    {u8"Эксперимент", u8"Скоро", false},
};

static int selectedCategory = 0;
static float categoryHover[std::size(CATEGORIES)] = {};
static float settingsPanelAnimation = 0.0f;
static bool editingLabName = false;

static int ClampSpeed(int speed) {
    switch (speed) {
        case 1:
        case 2:
        case 4:
        case 8:
            return speed;
        default:
            return 1;
    }
}

static const char* GetNonEmptyLabName() {
    return gGameState.settings.labName[0] ? gGameState.settings.labName : "AEGIS LAB";
}

static std::string SettingsPath() {
    return ResourcePath("settings.json");
}

static std::string JsonEscape(const char* text) {
    std::string out;
    if (!text) return out;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        switch (*p) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default: out.push_back((char)*p); break;
        }
    }
    return out;
}

static std::string JsonUnescape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        char next = text[++i];
        switch (next) {
            case '\\': out.push_back('\\'); break;
            case '\"': out.push_back('\"'); break;
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            default: out.push_back(next); break;
        }
    }
    return out;
}

static void CopyLabName(const char* text) {
    std::memset(gGameState.settings.labName, 0, sizeof(gGameState.settings.labName));
    if (!text) return;
    std::strncpy(gGameState.settings.labName, text, sizeof(gGameState.settings.labName) - 1);
}

static bool AppendUtf8Codepoint(char* buffer, size_t capacity, int codepoint) {
    if (!buffer || capacity == 0) return false;
    size_t len = std::strlen(buffer);
    unsigned char bytes[4];
    size_t count = 0;
    if (codepoint <= 0x7F) {
        bytes[count++] = (unsigned char)codepoint;
    } else if (codepoint <= 0x7FF) {
        bytes[count++] = (unsigned char)(0xC0 | ((codepoint >> 6) & 0x1F));
        bytes[count++] = (unsigned char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        bytes[count++] = (unsigned char)(0xE0 | ((codepoint >> 12) & 0x0F));
        bytes[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        bytes[count++] = (unsigned char)(0xF0 | ((codepoint >> 18) & 0x07));
        bytes[count++] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | (codepoint & 0x3F));
    } else {
        return false;
    }

    if (len + count >= capacity) return false;
    for (size_t i = 0; i < count; ++i) buffer[len + i] = (char)bytes[i];
    buffer[len + count] = '\0';
    return true;
}

static void RemoveLastUtf8Codepoint(char* buffer) {
    if (!buffer) return;
    size_t len = std::strlen(buffer);
    if (len == 0) return;
    size_t start = len - 1;
    while (start > 0 && (((unsigned char)buffer[start] & 0xC0) == 0x80)) --start;
    buffer[start] = '\0';
}

static void DrawBackdrop() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    VideoPlayer& video = gGameState.menu.backgroundVideo;
    ClearBackground({5, 7, 13, 255});
    if (video.ready && video.texture.id != 0) {
        float coverScale = std::max(sw / (float)video.width, sh / (float)video.height);
        Rectangle dst = {(sw - (float)video.width * coverScale) * 0.5f, (sh - (float)video.height * coverScale) * 0.5f,
                         (float)video.width * coverScale, (float)video.height * coverScale};
        DrawTexturePro(video.texture, {0, 0, (float)video.width, (float)video.height}, dst, {0, 0}, 0.0f, WHITE);
    }
    DrawRectangle(0, 0, (int)sw, (int)sh, {5, 7, 13, 128});
}

static void DrawSettingsLogo() {
    if (!gGameState.menu.logoLoaded || gGameState.menu.logoTexture.id == 0) return;
    float scale = GetUIScale();
    float w = 292.0f * scale;
    float h = 74.0f * scale;
    Rectangle src = {96.0f, 152.0f, 692.0f, 176.0f};
    Rectangle dst = {(float)GetScreenWidth() * 0.5f - w * 0.5f, 24.0f * scale, w, h};
    DrawTexturePro(gGameState.menu.logoTexture, src, dst, {0, 0}, 0.0f, WHITE);
}

static Rectangle GetSettingsPanelRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float scale = GetUIScale();
    float width = std::min(sw * 0.82f, 1220.0f * scale);
    float height = std::min(sh * 0.78f, 780.0f * scale);
    return {sw * 0.5f - width * 0.5f, sh * 0.5f - height * 0.46f, width, height};
}

static Rectangle GetSidebarRect(Rectangle panel) {
    return {panel.x + 18.0f, panel.y + 74.0f, 256.0f, panel.height - 92.0f};
}

static Rectangle GetContentRect(Rectangle panel) {
    return {panel.x + 292.0f, panel.y + 74.0f, panel.width - 310.0f, panel.height - 92.0f};
}

static Rectangle GetCategoryRect(Rectangle sidebar, int index) {
    float scale = GetUIScale();
    float rowH = 62.0f * scale;
    float gap = 8.0f * scale;
    return {sidebar.x + 10.0f, sidebar.y + 10.0f + index * (rowH + gap), sidebar.width - 20.0f, rowH};
}

static Rectangle GetNameFieldRect(Rectangle content) {
    return {content.x + 28.0f, content.y + 176.0f, content.width - 56.0f, 58.0f};
}

static Rectangle GetResetButtonRect(Rectangle content) {
    Rectangle field = GetNameFieldRect(content);
    return {field.x + field.width - 128.0f, field.y + 10.0f, 110.0f, 38.0f};
}

static Rectangle GetSpeedButtonRect(Rectangle content, int index) {
    float scale = GetUIScale();
    float w = 96.0f * scale;
    float gap = 12.0f * scale;
    float x = content.x + 28.0f + index * (w + gap);
    return {x, content.y + 186.0f, w, 44.0f * scale};
}

static void DrawSettingsPanel(Rectangle rect) {
    DrawRectangleRounded({rect.x + 8.0f, rect.y + 10.0f, rect.width, rect.height}, 0.05f, 16, {0, 0, 0, 118});
    DrawRectangleRounded(rect, 0.05f, 16, {13, 18, 28, 244});
    DrawRectangleRoundedLinesEx(rect, 0.05f, 16, 1.0f, {84, 110, 145, 96});
}

static void DrawCategory(Rectangle rect, const SettingsCategory& category, bool selected, bool hovered) {
    float scale = GetUIScale();
    Color fill = selected ? Color{46, 58, 84, 235}
                          : hovered ? Color{30, 38, 52, 236}
                                    : Color{20, 26, 36, 224};
    Color border = selected ? Color{90, 190, 255, 148} : Color{255, 255, 255, 28};
    DrawRectangleRounded(rect, 0.18f, 12, border);
    DrawRectangleRounded({rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f}, 0.18f, 12, fill);
    DrawRectangleRounded({rect.x + 5.0f, rect.y + 9.0f, 4.0f, rect.height - 18.0f}, 0.8f, 8,
                         category.enabled ? Color{70, 190, 255, (unsigned char)(selected ? 240 : 180)}
                                           : Color{255, 255, 255, 34});
    DrawTextUI(category.title, {rect.x + 18.0f, rect.y + 12.0f}, 16.0f * scale,
               category.enabled ? (selected ? WHITE : Color{220, 228, 238, 255}) : Color{135, 142, 152, 255});
    DrawTextUI(category.subtitle, {rect.x + 18.0f, rect.y + 34.0f}, 12.0f * scale,
               category.enabled ? Color{140, 150, 166, 255} : Color{110, 116, 126, 255});
    if (!category.enabled) {
        DrawTextUI(u8"СКОРО", {rect.x + rect.width - 68.0f, rect.y + 20.0f}, 10.0f * scale, {118, 124, 136, 255});
    }
}

static void DrawSectionCard(Rectangle rect, const char* title, const char* subtitle, Color accent) {
    DrawRectangleRounded(rect, 0.10f, 16, {17, 22, 32, 230});
    DrawRectangleRoundedLinesEx(rect, 0.10f, 16, 1.0f, {accent.r, accent.g, accent.b, 76});
    DrawTextUI(title, {rect.x + 18.0f, rect.y + 14.0f}, 18.0f, WHITE);
    DrawTextUI(subtitle, {rect.x + 18.0f, rect.y + 40.0f}, 13.0f, {152, 160, 174, 255});
}

static void DrawPill(Rectangle rect, const char* label, bool active) {
    Color fill = active ? Color{90, 188, 255, 230} : Color{25, 30, 42, 236};
    Color border = active ? Color{170, 220, 255, 160} : Color{255, 255, 255, 22};
    DrawRectangleRounded(rect, 0.35f, 12, border);
    DrawRectangleRounded({rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f}, 0.35f, 12, fill);
    DrawCenteredTextUI(label, rect, 16.0f, active ? Color{8, 14, 22, 255} : Color{220, 228, 236, 255});
}

static void DrawSummaryCard(Rectangle rect) {
    DrawRectangleRounded(rect, 0.08f, 20, {18, 24, 34, 235});
    DrawRectangleRoundedLinesEx(rect, 0.08f, 20, 1.0f, {255, 255, 255, 20});
    DrawTextUI(u8"Профиль лаборатории", {rect.x + 20.0f, rect.y + 16.0f}, 14.0f, {150, 162, 180, 255});
    DrawTextUI(GetNonEmptyLabName(), {rect.x + 20.0f, rect.y + 38.0f}, 24.0f, WHITE);

    char speedText[32];
    std::snprintf(speedText, sizeof(speedText), "%dx", GetDefaultGameplaySpeed());
    DrawTextUI(u8"Стартовая скорость", {rect.x + 20.0f, rect.y + 82.0f}, 13.0f, {150, 162, 180, 255});
    DrawTextUI(speedText, {rect.x + 20.0f, rect.y + 104.0f}, 20.0f, {90, 190, 255, 255});
    DrawTextUI(u8"Изменения применяются к новым партиям и кнопке Play.", {rect.x + 20.0f, rect.y + 142.0f},
               12.5f, {170, 180, 192, 255});
}

static void DrawGeneralSection(Rectangle content) {
    Rectangle left = {content.x + 28.0f, content.y + 92.0f, content.width * 0.54f, 220.0f};
    Rectangle right = {content.x + content.width * 0.58f, content.y + 92.0f, content.width * 0.36f, 220.0f};
    DrawSummaryCard(left);
    DrawSectionCard(right, u8"Статус", u8"Косметика и ориентиры", {90, 190, 255, 255});
    DrawTextUI(u8"Лаборатория", {right.x + 18.0f, right.y + 80.0f}, 13.0f, {150, 160, 174, 255});
    DrawTextUI(GetNonEmptyLabName(), {right.x + 18.0f, right.y + 102.0f}, 21.0f, WHITE);
    DrawTextUI(u8"Темп", {right.x + 18.0f, right.y + 146.0f}, 13.0f, {150, 160, 174, 255});
    char speedText[16];
    std::snprintf(speedText, sizeof(speedText), "%dx", GetDefaultGameplaySpeed());
    DrawTextUI(speedText, {right.x + 18.0f, right.y + 168.0f}, 21.0f, {90, 190, 255, 255});
}

static void DrawLabSection(Rectangle content) {
    Rectangle card = {content.x + 28.0f, content.y + 92.0f, content.width - 56.0f, 260.0f};
    DrawSectionCard(card, u8"Лаборатория", u8"Имя показывается в HUD и всплывающих событиях", {90, 190, 255, 255});

    Rectangle field = GetNameFieldRect(content);
    DrawTextUI(u8"Название лаборатории", {field.x, field.y - 24.0f}, 13.0f, {145, 156, 170, 255});
    Color border = editingLabName ? Color{90, 190, 255, 185} : Color{255, 255, 255, 24};
    DrawRectangleRounded(field, 0.14f, 16, border);
    DrawRectangleRounded({field.x + 1.0f, field.y + 1.0f, field.width - 2.0f, field.height - 2.0f}, 0.14f, 16,
                         {19, 25, 36, 242});
    DrawTextUI(GetNonEmptyLabName(), {field.x + 18.0f, field.y + 16.0f}, 19.0f, WHITE);
    if (editingLabName && ((int)(GetTime() * 2.0) % 2 == 0)) {
        float caretX = field.x + 18.0f + MeasureTextUI(GetNonEmptyLabName(), 19.0f).x + 2.0f;
        DrawLine((int)caretX, (int)(field.y + 14.0f), (int)caretX, (int)(field.y + field.height - 14.0f), {90, 190, 255, 255});
    }

    Rectangle reset = GetResetButtonRect(content);
    DrawPill(reset, u8"Сброс", false);

    DrawTextUI(u8"Кликните по полю и введите новое имя. Enter подтверждает, Esc снимает фокус.", {card.x + 24.0f, card.y + 150.0f},
               13.0f, {171, 180, 194, 255});

    Rectangle accent = {card.x + 24.0f, card.y + 182.0f, card.width - 48.0f, 48.0f};
    DrawRectangleRounded(accent, 0.14f, 10, {28, 38, 54, 248});
    DrawRectangleRoundedLinesEx(accent, 0.14f, 10, 1.0f, {90, 190, 255, 64});
    DrawTextUI(u8"Название сразу применяется к панели лаборатории в геймплее.", {accent.x + 16.0f, accent.y + 15.0f},
               13.0f, {196, 208, 224, 255});

    Rectangle hint = {content.x + 28.0f, content.y + 368.0f, content.width - 56.0f, 100.0f};
    DrawSectionCard(hint, u8"Подсказка", u8"Параллельно можно оставить фантомные разделы как намёк на будущие настройки.", {70, 195, 255, 255});
}

static void DrawGameplaySection(Rectangle content) {
    Rectangle card = {content.x + 28.0f, content.y + 92.0f, content.width - 56.0f, 256.0f};
    DrawSectionCard(card, u8"Геймплей", u8"Стартовая скорость новой партии", {90, 190, 255, 255});
    DrawTextUI(u8"После старта игры кнопка Play и новая сессия начнутся с этого темпа.", {card.x + 18.0f, card.y + 68.0f},
               13.0f, {176, 184, 198, 255});

    const int choices[] = {1, 2, 4, 8};
    for (int i = 0; i < 4; ++i) {
        Rectangle btn = GetSpeedButtonRect(content, i);
        bool active = GetDefaultGameplaySpeed() == choices[i];
        DrawPill(btn, TextFormat("%dx", choices[i]), active);
    }

    Rectangle note = {card.x + 18.0f, card.y + 132.0f, card.width - 36.0f, 82.0f};
    DrawRectangleRounded(note, 0.12f, 14, {28, 34, 46, 240});
    DrawTextUI(u8"Что это меняет", {note.x + 14.0f, note.y + 12.0f}, 12.5f, {150, 160, 176, 255});
    DrawTextUI(u8"• Темп симуляции при старте\n• Скорость после нажатия Play\n• Восприятие новой партии", {note.x + 14.0f, note.y + 32.0f},
               13.0f, {210, 220, 232, 255});

    Rectangle footer = {content.x + 28.0f, content.y + 368.0f, content.width - 56.0f, 116.0f};
    DrawSectionCard(footer, u8"Текущие значения", u8"В реальном времени", {90, 190, 255, 255});
    char speedText[32];
    std::snprintf(speedText, sizeof(speedText), "%dx", GetDefaultGameplaySpeed());
    DrawTextUI(u8"По умолчанию", {footer.x + 18.0f, footer.y + 72.0f}, 13.0f, {150, 160, 176, 255});
    DrawTextUI(speedText, {footer.x + 18.0f, footer.y + 92.0f}, 22.0f, WHITE);
}

static void DrawPhantomSection(Rectangle content, const char* title, const char* body) {
    Rectangle card = {content.x + 28.0f, content.y + 92.0f, content.width - 56.0f, 182.0f};
    DrawSectionCard(card, title, u8"Фантомный раздел", {88, 110, 138, 255});
    DrawTextUI(body, {card.x + 18.0f, card.y + 72.0f}, 15.0f, {160, 168, 180, 255});
    DrawRectangleRounded({card.x + 18.0f, card.y + 112.0f, card.width - 36.0f, 44.0f}, 0.14f, 10, {20, 26, 36, 220});
    DrawCenteredTextUI(u8"Скоро", {card.x + 18.0f, card.y + 112.0f, card.width - 36.0f, 44.0f}, 16.0f, {120, 130, 142, 255});
}

} // namespace

void LoadAppSettings() {
    gGameState.settings.defaultSpeedMultiplier = 1;
    CopyLabName("AEGIS LAB");

    std::string path = SettingsPath();
    if (!FileExists(path.c_str())) {
        SaveAppSettings();
        return;
    }

    char* text = LoadFileText(path.c_str());
    if (!text) return;

    std::string data(text);
    UnloadFileText(text);

    auto readString = [&](const char* key, std::string& out) {
        size_t pos = data.find(key);
        if (pos == std::string::npos) return false;
        pos = data.find(':', pos);
        if (pos == std::string::npos) return false;
        pos = data.find('"', pos);
        if (pos == std::string::npos) return false;
        size_t end = pos + 1;
        bool escape = false;
        for (; end < data.size(); ++end) {
            char c = data[end];
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') break;
        }
        if (end >= data.size()) return false;
        out = JsonUnescape(data.substr(pos + 1, end - pos - 1));
        return true;
    };

    auto readInt = [&](const char* key, int& out) {
        size_t pos = data.find(key);
        if (pos == std::string::npos) return false;
        pos = data.find(':', pos);
        if (pos == std::string::npos) return false;
        int value = 0;
        if (std::sscanf(data.c_str() + pos + 1, "%d", &value) != 1) return false;
        out = value;
        return true;
    };

    std::string labName;
    if (readString("\"labName\"", labName)) {
        CopyLabName(labName.c_str());
        if (gGameState.settings.labName[0] == '\0') CopyLabName("AEGIS LAB");
    }
    if (!readInt("\"defaultSpeedMultiplier\"", gGameState.settings.defaultSpeedMultiplier)) {
        gGameState.settings.defaultSpeedMultiplier = 1;
    }
    gGameState.settings.defaultSpeedMultiplier = ClampSpeed(gGameState.settings.defaultSpeedMultiplier);
}

void SaveAppSettings() {
    std::string path = SettingsPath();
    std::string lab = JsonEscape(GetNonEmptyLabName());
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
        "{\n  \"labName\": \"%s\",\n  \"defaultSpeedMultiplier\": %d\n}\n",
        lab.c_str(), ClampSpeed(gGameState.settings.defaultSpeedMultiplier));
    SaveFileText(path.c_str(), buffer);
}

const char* GetLabDisplayName() {
    return GetNonEmptyLabName();
}

int GetDefaultGameplaySpeed() {
    return ClampSpeed(gGameState.settings.defaultSpeedMultiplier);
}

void PrepareSettingsScreen() {
    settingsPanelAnimation = 0.0f;
    editingLabName = false;
    selectedCategory = std::clamp(selectedCategory, 0, (int)std::size(CATEGORIES) - 1);
}

void UpdateSettingsScreen(float delta) {
    UpdateMenuBackground(delta);
    settingsPanelAnimation += (1.0f - settingsPanelAnimation) * std::min(1.0f, delta * 9.0f);

    Vector2 mouse = GetMousePosition();
    bool escapePressed = IsKeyPressed(KEY_ESCAPE);
    bool enterPressed = IsKeyPressed(KEY_ENTER);
    bool escapeConsumed = false;
    Rectangle panel = GetSettingsPanelRect();
    Rectangle sidebar = GetSidebarRect(panel);

    for (int i = 0; i < (int)std::size(CATEGORIES); ++i) {
        Rectangle rect = GetCategoryRect(sidebar, i);
        bool hovered = CATEGORIES[i].enabled && CheckCollisionPointRec(mouse, rect);
        categoryHover[i] += ((hovered ? 1.0f : 0.0f) - categoryHover[i]) * std::min(1.0f, delta * 12.0f);
        if (hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            selectedCategory = i;
            editingLabName = false;
        }
    }

    Rectangle content = GetContentRect(panel);
    Rectangle nameField = GetNameFieldRect(content);
    if (selectedCategory == 1) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            Rectangle reset = GetResetButtonRect(content);
            if (CheckCollisionPointRec(mouse, reset)) {
                CopyLabName("AEGIS LAB");
                editingLabName = false;
                SaveAppSettings();
            } else {
                editingLabName = CheckCollisionPointRec(mouse, nameField);
            }
        }

        if (editingLabName) {
            int codepoint = 0;
            while ((codepoint = GetCharPressed()) > 0) {
                if (codepoint == '\n' || codepoint == '\r') {
                    editingLabName = false;
                    SaveAppSettings();
                    break;
                }
                AppendUtf8Codepoint(gGameState.settings.labName, sizeof(gGameState.settings.labName), codepoint);
                SaveAppSettings();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                RemoveLastUtf8Codepoint(gGameState.settings.labName);
                if (gGameState.settings.labName[0] == '\0') CopyLabName("AEGIS LAB");
                SaveAppSettings();
            }
            if (escapePressed) {
                editingLabName = false;
                escapeConsumed = true;
            }
        }
    }

    if (selectedCategory == 2 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        const int choices[] = {1, 2, 4, 8};
        for (int i = 0; i < 4; ++i) {
            Rectangle btn = GetSpeedButtonRect(content, i);
            if (CheckCollisionPointRec(mouse, btn)) {
                gGameState.settings.defaultSpeedMultiplier = choices[i];
                SaveAppSettings();
            }
        }
    }

    if (escapePressed && !escapeConsumed) {
        if (editingLabName) {
            editingLabName = false;
        } else {
            gGameState.currentScreen = SCREEN_MENU;
        }
    }

    if (enterPressed && editingLabName) {
        editingLabName = false;
        SaveAppSettings();
    }
}

void DrawSettingsScreen() {
    DrawBackdrop();
    DrawSettingsLogo();

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle panel = GetSettingsPanelRect();
    float scale = GetUIScale();

    DrawRectangle(0, 0, (int)sw, (int)sh, {2, 4, 10, 44});

    float t = settingsPanelAnimation;
    Rectangle outer = {panel.x, panel.y + (1.0f - t) * 18.0f, panel.width, panel.height};
    DrawSettingsPanel(outer);

    Rectangle sidebar = GetSidebarRect(outer);
    Rectangle content = GetContentRect(outer);

    DrawTextUI(u8"Настройки", {outer.x + 24.0f, outer.y + 18.0f}, 24.0f, WHITE);
    DrawTextUI(u8"Discord-style лаборатория и темп игры", {outer.x + 24.0f, outer.y + 48.0f}, 13.0f, {150, 160, 176, 255});

    DrawRectangleRounded(sidebar, 0.08f, 18, {17, 22, 30, 220});
    DrawRectangleRoundedLinesEx(sidebar, 0.08f, 18, 1.0f, {255, 255, 255, 18});
    DrawTextUI(u8"Категории", {sidebar.x + 16.0f, sidebar.y - 24.0f}, 12.0f, {132, 140, 154, 255});
    for (int i = 0; i < (int)std::size(CATEGORIES); ++i) {
        Rectangle rect = GetCategoryRect(sidebar, i);
        bool selected = selectedCategory == i;
        bool hovered = CATEGORIES[i].enabled && categoryHover[i] > 0.05f;
        DrawCategory(rect, CATEGORIES[i], selected, hovered);
    }

    DrawRectangleRounded(content, 0.08f, 18, {15, 20, 30, 224});
    DrawRectangleRoundedLinesEx(content, 0.08f, 18, 1.0f, {255, 255, 255, 18});

    const SettingsCategory& category = CATEGORIES[selectedCategory];
    DrawTextUI(category.title, {content.x + 28.0f, content.y + 20.0f}, 22.0f, WHITE);
    DrawTextUI(category.subtitle, {content.x + 28.0f, content.y + 48.0f}, 13.0f, {150, 160, 176, 255});

    switch (selectedCategory) {
        case 0: DrawGeneralSection(content); break;
        case 1: DrawLabSection(content); break;
        case 2: DrawGameplaySection(content); break;
        case 3: DrawPhantomSection(content, u8"Внешний вид", u8"Потом здесь можно будет собрать цветовую тему и фоновые акценты."); break;
        case 4: DrawPhantomSection(content, u8"Уведомления", u8"Здесь удобно поселятся звуки, карточки и анимации."); break;
        case 5: DrawPhantomSection(content, u8"Эксперимент", u8"Это место для будущих переключателей и странных идей."); break;
        default: break;
    }

    Rectangle close = {panel.x + panel.width - 54.0f, panel.y + 18.0f, 34.0f, 34.0f};
    DrawPill(close, "X", false);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), close)) {
        gGameState.currentScreen = SCREEN_MENU;
        editingLabName = false;
    }
}

// ============================================================================
// How to play
// ============================================================================


namespace {

Rectangle GuidePanel() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float width = std::min(1180.0f, sw - 90.0f);
    float height = std::min(720.0f, sh - 70.0f);
    return {std::round((sw - width) * 0.5f), std::round((sh - height) * 0.5f), width, height};
}

Rectangle BackButton(Rectangle panel) {
    return {panel.x + panel.width - 148.0f, panel.y + 28.0f, 116.0f, 38.0f};
}

void DrawGuideCard(Rectangle bounds, const char* number, const char* title,
                   const char* body, Color accent) {
    DrawRectangleRounded(bounds, 0.055f, 12, {12, 28, 40, 238});
    DrawRectangleRoundedLinesEx(bounds, 0.055f, 12, 1.0f,
                                {accent.r, accent.g, accent.b, 95});
    DrawCircleV({bounds.x + 38.0f, bounds.y + 38.0f}, 21.0f,
                {accent.r, accent.g, accent.b, 205});
    DrawCenteredTextUI(number, {bounds.x + 17.0f, bounds.y + 17.0f, 42.0f, 42.0f},
                       14.0f, {4, 18, 24, 255});
    DrawTextUI(title, {bounds.x + 76.0f, bounds.y + 20.0f}, 19.0f, WHITE);
    DrawTextUI(body, {bounds.x + 24.0f, bounds.y + 82.0f}, 14.0f,
               {194, 209, 219, 255});
}

} // namespace

void PrepareHowToPlayScreen() {
}

void UpdateHowToPlayScreen(float) {
    Rectangle panel = GuidePanel();
    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
         CheckCollisionPointRec(GetMousePosition(), BackButton(panel)))) {
        gGameState.currentScreen = SCREEN_MENU;
    }
}

void DrawHowToPlayScreen() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Rectangle panel = GuidePanel();

    ClearBackground({4, 8, 15, 255});
    DrawRectangleGradientV(0, 0, sw, sh, {8, 20, 31, 255}, {17, 8, 23, 255});
    for (int x = 0; x < sw; x += 54) DrawLine(x, 0, x, sh, {255, 255, 255, 6});
    for (int y = 0; y < sh; y += 54) DrawLine(0, y, sw, y, {255, 255, 255, 5});

    DrawRectangleRounded({panel.x + 10.0f, panel.y + 14.0f, panel.width, panel.height},
                         0.03f, 16, {0, 0, 0, 120});
    DrawRectangleRounded(panel, 0.03f, 16, {7, 16, 27, 250});
    DrawRectangleRoundedLinesEx(panel, 0.03f, 16, 1.0f, {84, 180, 216, 105});

    DrawTextUI(u8"КАК ИГРАТЬ", {panel.x + 34.0f, panel.y + 28.0f}, 30.0f, WHITE);
    DrawTextUI(u8"Определите вирус, создайте лекарство и остановите пандемию.",
               {panel.x + 34.0f, panel.y + 72.0f}, 15.0f, {156, 185, 200, 255});

    Rectangle back = BackButton(panel);
    bool hovered = CheckCollisionPointRec(GetMousePosition(), back);
    DrawRectangleRounded(back, 0.18f, 9,
                         hovered ? Color{48, 88, 108, 245} : Color{28, 51, 67, 235});
    DrawRectangleRoundedLinesEx(back, 0.18f, 9, 1.0f, {105, 183, 214, 130});
    DrawCenteredTextUI(u8"НАЗАД  [ESC]", back, 13.0f, WHITE);

    float gap = 18.0f;
    float cardWidth = (panel.width - 86.0f - gap) * 0.5f;
    float cardHeight = 204.0f;
    float left = panel.x + 34.0f;
    float top = panel.y + 126.0f;

    DrawGuideCard({left, top, cardWidth, cardHeight}, "1", u8"РАЗМЕСТИТЕ ЛАБОРАТОРИЮ",
                  u8"Выберите страну на карте. Лаборатория дает ресурсы и открывает исследования.",
                  {74, 196, 255, 255});
    DrawGuideCard({left + cardWidth + gap, top, cardWidth, cardHeight}, "2", u8"ОТКРОЙТЕ МЕТОДЫ",
                  u8"Покупайте улучшения лаборатории. Новое оборудование открывает мини-игры.",
                  {242, 190, 78, 255});
    DrawGuideCard({left, top + cardHeight + gap, cardWidth, cardHeight}, "3", u8"СОБЕРИТЕ УЛИКИ",
                  u8"Нажмите J, откройте «Мини-игры» и проведите анализы. Результаты запоминайте сами.",
                  {80, 211, 176, 255});
    DrawGuideCard({left + cardWidth + gap, top + cardHeight + gap, cardWidth, cardHeight}, "4", u8"ПОСТАВЬТЕ ДИАГНОЗ",
                  u8"Во вкладке «Дедукция» вручную отметьте улики, исключите вирусы и подтвердите ответ.",
                  {194, 119, 255, 255});

    DrawCenteredTextUI(u8"ЛКМ — действия   •   J — блокнот   •   Esc — назад/пауза   •   F11 — режим окна",
                       {panel.x + 34.0f, panel.y + panel.height - 62.0f,
                        panel.width - 68.0f, 32.0f},
                       14.0f, {162, 181, 194, 255});
}

// ============================================================================
// Menu and FFmpeg video background
// ============================================================================


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

static float buttonHoverAnimation[5] = {};
static float buttonPressAnimation[5] = {};

static void LogFFmpegError(const char* step, int err) {
    char message[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, message, sizeof(message));
    TraceLog(LOG_WARNING, "VIDEO: %s failed (%d): %s", step, err, message);
}

static bool RestartVideo(VideoPlayer* video) {
    if (!video || !video->formatCtx || !video->codecCtx) return false;

    int ret = av_seek_frame(video->formatCtx, video->videoStream, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LogFFmpegError("av_seek_frame", ret);
        return false;
    }

    avcodec_flush_buffers(video->codecCtx);
    video->accumulator = 0.0;
    return true;
}

static bool UploadDecodedFrame(VideoPlayer* video) {
    if (!video || !video->frame || !video->rgbaFrame || !video->swsCtx || video->texture.id == 0) return false;

    sws_scale(video->swsCtx, video->frame->data, video->frame->linesize, 0, video->height, video->rgbaFrame->data, video->rgbaFrame->linesize);
    UpdateTexture(video->texture, video->pixels);
    return true;
}

static bool DecodeNextVideoFrame(VideoPlayer* video) {
    if (!video || !video->formatCtx || !video->codecCtx) return false;

    for (int attempt = 0; attempt < 512; ++attempt) {
        int ret = av_read_frame(video->formatCtx, video->packet);

        if (ret == AVERROR_EOF) {
            if (!RestartVideo(video)) return false;
            continue;
        }

        if (ret < 0) {
            LogFFmpegError("av_read_frame", ret);
            return false;
        }

        if (video->packet->stream_index != video->videoStream) {
            av_packet_unref(video->packet);
            continue;
        }

        ret = avcodec_send_packet(video->codecCtx, video->packet);
        av_packet_unref(video->packet);

        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            LogFFmpegError("avcodec_send_packet", ret);
            return false;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(video->codecCtx, video->frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                LogFFmpegError("avcodec_receive_frame", ret);
                return false;
            }
            return UploadDecodedFrame(video);
        }
    }

    return false;
}

static bool InitVideoPlayer(VideoPlayer* video, const char* fileName) {
    if (!video) return false;

    std::string path = ResourcePath(fileName);
    if (!FileExists(path.c_str())) {
        TraceLog(LOG_WARNING, "VIDEO: file not found: %s", path.c_str());
        return false;
    }

    std::memset(video, 0, sizeof(VideoPlayer));
    video->videoStream = -1;
    video->frameTime = 1.0 / 30.0;

    int ret = avformat_open_input(&video->formatCtx, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LogFFmpegError("avformat_open_input", ret);
        return false;
    }

    ret = avformat_find_stream_info(video->formatCtx, nullptr);
    if (ret < 0) {
        LogFFmpegError("avformat_find_stream_info", ret);
        return false;
    }

    ret = av_find_best_stream(video->formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        LogFFmpegError("av_find_best_stream", ret);
        return false;
    }

    video->videoStream = ret;
    AVStream* stream = video->formatCtx->streams[video->videoStream];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) return false;

    video->codecCtx = avcodec_alloc_context3(decoder);
    if (!video->codecCtx) return false;

    ret = avcodec_parameters_to_context(video->codecCtx, stream->codecpar);
    if (ret < 0) {
        LogFFmpegError("avcodec_parameters_to_context", ret);
        return false;
    }

    ret = avcodec_open2(video->codecCtx, decoder, nullptr);
    if (ret < 0) {
        LogFFmpegError("avcodec_open2", ret);
        return false;
    }

    video->width = video->codecCtx->width;
    video->height = video->codecCtx->height;

    AVRational fps = stream->avg_frame_rate.num > 0 ? stream->avg_frame_rate : stream->r_frame_rate;
    if (fps.num > 0 && fps.den > 0) video->frameTime = (double)fps.den / (double)fps.num;
    if (video->width <= 0 || video->height <= 0) return false;

    video->packet = av_packet_alloc();
    video->frame = av_frame_alloc();
    video->rgbaFrame = av_frame_alloc();
    if (!video->packet || !video->frame || !video->rgbaFrame) return false;

    video->pixels = (unsigned char*)MemAlloc(video->width * video->height * 4);
    if (!video->pixels) return false;
    std::memset(video->pixels, 0, video->width * video->height * 4);

    ret = av_image_fill_arrays(video->rgbaFrame->data, video->rgbaFrame->linesize, video->pixels, AV_PIX_FMT_RGBA, video->width, video->height, 1);
    if (ret < 0) {
        LogFFmpegError("av_image_fill_arrays", ret);
        return false;
    }

    video->swsCtx = sws_getContext(video->width, video->height, video->codecCtx->pix_fmt, video->width, video->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!video->swsCtx) return false;

    Image image = {};
    image.data = video->pixels;
    image.width = video->width;
    image.height = video->height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    video->texture = LoadTextureFromImage(image);
    if (video->texture.id == 0) return false;
    SetTextureFilter(video->texture, TEXTURE_FILTER_BILINEAR);

    video->ready = DecodeNextVideoFrame(video);
    return video->ready;
}

static void UpdateVideoPlayer(VideoPlayer* video, float delta) {
    if (!video || !video->ready) return;

    video->accumulator += delta;
    int guard = 0;
    while (video->accumulator >= video->frameTime && guard < 4) {
        DecodeNextVideoFrame(video);
        video->accumulator -= video->frameTime;
        ++guard;
    }
}

void UpdateMenuBackground(float delta) {
    UpdateVideoPlayer(&gGameState.menu.backgroundVideo, delta);
}

static void UnloadVideoPlayer(VideoPlayer* video) {
    if (!video) return;
    if (video->texture.id != 0) UnloadTexture(video->texture);
    if (video->swsCtx) sws_freeContext(video->swsCtx);
    if (video->packet) av_packet_free(&video->packet);
    if (video->frame) av_frame_free(&video->frame);
    if (video->rgbaFrame) av_frame_free(&video->rgbaFrame);
    if (video->codecCtx) avcodec_free_context(&video->codecCtx);
    if (video->formatCtx) avformat_close_input(&video->formatCtx);
    if (video->pixels) MemFree(video->pixels);
    *video = {};
}

static Rectangle GetButtonRect(int index) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float scale = GetUIScale();
    float bw = 292.0f * scale;
    float bh = 48.0f * scale;
    float gap = 10.0f * scale;
    float x = sw - 40.0f * scale - bw;
    float y = sh - 30.0f * scale - bh - (float)index * (bh + gap);
    return {x, y, bw, bh};
}

static void DrawMenuBackground() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    VideoPlayer& video = gGameState.menu.backgroundVideo;

    if (video.ready && video.texture.id != 0) {
        float coverScale = fmaxf(sw / (float)video.width, sh / (float)video.height);
        Rectangle dst = {(sw - (float)video.width * coverScale) * 0.5f, (sh - (float)video.height * coverScale) * 0.5f, (float)video.width * coverScale, (float)video.height * coverScale};
        DrawTexturePro(video.texture, {0, 0, (float)video.width, (float)video.height}, dst, {0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground({5, 6, 14, 255});
        DrawRectangleGradientV(0, 0, (int)sw, (int)sh, {5, 8, 20, 255}, {22, 8, 18, 255});
    }
}

static void DrawLogo() {
    if (!gGameState.menu.logoLoaded || gGameState.menu.logoTexture.id == 0) return;

    float scale = GetUIScale();
    float w = 300.0f * scale;
    float h = 76.0f * scale;
    Rectangle src = {96.0f, 152.0f, 692.0f, 176.0f};
    Rectangle dst = {(float)GetScreenWidth() - 20.0f * scale - w, 20.0f * scale, w, h};
    DrawTexturePro(gGameState.menu.logoTexture, src, dst, {0, 0}, 0.0f, WHITE);
}

static void DrawMenuButton(int index, Rectangle r, const char* text) {
    float hover = buttonHoverAnimation[index];
    float press = buttonPressAnimation[index];
    float scale = GetUIScale();
    bool primary = index == 0;

    r.x -= hover * 3.0f * scale;
    r.y -= hover * 1.0f * scale;
    r.y += press * 2.0f * scale;
    float radius = 0.16f;

    DrawRectangleRounded({r.x, r.y + (5.0f - hover * 2.0f) * scale, r.width, r.height},
                         radius, 10, {0, 0, 0, (unsigned char)(60 + hover * 25)});

    Color fill;
    Color border;
    Color textColor;
    if (primary) {
        fill = LerpColor({220, 226, 234, 220}, {242, 246, 251, 238}, hover);
        fill = LerpColor(fill, {194, 202, 212, 232}, press);
        border = LerpColor({255, 255, 255, 110}, {255, 255, 255, 175}, hover);
        textColor = LerpColor({16, 21, 28, 255}, {4, 8, 13, 255}, hover);
    } else {
        fill = LerpColor({19, 23, 29, 198}, {31, 37, 46, 226}, hover);
        fill = LerpColor(fill, {15, 19, 25, 234}, press);
        border = LerpColor({255, 255, 255, 28}, {255, 255, 255, 78}, hover);
        textColor = LerpColor({222, 228, 235, 238}, WHITE, hover);
    }

    DrawRectangleRounded(r, radius, 24, border);
    Rectangle inner = {
        r.x + 1.0f * scale,
        r.y + 1.0f * scale,
        r.width - 2.0f * scale,
        r.height - 2.0f * scale
    };
    DrawRectangleRounded(inner, radius, 24, fill);

    DrawLineEx({inner.x + 10.0f * scale, inner.y + 1.0f},
               {inner.x + inner.width - 10.0f * scale, inner.y + 1.0f},
               1.0f, {255, 255, 255, (unsigned char)(primary ? 115 + hover * 45 : 20 + hover * 18)});
    DrawLineEx({inner.x + 10.0f * scale, inner.y + inner.height - 1.0f},
               {inner.x + inner.width - 10.0f * scale, inner.y + inner.height - 1.0f},
               1.0f, {0, 0, 0, (unsigned char)(primary ? 22 : 55)});

    DrawCenteredTextUI(text, inner, 18.0f * scale, textColor);
}

void LoadMenuAssets() {
    LoadAppSettings();
    std::string logoPath = ResourcePath("antivirus.png");
    if (FileExists(logoPath.c_str())) {
        gGameState.menu.logoTexture = LoadTexture(logoPath.c_str());
        gGameState.menu.logoLoaded = gGameState.menu.logoTexture.id != 0;
        if (gGameState.menu.logoLoaded) SetTextureFilter(gGameState.menu.logoTexture, TEXTURE_FILTER_BILINEAR);
    }

    if (!InitVideoPlayer(&gGameState.menu.backgroundVideo, "background.mp4")) {
        InitVideoPlayer(&gGameState.menu.backgroundVideo, "background3.mp4");
    }
}

void UnloadMenuAssets() {
    SaveAppSettings();
    UnloadVideoPlayer(&gGameState.menu.backgroundVideo);
    if (gGameState.menu.logoLoaded && gGameState.menu.logoTexture.id != 0) {
        UnloadTexture(gGameState.menu.logoTexture);
        gGameState.menu.logoTexture = {0};
        gGameState.menu.logoLoaded = false;
    }
}

void UpdateMenu(float delta) {
    UpdateMenuBackground(delta);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 5; ++i) {
        Rectangle rect = GetButtonRect(i);
        bool hovered = CheckCollisionPointRec(mouse, rect);
        float hoverTarget = hovered ? 1.0f : 0.0f;
        float pressTarget = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1.0f : 0.0f;
        buttonHoverAnimation[i] += (hoverTarget - buttonHoverAnimation[i]) * std::min(1.0f, delta * 12.0f);
        buttonPressAnimation[i] += (pressTarget - buttonPressAnimation[i]) * std::min(1.0f, delta * 18.0f);

        if (hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (i == 0) {
                PrepareDifficultyScreen();
                gGameState.currentScreen = SCREEN_DIFFICULTY;
            } else if (i == 1) {
                PrepareSettingsScreen();
                gGameState.currentScreen = SCREEN_SETTINGS;
            } else if (i == 2) {
                gGameState.currentScreen = SCREEN_LOAD;
            } else if (i == 3) {
                PrepareHowToPlayScreen();
                gGameState.currentScreen = SCREEN_HOWTOPLAY;
            } else if (i == 4) {
                gGameState.shouldExit = true;
            }
        }
    }
}

void DrawMenu() {
    static const char* labels[5] = {u8"Играть", u8"Настройки", u8"Загрузить игру", u8"Как играть?", u8"Выйти"};
    DrawMenuBackground();
    DrawLogo();

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 5; ++i) {
        Rectangle rect = GetButtonRect(i);
        DrawMenuButton(i, rect, labels[i]);
    }
}
