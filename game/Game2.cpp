#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>


namespace {

std::mt19937& SequencerRng() {
    static std::mt19937 rng((unsigned int)std::random_device{}());
    return rng;
}

Vector2 MeasureSequencerText(Font font, const char* text, float size) {
    return MeasureTextEx(font, text, size, 0.0f);
}

void DrawSequencerText(Font font, const char* text, Vector2 position, float size, Color color) {
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    DrawTextEx(font, text, position, size, 0.0f, color);
}

void DrawSequencerCentered(Font font, const char* text, Rectangle bounds, float size, Color color) {
    Vector2 measured = MeasureSequencerText(font, text, size);
    DrawSequencerText(font, text,
                      {bounds.x + (bounds.width - measured.x) * 0.5f,
                       bounds.y + (bounds.height - measured.y) * 0.5f},
                      size, color);
}

Rectangle SequencerWindow() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float width = std::min(1120.0f, sw - 90.0f);
    float height = std::min(720.0f, sh - 70.0f);
    return {std::round((sw - width) * 0.5f), std::round((sh - height) * 0.5f), width, height};
}

Rectangle ScanArea(Rectangle window) {
    return {window.x + 70.0f, window.y + 184.0f, window.width - 140.0f, 238.0f};
}

Rectangle CaptureButton(Rectangle window) {
    return {window.x + window.width * 0.5f - 160.0f, window.y + window.height - 96.0f, 320.0f, 48.0f};
}

Rectangle ChoiceButton(Rectangle window, int index) {
    float gap = 24.0f;
    float width = (window.width - 188.0f - gap) * 0.5f;
    return {window.x + 94.0f + index * (width + gap), window.y + window.height - 116.0f, width, 58.0f};
}

Rectangle CloseButton(Rectangle window) {
    return {window.x + window.width - 58.0f, window.y + 22.0f, 34.0f, 34.0f};
}

void DrawBasePair(Font font, const char* left, const char* right, Vector2 center, Color accent) {
    DrawCircleV({center.x - 34.0f, center.y}, 20.0f, {accent.r, accent.g, accent.b, 54});
    DrawCircleV({center.x + 34.0f, center.y}, 20.0f, {accent.r, accent.g, accent.b, 54});
    DrawLineEx({center.x - 14.0f, center.y}, {center.x + 14.0f, center.y}, 2.0f, {accent.r, accent.g, accent.b, 150});
    DrawSequencerCentered(font, left, {center.x - 54.0f, center.y - 20.0f, 40.0f, 40.0f}, 17.0f, WHITE);
    DrawSequencerCentered(font, right, {center.x + 14.0f, center.y - 20.0f, 40.0f, 40.0f}, 17.0f, WHITE);
}

} // namespace

void PathogenSequencerGame::Start(PathogenEvidence target) {
    targetPathogen = target;
    active = true;
    capturedFragments = 0;
    errors = 0;
    scannerPosition = 0.0f;

    std::uniform_real_distribution<float> first(0.16f, 0.27f);
    std::uniform_real_distribution<float> second(0.43f, 0.59f);
    std::uniform_real_distribution<float> third(0.72f, 0.86f);
    targetPositions[0] = first(SequencerRng());
    targetPositions[1] = second(SequencerRng());
    targetPositions[2] = third(SequencerRng());
}

bool PathogenSequencerGame::TryCapture() {
    if (IsInterpretationStage()) return false;
    float distance = std::fabs(scannerPosition - targetPositions[capturedFragments]);
    if (distance <= 0.055f) {
        ++capturedFragments;
        scannerPosition = 0.0f;
        return true;
    }
    ++errors;
    return false;
}

PathogenSequencerResult PathogenSequencerGame::ChoosePathogen(PathogenEvidence choice) {
    active = false;
    return choice == targetPathogen ? PathogenSequencerResult::Correct : PathogenSequencerResult::Wrong;
}

