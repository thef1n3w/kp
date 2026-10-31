#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <random>


namespace {
std::mt19937& AssayRng() {
    static std::mt19937 rng((unsigned int)std::random_device{}());
    return rng;
}
void Text(Font font, const char* value, Vector2 pos, float size, Color color) {
    pos.x = std::round(pos.x); pos.y = std::round(pos.y);
    DrawTextEx(font, value, pos, size, 0.0f, color);
}
void Center(Font font, const char* value, Rectangle area, float size, Color color) {
    Vector2 m = MeasureTextEx(font, value, size, 0.0f);
    Text(font, value, {area.x + (area.width - m.x) * 0.5f, area.y + (area.height - m.y) * 0.5f}, size, color);
}
Rectangle Window() {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float w = std::min(1050.0f, sw - 90.0f), h = std::min(680.0f, sh - 70.0f);
    return {(sw - w) * 0.5f, (sh - h) * 0.5f, w, h};
}
Rectangle Reagent(Rectangle window, int i) {
    float gap = 20.0f, width = (window.width - 160.0f - gap * 2.0f) / 3.0f;
    return {window.x + 80.0f + i * (width + gap), window.y + 270.0f, width, 92.0f};
}
Rectangle Choice(Rectangle window, int i) {
    float gap = 16.0f, width = (window.width - 160.0f - gap * 2.0f) / 3.0f;
    return {window.x + 80.0f + i * (width + gap), window.y + window.height - 105.0f, width, 52.0f};
}
Rectangle Close(Rectangle window) { return {window.x + window.width - 58.0f, window.y + 22.0f, 34.0f, 34.0f}; }
}

void SerologyAssayGame::Start(ImmuneResponseEvidence target) {
    targetResponse = target; active = true; protocolStep = 0; errors = 0;
    reagentOrder[0] = 0; reagentOrder[1] = 1; reagentOrder[2] = 2;
    std::shuffle(std::begin(reagentOrder), std::end(reagentOrder), AssayRng());
}

SerologyAssayResult SerologyAssayGame::Choose(ImmuneResponseEvidence choice) {
    active = false;
    return choice == targetResponse ? SerologyAssayResult::Correct : SerologyAssayResult::Wrong;
}

SerologyAssayResult SerologyAssayGame::Update() {
    if (!active) return SerologyAssayResult::None;
    Rectangle window = Window(); Vector2 mouse = GetMousePosition();
    if (IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, Close(window)))) {
        active = false; return SerologyAssayResult::Cancelled;
    }
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return SerologyAssayResult::None;
    if (!IsResultStage()) {
        for (int i = 0; i < 3; ++i) {
            if (!CheckCollisionPointRec(mouse, Reagent(window, i))) continue;
            if (i == reagentOrder[protocolStep]) ++protocolStep;
            else ++errors;
            if (errors >= 3) { active = false; return SerologyAssayResult::Wrong; }
            return SerologyAssayResult::None;
        }
    } else {
        for (int i = 0; i < 3; ++i) if (CheckCollisionPointRec(mouse, Choice(window, i))) return Choose((ImmuneResponseEvidence)i);
    }
    return SerologyAssayResult::None;
}

void SerologyAssayGame::Draw(Font font) const {
    Rectangle w = Window(); Vector2 mouse = GetMousePosition(); Color accent = {255, 122, 170, 255};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {1, 3, 8, 226});
    DrawRectangleRounded(w, 0.035f, 16, {8, 17, 28, 252});
    DrawRectangleRoundedLinesEx(w, 0.035f, 16, 1.5f, {accent.r, accent.g, accent.b, 130});
    Text(font, u8"СЕРОЛОГИЧЕСКИЙ ТЕСТ", {w.x + 34, w.y + 28}, 25, WHITE);
    Text(font, IsResultStage() ? u8"Оцените выраженность агглютинации." : u8"Добавьте реагенты в порядке протокола.",
         {w.x + 34, w.y + 70}, 15, {193, 207, 218, 255});
    Rectangle close = Close(w); DrawRectangleRounded(close, .2f, 8, {45, 31, 43, 245}); Center(font, "X", close, 16, WHITE);

    const char* reagentNames[] = {u8"СЫВОРОТКА", u8"АНТИГЕН", u8"КРАСИТЕЛЬ"};
    if (!IsResultStage()) {
        Text(font, u8"ПРОТОКОЛ", {w.x + 80, w.y + 138}, 12, accent);
        for (int i = 0; i < 3; ++i) {
            const char* name = reagentNames[reagentOrder[i]];
            Text(font, name, {w.x + 80 + i * 180.0f, w.y + 174}, 15,
                 i < protocolStep ? Color{91, 220, 151, 255} : WHITE);
            if (i < 2) Text(font, ">", {w.x + 226 + i * 180.0f, w.y + 174}, 17, accent);
        }
        for (int i = 0; i < 3; ++i) {
            Rectangle r = Reagent(w, i); bool hover = CheckCollisionPointRec(mouse, r);
            DrawRectangleRounded(r, .12f, 10, hover ? Color{94, 48, 76, 245} : Color{29, 37, 52, 245});
            DrawRectangleRoundedLinesEx(r, .12f, 10, 1, {accent.r, accent.g, accent.b, 110});
            Center(font, reagentNames[i], r, 15, WHITE);
        }
        Center(font, TextFormat(u8"Шаг %d/3   •   Ошибки %d/3", protocolStep, errors),
               {w.x, w.y + 405, w.width, 40}, 14, {167, 184, 197, 255});
        return;
    }
    float clumps = targetResponse == ImmuneResponseEvidence::Strong ? 18.0f :
                   targetResponse == ImmuneResponseEvidence::Weak ? 7.0f : 1.0f;
    Vector2 center = {w.x + w.width * .5f, w.y + 300};
    DrawCircleV(center, 125, {80, 20, 45, 255}); DrawCircleLines((int)center.x, (int)center.y, 125, accent);
    for (int i = 0; i < (int)clumps; ++i) {
        float a = i * 2.399f, r = 25.0f + (i % 5) * 18.0f;
        DrawCircleV({center.x + std::cos(a) * r, center.y + std::sin(a) * r}, 7 + i % 3, {255, 170, 194, 230});
    }
    const char* labels[] = {u8"СИЛЬНАЯ", u8"СЛАБАЯ", u8"ОТСУТСТВУЕТ"};
    for (int i = 0; i < 3; ++i) {
        Rectangle r = Choice(w, i); bool hover = CheckCollisionPointRec(mouse, r);
        DrawRectangleRounded(r, .15f, 9, hover ? Color{accent.r, accent.g, accent.b, 170} : Color{28, 38, 52, 245});
        Center(font, labels[i], r, 14, WHITE);
    }
}
