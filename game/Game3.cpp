#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>


namespace {

std::mt19937& MicroscopeRng() {
    static std::mt19937 rng((unsigned int)std::random_device{}());
    return rng;
}

void DrawMicroText(Font font, const char* text, Vector2 position, float size, Color color) {
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    DrawTextEx(font, text, position, size, 0.0f, color);
}

void DrawMicroCentered(Font font, const char* text, Rectangle bounds, float size, Color color) {
    Vector2 measured = MeasureTextEx(font, text, size, 0.0f);
    DrawMicroText(font, text,
                  {bounds.x + (bounds.width - measured.x) * 0.5f,
                   bounds.y + (bounds.height - measured.y) * 0.5f},
                  size, color);
}

Rectangle MicroscopeWindow() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float width = std::min(1140.0f, sw - 90.0f);
    float height = std::min(740.0f, sh - 70.0f);
    return {std::round((sw - width) * 0.5f), std::round((sh - height) * 0.5f), width, height};
}

Rectangle Viewport(Rectangle window) {
    return {window.x + 62.0f, window.y + 142.0f, window.width - 124.0f, 390.0f};
}

Rectangle FocusButton(Rectangle window) {
    return {window.x + window.width * 0.5f - 170.0f, window.y + window.height - 92.0f, 340.0f, 46.0f};
}

Rectangle ChoiceButton(Rectangle window, int index) {
    float gap = 16.0f;
    float width = (window.width - 156.0f - gap * 2.0f) / 3.0f;
    return {window.x + 78.0f + index * (width + gap), window.y + window.height - 108.0f, width, 54.0f};
}

Rectangle CloseButton(Rectangle window) {
    return {window.x + window.width - 58.0f, window.y + 22.0f, 34.0f, 34.0f};
}

void DrawMorphologySample(MorphologyEvidence morphology, Rectangle area, Color accent) {
    Vector2 center = {area.x + area.width * 0.5f, area.y + area.height * 0.5f};
    if (morphology == MorphologyEvidence::Spherical) {
        for (int i = 0; i < 7; ++i) {
            float angle = (float)i * 0.897f;
            float radius = 42.0f + (float)(i % 3) * 38.0f;
            Vector2 p = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius * 0.62f};
            DrawCircleV(p, 21.0f + (float)(i % 2) * 6.0f, {accent.r, accent.g, accent.b, 48});
            DrawCircleLines((int)p.x, (int)p.y, 21.0f + (float)(i % 2) * 6.0f, accent);
            for (int s = 0; s < 8; ++s) {
                float spikeAngle = (float)s * PI / 4.0f;
                Vector2 a = {p.x + std::cos(spikeAngle) * 23.0f, p.y + std::sin(spikeAngle) * 23.0f};
                Vector2 b = {p.x + std::cos(spikeAngle) * 30.0f, p.y + std::sin(spikeAngle) * 30.0f};
                DrawLineEx(a, b, 1.5f, accent);
            }
        }
    } else if (morphology == MorphologyEvidence::Filamentous) {
        for (int strand = 0; strand < 4; ++strand) {
            Vector2 previous = {area.x + 110.0f, area.y + 86.0f + strand * 66.0f};
            for (int i = 1; i <= 16; ++i) {
                float t = (float)i / 16.0f;
                Vector2 next = {
                    area.x + 110.0f + t * (area.width - 220.0f),
                    area.y + 86.0f + strand * 66.0f + std::sin(t * 10.0f + strand) * 25.0f
                };
                DrawLineEx(previous, next, 8.0f, {accent.r, accent.g, accent.b, 52});
                DrawLineEx(previous, next, 2.0f, accent);
                previous = next;
            }
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            float angle = (float)i * PI / 4.0f;
            float ring = i < 4 ? 72.0f : 142.0f;
            Vector2 p = {center.x + std::cos(angle) * ring, center.y + std::sin(angle) * ring * 0.58f};
            DrawPoly(p, 6, i < 4 ? 29.0f : 23.0f, 30.0f, {accent.r, accent.g, accent.b, 52});
            DrawPolyLinesEx(p, 6, i < 4 ? 29.0f : 23.0f, 30.0f, 2.0f, accent);
        }
    }
}

} // namespace

