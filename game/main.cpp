#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// ============================================================================
// Shared text, fonts and resource paths
// ============================================================================


float GetUIScale() {
    float scale = std::min((float)GetScreenWidth() / 1920.0f, (float)GetScreenHeight() / 1080.0f);
    if (scale < 0.75f) scale = 0.75f;
    if (scale > 1.35f) scale = 1.35f;
    return scale;
}

Color LerpColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return {
        (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
        (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
        (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
        (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t)
    };
}

std::string ResourcePath(const char* fileName) {
    std::string local = std::string("res/") + fileName;
    if (FileExists(local.c_str())) return local;

    std::string gameLocal = std::string("game/res/") + fileName;
    if (FileExists(gameLocal.c_str())) return gameLocal;

    std::string parent = std::string("../game/res/") + fileName;
    if (FileExists(parent.c_str())) return parent;

    std::string root = std::string("../res/") + fileName;
    if (FileExists(root.c_str())) return root;

    return local;
}

bool LoadUIFont() {
    const char* fallbackFonts[] = {
        "font.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };

    int codepoints[512];
    int count = 0;
    for (int i = 32; i <= 126; ++i) codepoints[count++] = i;
    for (int i = 0x0400; i <= 0x052F; ++i) codepoints[count++] = i;
    codepoints[count++] = 0x2014;
    codepoints[count++] = 0x2022;
    codepoints[count++] = 0x203A;
    codepoints[count++] = 0x2190;
    codepoints[count++] = 0x2192;
    codepoints[count++] = 0x00AB;
    codepoints[count++] = 0x00BB;
    codepoints[count++] = 0x2116;
    codepoints[count++] = 0x2295;
    codepoints[count++] = 0x2620;
    codepoints[count++] = 0x26A0;
    codepoints[count++] = 0x25BC;
    codepoints[count++] = 0x23F8;
    codepoints[count++] = 0x25B6;
    codepoints[count++] = 0x23E9;
    codepoints[count++] = 0x23ED;

    for (const char* path : fallbackFonts) {
        std::string resolved = path[1] == ':' ? std::string(path) : ResourcePath(path);
        if (!FileExists(resolved.c_str())) continue;

        gGameState.font = LoadFontEx(resolved.c_str(), 128, codepoints, count);
        if (gGameState.font.texture.id != 0) {
            GenTextureMipmaps(&gGameState.font.texture);
            SetTextureFilter(gGameState.font.texture, TEXTURE_FILTER_TRILINEAR);
            gGameState.fontLoaded = true;
            return true;
        }
    }

    gGameState.font = GetFontDefault();
    gGameState.fontLoaded = false;
    return false;
}

void UnloadUIFont() {
    if (gGameState.fontLoaded) {
        UnloadFont(gGameState.font);
        gGameState.fontLoaded = false;
    }
}

Vector2 MeasureTextUI(const char* text, float fontSize, float spacing) {
    return MeasureTextEx(gGameState.font, text, fontSize, spacing);
}

void DrawTextUI(const char* text, Vector2 position, float fontSize, Color color, float spacing) {
    position.x = roundf(position.x);
    position.y = roundf(position.y);
    DrawTextEx(gGameState.font, text, position, fontSize, spacing, color);
}

void DrawCenteredTextUI(const char* text, Rectangle bounds, float fontSize, Color color) {
    Vector2 size = MeasureTextUI(text, fontSize);
    DrawTextUI(text, {bounds.x + (bounds.width - size.x) * 0.5f, bounds.y + (bounds.height - size.y) * 0.5f}, fontSize, color);
}

void DrawCapsuleGradient(Rectangle bounds, Color top, Color bottom, Color border, Color shadow) {
    float radius = bounds.height * 0.5f;
    float cy = bounds.y + radius;

    DrawRectangleRounded({bounds.x + 3.0f, bounds.y + 4.0f, bounds.width, bounds.height}, 1.0f, 24, shadow);

    for (int y = 0; y < (int)bounds.height; ++y) {
        float yy = bounds.y + (float)y + 0.5f;
        float dy = fabsf(yy - cy);
        float inset = radius - sqrtf(fmaxf(0.0f, radius * radius - dy * dy));
        Color color = LerpColor(top, bottom, (float)y / fmaxf(1.0f, bounds.height - 1.0f));
        DrawLine((int)(bounds.x + inset), (int)(bounds.y + y), (int)(bounds.x + bounds.width - inset), (int)(bounds.y + y), color);
    }

    DrawRectangleRoundedLinesEx(bounds, 1.0f, 24, 1.0f, border);
}

void DrawVignette(int screenWidth, int screenHeight, unsigned char alpha) {
    DrawRectangleGradientV(0, 0, screenWidth, 110, {0, 0, 0, alpha}, {0, 0, 0, 0});
    DrawRectangleGradientV(0, screenHeight - 160, screenWidth, 160, {0, 0, 0, 0}, {0, 0, 0, alpha});
    DrawRectangleGradientH(0, 0, 130, screenHeight, {0, 0, 0, alpha}, {0, 0, 0, 0});
    DrawRectangleGradientH(screenWidth - 130, 0, 130, screenHeight, {0, 0, 0, 0}, {0, 0, 0, alpha});
}

std::string FormatNumber(long long value) {
    char buffer[64];
    if (value >= 1000000000LL) {
        std::snprintf(buffer, sizeof(buffer), "%.2fB", (double)value / 1000000000.0);
    } else if (value >= 1000000LL) {
        std::snprintf(buffer, sizeof(buffer), "%.1fM", (double)value / 1000000.0);
    } else if (value >= 1000LL) {
        std::snprintf(buffer, sizeof(buffer), "%.1fK", (double)value / 1000.0);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%lld", value);
    }
    return buffer;
}

// ============================================================================
// Startup loading screen
// ============================================================================


namespace {

enum LoadingPhase {
    PHASE_LOADING,
    PHASE_HOLD,
    PHASE_FADE
};

struct LoadingState {
    LoadingPhase phase = PHASE_LOADING;
    int loadStep = 0;
    float phaseTime = 0.0f;
    float displayedProgress = 0.0f;
    float targetProgress = 0.04f;
    bool assetsLoaded = false;
    Texture2D logoTexture = {0};
    bool logoLoaded = false;
};

LoadingState loading;

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float EaseOutCubic(float value) {
    value = 1.0f - Clamp01(value);
    return 1.0f - value * value * value;
}

} // namespace

void InitLoadingScreen() {
    loading = {};
    loading.targetProgress = 0.04f;
    gGameState.font = GetFontDefault();

    std::string logoPath = ResourcePath("antivirus.png");
    if (FileExists(logoPath.c_str())) {
        loading.logoTexture = LoadTexture(logoPath.c_str());
        loading.logoLoaded = loading.logoTexture.id != 0;
        if (loading.logoLoaded) SetTextureFilter(loading.logoTexture, TEXTURE_FILTER_BILINEAR);
    }
}

bool AreStartupAssetsLoaded() {
    return loading.assetsLoaded;
}

void UpdateLoadingScreen(float delta) {
    loading.phaseTime += delta;

    if (loading.phase == PHASE_LOADING) {
        if (loading.loadStep == 0 && loading.phaseTime >= 0.20f) {
            LoadUIFont();
            loading.targetProgress = 0.28f;
            loading.loadStep = 1;
        } else if (loading.loadStep == 1 && loading.displayedProgress >= 0.22f) {
            LoadMenuAssets();
            loading.targetProgress = 0.68f;
            loading.loadStep = 2;
        } else if (loading.loadStep == 2 && loading.displayedProgress >= 0.58f) {
            LoadGameplayAssets();
            loading.targetProgress = 1.0f;
            loading.loadStep = 3;
            loading.assetsLoaded = true;
        }

        float followSpeed = loading.loadStep >= 3 ? 1.25f : 0.72f;
        loading.displayedProgress += (loading.targetProgress - loading.displayedProgress) *
                                     std::min(1.0f, delta * followSpeed * 4.0f);

        if (loading.assetsLoaded && loading.displayedProgress >= 0.995f) {
            loading.displayedProgress = 1.0f;
            loading.phase = PHASE_HOLD;
            loading.phaseTime = 0.0f;
        }
        return;
    }

    if (loading.phase == PHASE_HOLD && loading.phaseTime >= 0.35f) {
        loading.phase = PHASE_FADE;
        loading.phaseTime = 0.0f;
    } else if (loading.phase == PHASE_FADE && loading.phaseTime >= 0.75f) {
        if (loading.logoLoaded && loading.logoTexture.id != 0) {
            UnloadTexture(loading.logoTexture);
            loading.logoTexture = {0};
            loading.logoLoaded = false;
        }
        gGameState.currentScreen = SCREEN_MENU;
    }
}

void DrawLoadingScreen() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float scale = std::clamp(std::min((float)sw / 1920.0f, (float)sh / 1080.0f), 0.75f, 1.35f);
    float fade = loading.phase == PHASE_FADE ? EaseOutCubic(loading.phaseTime / 0.75f) : 0.0f;
    if (loading.phase == PHASE_FADE) {
        DrawMenu();
        DrawRectangle(0, 0, sw, sh, {1, 4, 8, (unsigned char)(255.0f * (1.0f - fade))});
    } else {
        ClearBackground({1, 4, 8, 255});
    }

    float visibility = 1.0f - fade;
    float barWidth = 230.0f * scale;
    float barHeight = 5.0f * scale;
    float barY = (float)sh * 0.54f;
    Rectangle outer = {(float)sw * 0.5f - barWidth * 0.5f, barY, barWidth, barHeight};

    DrawRectangleRounded({outer.x - scale, outer.y - scale, outer.width + 2.0f * scale,
                          outer.height + 2.0f * scale},
                         1.0f, 12, {48, 57, 64, (unsigned char)(180.0f * visibility)});
    DrawRectangleRounded(outer, 1.0f, 12, {15, 20, 24, (unsigned char)(255.0f * visibility)});

    Rectangle fill = {outer.x, outer.y, outer.width * loading.displayedProgress, outer.height};
    if (fill.width > 1.0f) {
        DrawRectangleRounded(fill, 1.0f, 12, {64, 165, 235, (unsigned char)(255.0f * visibility)});
    }

    if (loading.logoLoaded && loading.logoTexture.id != 0) {
        Rectangle source = {96.0f, 152.0f, 692.0f, 176.0f};
        float logoWidth = 300.0f * scale;
        float logoHeight = 76.0f * scale;
        Rectangle destination = {
            (float)sw * 0.5f,
            barY - 45.0f * scale,
            logoWidth,
            logoHeight
        };
        DrawTexturePro(loading.logoTexture, source, destination,
                       {logoWidth * 0.5f, logoHeight}, 0.0f,
                       {255, 255, 255, (unsigned char)(255.0f * visibility)});
    }
}

// ============================================================================
// Application initialization and main loop
// ============================================================================

GameState gGameState;

static void DrawSimpleScreen(const char* title, const char* subtitle) {
    ClearBackground({6, 9, 18, 255});
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {9, 16, 32, 255}, {28, 11, 22, 255});

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle panel = {sw * 0.5f - 340.0f, sh * 0.5f - 95.0f, 680.0f, 190.0f};
    DrawRectangleRounded(panel, 0.18f, 20, {0, 0, 0, 105});
    DrawCenteredTextUI(title, {panel.x, panel.y + 30.0f, panel.width, 60.0f}, 42.0f, WHITE);
    DrawCenteredTextUI(subtitle, {panel.x, panel.y + 105.0f, panel.width, 40.0f}, 22.0f, {0, 191, 255, 255});
}

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_UNDECORATED);
    InitWindow(1920, 1080, u8"Антивирус");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    int monitor = GetCurrentMonitor();
    Vector2 monitorPosition = GetMonitorPosition(monitor);
    SetWindowPosition((int)monitorPosition.x, (int)monitorPosition.y);
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));

    InitLoadingScreen();

    while (!WindowShouldClose() && !gGameState.shouldExit) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        float delta = GetFrameTime();

        switch (gGameState.currentScreen) {
            case SCREEN_BOOT:
                UpdateLoadingScreen(delta);
                break;
            case SCREEN_MENU:
                UpdateMenu(delta);
                break;
            case SCREEN_SETTINGS:
                UpdateSettingsScreen(delta);
                break;
            case SCREEN_DIFFICULTY:
                UpdateDifficultyScreen(delta);
                break;
            case SCREEN_GAMEPLAY:
                UpdateGameplay(delta);
                break;
            case SCREEN_HOWTOPLAY:
                UpdateHowToPlayScreen(delta);
                break;
            case SCREEN_LOAD:
                if (IsKeyPressed(KEY_ESCAPE)) gGameState.currentScreen = SCREEN_MENU;
                break;
        }

        BeginDrawing();
        switch (gGameState.currentScreen) {
            case SCREEN_BOOT:
                DrawLoadingScreen();
                break;
            case SCREEN_MENU:
                DrawMenu();
                break;
            case SCREEN_SETTINGS:
                DrawSettingsScreen();
                break;
            case SCREEN_DIFFICULTY:
                DrawDifficultyScreen();
                break;
            case SCREEN_GAMEPLAY:
                DrawGameplay();
                break;
            case SCREEN_HOWTOPLAY:
                DrawHowToPlayScreen();
                break;
            case SCREEN_LOAD:
                DrawSimpleScreen(u8"Загрузка игры", u8"ESC — вернуться в меню");
                break;
        }
        EndDrawing();
    }

    if (AreStartupAssetsLoaded()) {
        UnloadGameplayAssets();
        UnloadMenuAssets();
        UnloadUIFont();
    }
    CloseWindow();
    return 0;
}