PathogenSequencerResult PathogenSequencerGame::Update(float delta) {
    if (!active) return PathogenSequencerResult::None;
    if (IsKeyPressed(KEY_ESCAPE)) {
        active = false;
        return PathogenSequencerResult::Cancelled;
    }

    Rectangle window = SequencerWindow();
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, CloseButton(window))) {
        active = false;
        return PathogenSequencerResult::Cancelled;
    }

    if (!IsInterpretationStage()) {
        scannerPosition += delta * 0.34f;
        if (scannerPosition > 1.0f) scannerPosition -= 1.0f;

        bool capturePressed = IsKeyPressed(KEY_SPACE) ||
                              (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                               CheckCollisionPointRec(mouse, CaptureButton(window)));
        if (capturePressed) {
            TryCapture();
            if (errors >= 3) {
                active = false;
                return PathogenSequencerResult::Wrong;
            }
        }
        return PathogenSequencerResult::None;
    }

    bool chooseRna = IsKeyPressed(KEY_ONE);
    bool chooseDna = IsKeyPressed(KEY_TWO);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        chooseRna = chooseRna || CheckCollisionPointRec(mouse, ChoiceButton(window, 0));
        chooseDna = chooseDna || CheckCollisionPointRec(mouse, ChoiceButton(window, 1));
    }
    if (chooseRna) return ChoosePathogen(PathogenEvidence::RNA);
    if (chooseDna) return ChoosePathogen(PathogenEvidence::DNA);
    return PathogenSequencerResult::None;
}

const char* PathogenSequencerGame::GetDetectedMarkerName() const {
    return targetPathogen == PathogenEvidence::RNA ? u8"Урацил (U)" : u8"Тимин (T)";
}