void ElectronMicroscopeGame::Start(MorphologyEvidence target) {
    targetMorphology = target;
    active = true;
    focusedLayers = 0;
    errors = 0;
    focusPosition = 0.0f;
    focusDirection = 1.0f;

    std::uniform_real_distribution<float> focus(0.18f, 0.82f);
    for (float& targetValue : targetFocus) targetValue = focus(MicroscopeRng());
}

void ElectronMicroscopeGame::TryLockFocus() {
    if (IsClassificationStage()) return;
    if (std::fabs(focusPosition - targetFocus[focusedLayers]) <= 0.055f) {
        ++focusedLayers;
        focusPosition = 0.0f;
        focusDirection = 1.0f;
    } else {
        ++errors;
    }
}

ElectronMicroscopeResult ElectronMicroscopeGame::ChooseMorphology(MorphologyEvidence choice) {
    active = false;
    return choice == targetMorphology ? ElectronMicroscopeResult::Correct : ElectronMicroscopeResult::Wrong;
}

ElectronMicroscopeResult ElectronMicroscopeGame::Update(float delta) {
    if (!active) return ElectronMicroscopeResult::None;
    Rectangle window = MicroscopeWindow();
    Vector2 mouse = GetMousePosition();

    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, CloseButton(window)))) {
        active = false;
        return ElectronMicroscopeResult::Cancelled;
    }

    if (!IsClassificationStage()) {
        focusPosition += delta * 0.42f * focusDirection;
        if (focusPosition >= 1.0f) {
            focusPosition = 1.0f;
            focusDirection = -1.0f;
        } else if (focusPosition <= 0.0f) {
            focusPosition = 0.0f;
            focusDirection = 1.0f;
        }

        bool lock = IsKeyPressed(KEY_SPACE) ||
                    (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                     CheckCollisionPointRec(mouse, FocusButton(window)));
        if (lock) {
            TryLockFocus();
            if (errors >= 3) {
                active = false;
                return ElectronMicroscopeResult::Wrong;
            }
        }
        return ElectronMicroscopeResult::None;
    }

    if (IsKeyPressed(KEY_ONE)) return ChooseMorphology(MorphologyEvidence::Spherical);
    if (IsKeyPressed(KEY_TWO)) return ChooseMorphology(MorphologyEvidence::Filamentous);
    if (IsKeyPressed(KEY_THREE)) return ChooseMorphology(MorphologyEvidence::Crystalline);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < 3; ++i) {
            if (CheckCollisionPointRec(mouse, ChoiceButton(window, i))) {
                return ChooseMorphology((MorphologyEvidence)i);
            }
        }
    }
    return ElectronMicroscopeResult::None;
}

void ElectronMicroscopeGame::Draw(Font font) const {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle window = MicroscopeWindow();
    Rectangle viewport = Viewport(window);
    Vector2 mouse = GetMousePosition();
    Color accent = {194, 119, 255, 255};

    DrawRectangle(0, 0, (int)sw, (int)sh, {1, 3, 8, 228});
    DrawRectangleRounded({window.x + 10.0f, window.y + 13.0f, window.width, window.height},
                         0.035f, 16, {0, 0, 0, 125});
    DrawRectangleRounded(window, 0.035f, 16, {8, 16, 27, 252});
    DrawRectangleRoundedLinesEx(window, 0.035f, 16, 1.5f, {accent.r, accent.g, accent.b, 130});

    DrawMicroText(font, u8"ЭЛЕКТРОННЫЙ МИКРОСКОП", {window.x + 34.0f, window.y + 28.0f},
                  25.0f, WHITE);
    DrawMicroText(font, IsClassificationStage()
                            ? u8"Изображение стабилизировано. Классифицируйте форму вириона."
                            : u8"Зафиксируйте объектив в подсвеченной зоне на каждой глубине образца.",
                  {window.x + 34.0f, window.y + 72.0f}, 15.0f, {193, 205, 218, 255});

    Rectangle close = CloseButton(window);
    bool closeHovered = CheckCollisionPointRec(mouse, close);
    DrawRectangleRounded(close, 0.22f, 8, closeHovered ? Color{94, 39, 48, 245} : Color{28, 34, 47, 245});
    DrawMicroCentered(font, "X", close, 16.0f, closeHovered ? Color{255, 150, 160, 255} : WHITE);

    DrawRectangleRounded(viewport, 0.035f, 14, {6, 13, 22, 252});
    DrawRectangleRoundedLinesEx(viewport, 0.035f, 14, 1.0f, {accent.r, accent.g, accent.b, 90});
    DrawCircleV({viewport.x + viewport.width * 0.5f, viewport.y + viewport.height * 0.5f},
                std::min(viewport.width, viewport.height) * 0.43f, {15, 24, 35, 255});

    if (!IsClassificationStage()) {
        float trackX = viewport.x + 90.0f;
        float trackWidth = viewport.width - 180.0f;
        float trackY = viewport.y + viewport.height * 0.5f;
        DrawLineEx({trackX, trackY}, {trackX + trackWidth, trackY}, 8.0f, {43, 48, 65, 255});

        float targetX = trackX + targetFocus[focusedLayers] * trackWidth;
        DrawRectangleRounded({targetX - 34.0f, trackY - 52.0f, 68.0f, 104.0f},
                             0.18f, 8, {accent.r, accent.g, accent.b, 34});
        DrawRectangleRoundedLinesEx({targetX - 34.0f, trackY - 52.0f, 68.0f, 104.0f},
                                    0.18f, 8, 2.0f, accent);

        float lensX = trackX + focusPosition * trackWidth;
        DrawLineEx({lensX, trackY - 92.0f}, {lensX, trackY + 92.0f}, 4.0f, {231, 239, 255, 255});
        DrawCircleV({lensX, trackY}, 12.0f, WHITE);
        DrawCircleLines((int)lensX, (int)trackY, 21.0f, accent);

        char status[96];
        std::snprintf(status, sizeof(status), u8"Слои: %d/3     Ошибки: %d/3", focusedLayers, errors);
        DrawMicroText(font, status, {viewport.x + 26.0f, viewport.y + viewport.height - 34.0f},
                      14.0f, errors ? Color{245, 157, 104, 255} : Color{166, 180, 199, 255});

        Rectangle focusButton = FocusButton(window);
        bool hovered = CheckCollisionPointRec(mouse, focusButton);
        DrawRectangleRounded(focusButton, 0.18f, 10,
                             hovered ? Color{116, 67, 160, 255} : Color{75, 48, 112, 255});
        DrawRectangleRoundedLinesEx(focusButton, 0.18f, 10, 1.0f, {accent.r, accent.g, accent.b, 175});
        DrawMicroCentered(font, u8"ЗАФИКСИРОВАТЬ ФОКУС  [SPACE]", focusButton, 14.0f, WHITE);
        return;
    }

    DrawMorphologySample(targetMorphology, viewport, accent);
    DrawMicroCentered(font, u8"Выберите наблюдаемую морфологию",
                      {viewport.x, viewport.y + viewport.height - 54.0f, viewport.width, 40.0f},
                      16.0f, {225, 232, 241, 255});

    const char* labels[] = {u8"1  •  СФЕРИЧЕСКАЯ", u8"2  •  НИТЕВИДНАЯ", u8"3  •  КРИСТАЛЛИЧЕСКАЯ"};
    for (int i = 0; i < 3; ++i) {
        Rectangle choice = ChoiceButton(window, i);
        bool hovered = CheckCollisionPointRec(mouse, choice);
        DrawRectangleRounded(choice, 0.16f, 10,
                             hovered ? Color{accent.r, accent.g, accent.b, 175} : Color{25, 34, 49, 245});
        DrawRectangleRoundedLinesEx(choice, 0.16f, 10, 1.0f, {accent.r, accent.g, accent.b, 155});
        DrawMicroCentered(font, labels[i], choice, 14.0f, WHITE);
    }
}