void PathogenSequencerGame::Draw(Font font) const {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle window = SequencerWindow();
    Rectangle scan = ScanArea(window);
    Vector2 mouse = GetMousePosition();
    Color accent = {74, 218, 187, 255};

    DrawRectangle(0, 0, (int)sw, (int)sh, {0, 3, 8, 225});
    DrawRectangleRounded({window.x + 10.0f, window.y + 13.0f, window.width, window.height},
                         0.035f, 16, {0, 0, 0, 125});
    DrawRectangleRounded(window, 0.035f, 16, {7, 17, 27, 252});
    DrawRectangleRoundedLinesEx(window, 0.035f, 16, 1.5f, {accent.r, accent.g, accent.b, 130});

    DrawSequencerText(font, u8"ГЕНОМНЫЙ СЕКВЕНАТОР", {window.x + 34.0f, window.y + 28.0f},
                      25.0f, WHITE);
    DrawSequencerText(font, u8"АНАЛИЗ НУКЛЕИНОВОЙ КИСЛОТЫ", {window.x + 34.0f, window.y + 64.0f},
                      12.0f, {115, 190, 176, 255});
    DrawSequencerText(font,
                      IsInterpretationStage()
                          ? u8"Последовательность восстановлена. Интерпретируйте маркер вручную."
                          : u8"Остановите сканер внутри подсвеченного участка. Нужно зафиксировать 3 фрагмента.",
                      {window.x + 34.0f, window.y + 112.0f}, 15.0f, {190, 207, 216, 255});

    Rectangle close = CloseButton(window);
    bool closeHovered = CheckCollisionPointRec(mouse, close);
    DrawRectangleRounded(close, 0.22f, 8, closeHovered ? Color{94, 39, 48, 245} : Color{25, 37, 46, 245});
    DrawSequencerCentered(font, "X", close, 16.0f, closeHovered ? Color{255, 150, 160, 255} : WHITE);

    DrawRectangleRounded(scan, 0.045f, 14, {5, 22, 29, 245});
    DrawRectangleRoundedLinesEx(scan, 0.045f, 14, 1.0f, {accent.r, accent.g, accent.b, 92});

    for (int x = (int)scan.x + 20; x < (int)(scan.x + scan.width); x += 38) {
        DrawLine(x, (int)scan.y + 1, x, (int)(scan.y + scan.height - 1), {255, 255, 255, 10});
    }
    for (int y = (int)scan.y + 22; y < (int)(scan.y + scan.height); y += 36) {
        DrawLine((int)scan.x + 1, y, (int)(scan.x + scan.width - 1), y, {255, 255, 255, 9});
    }

    if (!IsInterpretationStage()) {
        float trackY = scan.y + scan.height * 0.5f;
        DrawLineEx({scan.x + 32.0f, trackY}, {scan.x + scan.width - 32.0f, trackY},
                   4.0f, {31, 74, 78, 255});

        for (int i = 0; i < 3; ++i) {
            float markerX = scan.x + 32.0f + targetPositions[i] * (scan.width - 64.0f);
            bool completed = i < capturedFragments;
            Color marker = completed ? Color{83, 225, 151, 255}
                                     : (i == capturedFragments ? Color{244, 202, 75, 255}
                                                               : Color{78, 112, 119, 180});
            DrawRectangleRounded({markerX - 28.0f, trackY - 54.0f, 56.0f, 108.0f},
                                 0.16f, 8, {marker.r, marker.g, marker.b, (unsigned char)(completed ? 36 : 24)});
            DrawRectangleRoundedLinesEx({markerX - 28.0f, trackY - 54.0f, 56.0f, 108.0f},
                                        0.16f, 8, completed ? 2.0f : 1.0f, marker);
            DrawSequencerCentered(font, completed ? "OK" : TextFormat("%d", i + 1),
                                  {markerX - 28.0f, trackY - 18.0f, 56.0f, 36.0f},
                                  15.0f, marker);
        }

        float scannerX = scan.x + 32.0f + scannerPosition * (scan.width - 64.0f);
        DrawRectangleGradientV((int)scannerX - 8, (int)scan.y + 18, 16, (int)scan.height - 36,
                               {accent.r, accent.g, accent.b, 8}, {accent.r, accent.g, accent.b, 80});
        DrawLineEx({scannerX, scan.y + 16.0f}, {scannerX, scan.y + scan.height - 16.0f},
                   3.0f, accent);
        DrawCircleV({scannerX, trackY}, 8.0f, WHITE);

        char status[96];
        std::snprintf(status, sizeof(status), u8"Фрагменты: %d/3     Ошибки: %d/3",
                      capturedFragments, errors);
        DrawSequencerText(font, status, {scan.x + 24.0f, scan.y + scan.height - 31.0f},
                          13.5f, errors > 0 ? Color{245, 157, 104, 255} : Color{150, 188, 190, 255});

        Rectangle capture = CaptureButton(window);
        bool hovered = CheckCollisionPointRec(mouse, capture);
        DrawRectangleRounded(capture, 0.18f, 10,
                             hovered ? Color{38, 145, 132, 255} : Color{25, 104, 99, 255});
        DrawRectangleRoundedLinesEx(capture, 0.18f, 10, 1.0f, {accent.r, accent.g, accent.b, 175});
        DrawSequencerCentered(font, u8"ЗАФИКСИРОВАТЬ ОБРАЗЕЦ  [SPACE]", capture, 14.5f, WHITE);
        return;
    }

    DrawSequencerText(font, u8"ВОССТАНОВЛЕННАЯ ЦЕПОЧКА", {scan.x + 28.0f, scan.y + 22.0f},
                      13.0f, {128, 194, 181, 255});
    const char* marker = targetPathogen == PathogenEvidence::RNA ? "U" : "T";
    const char* leftBases[] = {"A", "C", "G", marker, "A", "G"};
    const char* rightBases[] = {marker, "G", "C", "A", marker, "C"};
    float pairGap = (scan.width - 150.0f) / 5.0f;
    for (int i = 0; i < 6; ++i) {
        DrawBasePair(font, leftBases[i], rightBases[i],
                     {scan.x + 75.0f + pairGap * i, scan.y + 112.0f}, accent);
    }

    std::string detected = std::string(u8"Обнаруженный маркер: ") + GetDetectedMarkerName();
    DrawSequencerCentered(font, detected.c_str(),
                          {scan.x + 30.0f, scan.y + 164.0f, scan.width - 60.0f, 46.0f},
                          18.0f, {232, 244, 238, 255});

    for (int i = 0; i < 2; ++i) {
        Rectangle choice = ChoiceButton(window, i);
        bool hovered = CheckCollisionPointRec(mouse, choice);
        Color choiceAccent = i == 0 ? Color{82, 194, 255, 255} : Color{194, 119, 255, 255};
        DrawRectangleRounded(choice, 0.16f, 10,
                             hovered ? Color{choiceAccent.r, choiceAccent.g, choiceAccent.b, 170}
                                     : Color{20, 39, 52, 245});
        DrawRectangleRoundedLinesEx(choice, 0.16f, 10, 1.0f,
                                    {choiceAccent.r, choiceAccent.g, choiceAccent.b, 170});
        DrawSequencerCentered(font, i == 0 ? u8"1  •  РНК" : u8"2  •  ДНК",
                              choice, 18.0f, WHITE);
    }
}
