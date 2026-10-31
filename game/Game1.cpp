#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>


static const VirusDefinition VIRUS_MATRIX[] = {
    {"Sidero-V2", SymptomEvidence::Hemorrhagic, PathogenEvidence::DNA, MorphologyEvidence::Filamentous,
     ImmuneResponseEvidence::Absent, ThermalStabilityEvidence::Heat},
    {"Phthisis-X", SymptomEvidence::Pulmonary, PathogenEvidence::RNA, MorphologyEvidence::Spherical,
     ImmuneResponseEvidence::Strong, ThermalStabilityEvidence::Cold},
    {"Epiderm-9", SymptomEvidence::Cutaneous, PathogenEvidence::DNA, MorphologyEvidence::Crystalline,
     ImmuneResponseEvidence::Weak, ThermalStabilityEvidence::Neutral},
    {"Pulmo-Zeta", SymptomEvidence::Pulmonary, PathogenEvidence::DNA, MorphologyEvidence::Spherical,
     ImmuneResponseEvidence::Absent, ThermalStabilityEvidence::Cold},
    {"Kera-V", SymptomEvidence::Cutaneous, PathogenEvidence::RNA, MorphologyEvidence::Filamentous,
     ImmuneResponseEvidence::Strong, ThermalStabilityEvidence::Heat}
};

static const char* PULMONARY_SYMPTOMS[] = {
    u8"Сухой кашель, одышка, высокая температура",
    u8"Боль в груди, кашель с мокротой, хрипы",
    u8"Затрудненное дыхание, лихорадка"
};

static const char* CUTANEOUS_SYMPTOMS[] = {
    u8"Красная сыпь по всему телу, сильный зуд",
    u8"Волдыри на руках и лице, шелушение кожи",
    u8"Острый дерматит, жжение, повышенная температура"
};

static const char* HEMORRHAGIC_SYMPTOMS[] = {
    u8"Кровотечение из носа, подкожные синяки",
    u8"Рвота с кровью, внутренние кровоизлияния",
    u8"Кровавые слезы, резкий упадок давления"
};

static std::mt19937& PatientRng() {
    static std::mt19937 rng((unsigned int)std::random_device{}());
    return rng;
}

static std::vector<Texture2D> patientPortraits;

static void DrawPortraitTexture(Texture2D texture, Rectangle bounds) {
    float scale = std::max(bounds.width / (float)texture.width, bounds.height / (float)texture.height);
    float srcWidth = bounds.width / scale;
    float srcHeight = bounds.height / scale;
    Rectangle src = {
        ((float)texture.width - srcWidth) * 0.5f,
        ((float)texture.height - srcHeight) * 0.5f,
        srcWidth,
        srcHeight
    };
    DrawTexturePro(texture, src, bounds, {0, 0}, 0.0f, WHITE);
}

const VirusDefinition* GetVirusMatrix(int* count) {
    if (count) *count = (int)(sizeof(VIRUS_MATRIX) / sizeof(VIRUS_MATRIX[0]));
    return VIRUS_MATRIX;
}

const char* SymptomEvidenceName(SymptomEvidence symptom) {
    switch (symptom) {
        case SymptomEvidence::Pulmonary: return u8"Легочная";
        case SymptomEvidence::Cutaneous: return u8"Кожная";
        case SymptomEvidence::Hemorrhagic: return u8"Геморрагическая";
    }
    return u8"Неизвестная";
}

const char* PathogenEvidenceName(PathogenEvidence pathogen) {
    switch (pathogen) {
        case PathogenEvidence::RNA: return u8"РНК";
        case PathogenEvidence::DNA: return u8"ДНК";
    }
    return u8"Неизвестно";
}

const char* MorphologyEvidenceName(MorphologyEvidence morphology) {
    switch (morphology) {
        case MorphologyEvidence::Spherical: return u8"Сферическая";
        case MorphologyEvidence::Filamentous: return u8"Нитевидная";
        case MorphologyEvidence::Crystalline: return u8"Кристаллическая";
    }
    return u8"Неизвестная";
}

const char* ImmuneResponseEvidenceName(ImmuneResponseEvidence response) {
    switch (response) {
        case ImmuneResponseEvidence::Strong: return u8"Сильная";
        case ImmuneResponseEvidence::Weak: return u8"Слабая";
        case ImmuneResponseEvidence::Absent: return u8"Отсутствует";
    }
    return u8"Неизвестная";
}

const char* ThermalStabilityEvidenceName(ThermalStabilityEvidence stability) {
    switch (stability) {
        case ThermalStabilityEvidence::Cold: return u8"Холод";
        case ThermalStabilityEvidence::Heat: return u8"Тепло";
        case ThermalStabilityEvidence::Neutral: return u8"Нейтральная";
    }
    return u8"Неизвестная";
}

static const char* PickSymptomText(SymptomEvidence symptom) {
    const char** list = PULMONARY_SYMPTOMS;
    int count = 3;
    if (symptom == SymptomEvidence::Cutaneous) list = CUTANEOUS_SYMPTOMS;
    if (symptom == SymptomEvidence::Hemorrhagic) list = HEMORRHAGIC_SYMPTOMS;
    std::uniform_int_distribution<int> dist(0, count - 1);
    return list[dist(PatientRng())];
}

static Vector2 MeasurePatientText(const char* text, float fontSize) {
    return MeasureTextEx(gGameState.font, text, fontSize, 0.0f);
}

static void DrawPatientText(const char* text, Vector2 position, float fontSize, Color color) {
    position.x = roundf(position.x);
    position.y = roundf(position.y);
    DrawTextEx(gGameState.font, text, position, fontSize, 0.0f, color);
}

static void DrawPatientCenteredText(const char* text, Rectangle bounds, float fontSize, Color color) {
    Vector2 size = MeasurePatientText(text, fontSize);
    DrawPatientText(text, {bounds.x + (bounds.width - size.x) * 0.5f, bounds.y + (bounds.height - size.y) * 0.5f}, fontSize, color);
}

static void DrawPatientWrappedText(const char* text, Rectangle bounds, float fontSize, Color color, float lineGap = 4.0f) {
    std::string source(text ? text : "");
    std::string line;
    float y = bounds.y;
    size_t start = 0;

    while (start < source.size() && y <= bounds.y + bounds.height - fontSize) {
        size_t end = source.find(' ', start);
        std::string word = source.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::string candidate = line.empty() ? word : line + " " + word;

        if (!line.empty() && MeasurePatientText(candidate.c_str(), fontSize).x > bounds.width) {
            DrawPatientText(line.c_str(), {bounds.x, y}, fontSize, color);
            y += fontSize + lineGap;
            line = word;
        } else {
            line = candidate;
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (!line.empty() && y <= bounds.y + bounds.height - fontSize) {
        DrawPatientText(line.c_str(), {bounds.x, y}, fontSize, color);
    }
}

static Rectangle GetPatientWindowRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float guideWidth = std::min(340.0f, std::max(250.0f, sw * 0.23f));
    float gap = 18.0f;
    float width = std::min(1060.0f, sw - guideWidth - gap - 70.0f);
    float height = std::min(720.0f, sh - 58.0f);
    float totalWidth = width + gap + guideWidth;
    return {sw * 0.5f - totalWidth * 0.5f, sh * 0.5f - height * 0.5f, width, height};
}

static Rectangle GetPatientGuideRect(Rectangle window) {
    float sw = (float)GetScreenWidth();
    float guideWidth = std::min(340.0f, std::max(250.0f, sw * 0.23f));
    return {window.x + window.width + 18.0f, window.y, guideWidth, window.height};
}

static Rectangle GetPatientButtonRect(Rectangle window, int index) {
    float gap = 14.0f;
    float width = (window.width - 72.0f - gap * 2.0f) / 3.0f;
    return {window.x + 36.0f + index * (width + gap), window.y + window.height - 76.0f, width, 42.0f};
}

static void DrawPatientPortrait(Rectangle bounds, int index, int portraitIndex) {
    DrawRectangleRounded(bounds, 0.10f, 10, {10, 20, 30, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 10, 1.0f, {95, 125, 148, 82});

    if (portraitIndex >= 0 && portraitIndex < (int)patientPortraits.size() && patientPortraits[portraitIndex].id != 0) {
        Rectangle imageBounds = {bounds.x + 1.0f, bounds.y + 1.0f, bounds.width - 2.0f, bounds.height - 2.0f};
        DrawPortraitTexture(patientPortraits[portraitIndex], imageBounds);
        DrawRectangleRoundedLinesEx(bounds, 0.10f, 10, 1.0f, {120, 150, 170, 92});
        return;
    }

    Vector2 center = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.44f};
    float scale = std::min(bounds.width / 128.0f, bounds.height / 130.0f);
    Color skin = index % 2 == 0 ? Color{190, 210, 220, 220} : Color{160, 184, 198, 220};
    Color coat = {38, 70, 88, 235};
    DrawCircleV({center.x, center.y - 22.0f * scale}, 24.0f * scale, skin);
    DrawRectangleRounded({center.x - 45.0f * scale, center.y + 10.0f * scale, 90.0f * scale, 58.0f * scale},
                         0.35f, 12, coat);
    DrawRectangleRounded({center.x - 20.0f * scale, center.y + 4.0f * scale, 40.0f * scale, 24.0f * scale},
                         0.30f, 8, skin);
    DrawLineEx({center.x - 18.0f * scale, center.y - 26.0f * scale},
               {center.x - 8.0f * scale, center.y - 26.0f * scale}, 2.0f * scale, {20, 34, 44, 210});
    DrawLineEx({center.x + 8.0f * scale, center.y - 26.0f * scale},
               {center.x + 18.0f * scale, center.y - 26.0f * scale}, 2.0f * scale, {20, 34, 44, 210});
}

static void DrawSymptomGuideBlock(Rectangle area, const char* title, const char** symptoms, Color accent) {
    DrawRectangleRounded(area, 0.08f, 10, {15, 27, 38, 236});
    DrawRectangleRoundedLinesEx(area, 0.08f, 10, 1.0f, {accent.r, accent.g, accent.b, 105});
    DrawPatientText(title, {area.x + 16.0f, area.y + 13.0f}, 16.0f, accent);

    float y = area.y + 44.0f;
    for (int i = 0; i < 3; ++i) {
        DrawCircleV({area.x + 20.0f, y + 7.0f}, 3.0f, accent);
        DrawPatientWrappedText(symptoms[i], {area.x + 32.0f, y, area.width - 48.0f, 42.0f},
                               12.5f, {210, 222, 232, 255}, 3.0f);
        y += 54.0f;
    }
}

static void DrawPatientGuide(Rectangle guide) {
    DrawRectangleRounded({guide.x + 7.0f, guide.y + 10.0f, guide.width, guide.height}, 0.04f, 16, {0, 0, 0, 105});
    DrawRectangleRounded(guide, 0.04f, 16, {8, 18, 29, 248});
    DrawRectangleRoundedLinesEx(guide, 0.04f, 16, 1.0f, {80, 205, 245, 120});

    DrawPatientText(u8"Руководство", {guide.x + 22.0f, guide.y + 24.0f}, 24.0f, WHITE);
    DrawPatientWrappedText(u8"Сравните карточки с этими группами симптомов. Блокнот игра не заполняет автоматически.",
                           {guide.x + 22.0f, guide.y + 60.0f, guide.width - 44.0f, 50.0f},
                           13.0f, {166, 184, 198, 255}, 4.0f);

    DrawRectangleRec({guide.x + 18.0f, guide.y + 56.0f, guide.width - 36.0f, 58.0f}, {8, 18, 29, 248});
    DrawPatientWrappedText(u8"Сравните карточки с группами симптомов.",
                           {guide.x + 22.0f, guide.y + 60.0f, guide.width - 44.0f, 36.0f},
                           13.0f, {166, 184, 198, 255}, 4.0f);

    float blockGap = 14.0f;
    float top = guide.y + 126.0f;
    float blockHeight = (guide.height - 154.0f - blockGap * 2.0f) / 3.0f;
    DrawSymptomGuideBlock({guide.x + 18.0f, top, guide.width - 36.0f, blockHeight},
                          u8"Легочная", PULMONARY_SYMPTOMS, {86, 185, 255, 255});
    DrawSymptomGuideBlock({guide.x + 18.0f, top + blockHeight + blockGap, guide.width - 36.0f, blockHeight},
                          u8"Кожная", CUTANEOUS_SYMPTOMS, {90, 220, 150, 255});
    DrawSymptomGuideBlock({guide.x + 18.0f, top + (blockHeight + blockGap) * 2.0f, guide.width - 36.0f, blockHeight},
                          u8"Геморрагическая", HEMORRHAGIC_SYMPTOMS, {245, 92, 104, 255});
}

void PatientCardGame::SelectRandomVirus() {
    int count = 0;
    const VirusDefinition* viruses = GetVirusMatrix(&count);
    static std::vector<int> bag;
    static int lastVirus = -1;
    static int lastPathogen = -1;
    static int pathogenStreak = 0;

    if (bag.empty()) {
        bag.reserve(count);
        for (int i = 0; i < count; ++i) bag.push_back(i);
        std::shuffle(bag.begin(), bag.end(), PatientRng());
    }

    int choicePosition = 0;
    for (int i = 0; i < (int)bag.size(); ++i) {
        int candidate = bag[i];
        int pathogen = (int)viruses[candidate].pathogen;
        bool repeatsVirus = candidate == lastVirus && bag.size() > 1;
        bool extendsLongStreak = pathogenStreak >= 2 && pathogen == lastPathogen;
        if (!repeatsVirus && !extendsLongStreak) {
            choicePosition = i;
            break;
        }
    }

    activeVirusIndex = bag[choicePosition];
    bag.erase(bag.begin() + choicePosition);

    int pathogen = (int)viruses[activeVirusIndex].pathogen;
    pathogenStreak = pathogen == lastPathogen ? pathogenStreak + 1 : 1;
    lastPathogen = pathogen;
    lastVirus = activeVirusIndex;
}

void PatientCardGame::LoadPortraitAssets() {
    if (!patientPortraits.empty()) return;

    for (int i = 1; i <= 12; ++i) {
        char fileName[64];
        std::snprintf(fileName, sizeof(fileName), "patient_%02d.png", i);
        std::string path = ResourcePath(fileName);
        if (!FileExists(path.c_str())) continue;

        Texture2D texture = LoadTexture(path.c_str());
        if (texture.id != 0) {
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            patientPortraits.push_back(texture);
        }
    }
}

void PatientCardGame::UnloadPortraitAssets() {
    for (Texture2D texture : patientPortraits) {
        if (texture.id != 0) UnloadTexture(texture);
    }
    patientPortraits.clear();
}

void PatientCardGame::Start() {
    if (!HasActiveVirus()) SelectRandomVirus();
    GeneratePatientCards();
    lastChosenSymptomName.clear();
    active = true;
}

PatientCardResult PatientCardGame::Update() {
    if (!active) return PatientCardResult::None;
    if (IsKeyPressed(KEY_ESCAPE)) {
        active = false;
        return PatientCardResult::Cancelled;
    }

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return PatientCardResult::None;

    Vector2 mouse = GetMousePosition();
    Rectangle window = GetPatientWindowRect();
    const SymptomEvidence choices[3] = {
        SymptomEvidence::Pulmonary,
        SymptomEvidence::Cutaneous,
        SymptomEvidence::Hemorrhagic
    };

    for (int i = 0; i < 3; ++i) {
        if (CheckCollisionPointRec(mouse, GetPatientButtonRect(window, i))) {
            return ChooseSymptom(choices[i]);
        }
    }

    return PatientCardResult::None;
}

void PatientCardGame::Draw() const {
    if (!active) return;

    Rectangle window = GetPatientWindowRect();
    Rectangle guide = GetPatientGuideRect(window);
    Color accent = {80, 205, 245, 255};

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 4, 10, 178});
    DrawRectangleRounded({window.x + 9.0f, window.y + 12.0f, window.width, window.height}, 0.04f, 18, {0, 0, 0, 118});
    DrawRectangleRounded(window, 0.04f, 18, {9, 18, 30, 248});
    DrawRectangleRoundedLinesEx(window, 0.04f, 18, 1.0f, {accent.r, accent.g, accent.b, 126});

    DrawPatientText(u8"Эпидемиологический опрос", {window.x + 34.0f, window.y + 26.0f}, 28.0f, WHITE);
    DrawPatientText(u8"Прочитайте 5 карточек и вручную определите преобладающую симптоматику.", {window.x + 34.0f, window.y + 64.0f}, 15.0f, {172, 188, 202, 255});
    DrawPatientText(u8"Esc: закрыть без выбора", {window.x + window.width - 220.0f, window.y + 33.0f}, 14.0f, {135, 154, 170, 255});

    float cardGap = 12.0f;
    float cardWidth = (window.width - 68.0f - cardGap * 4.0f) / 5.0f;
    float cardTop = window.y + 112.0f;
    float cardHeight = window.height - 226.0f;
    for (int i = 0; i < 5; ++i) {
        Rectangle card = {window.x + 34.0f + i * (cardWidth + cardGap), cardTop, cardWidth, cardHeight};
        DrawRectangleRounded(card, 0.08f, 12, {17, 28, 40, 238});
        DrawRectangleRoundedLinesEx(card, 0.08f, 12, 1.0f, {95, 122, 145, 92});

        char title[32];
        std::snprintf(title, sizeof(title), u8"Пациент %d", i + 1);
        DrawPatientText(title, {card.x + 14.0f, card.y + 16.0f}, 16.0f, {205, 226, 238, 255});
        DrawPatientText(u8"жалобы:", {card.x + 14.0f, card.y + 48.0f}, 12.5f, {132, 150, 166, 255});
        Rectangle portrait = {card.x + 14.0f, card.y + 74.0f, card.width - 28.0f, std::min(150.0f, card.width * 0.86f)};
        int portraitIndex = i < (int)portraitIndices.size() ? portraitIndices[i] : -1;
        DrawPatientPortrait(portrait, i, portraitIndex);
        const char* text = i < (int)patientCards.size() ? patientCards[i].c_str() : "";
        DrawPatientWrappedText(text, {card.x + 14.0f, portrait.y + portrait.height + 18.0f, card.width - 28.0f,
                                      card.y + card.height - (portrait.y + portrait.height + 34.0f)},
                               15.0f, {225, 234, 240, 255}, 5.0f);
    }

    const char* labels[3] = {u8"ЛЕГОЧНАЯ", u8"КОЖНАЯ", u8"ГЕМОРРАГИЧЕСКАЯ"};
    const Color colors[3] = {{86, 185, 255, 255}, {90, 220, 150, 255}, {245, 92, 104, 255}};
    for (int i = 0; i < 3; ++i) {
        Rectangle button = GetPatientButtonRect(window, i);
        bool hovered = CheckCollisionPointRec(GetMousePosition(), button);
        Color color = colors[i];
        DrawRectangleRounded(button, 0.16f, 10, hovered ? Color{color.r, color.g, color.b, 220} : Color{25, 44, 60, 246});
        DrawRectangleRoundedLinesEx(button, 0.16f, 10, 1.0f, {color.r, color.g, color.b, (unsigned char)(hovered ? 220 : 130)});
        DrawPatientCenteredText(labels[i], button, 15.0f, hovered ? Color{4, 18, 26, 255} : WHITE);
    }

    DrawPatientGuide(guide);
}

SymptomEvidence PatientCardGame::GetActiveSymptom() const {
    return GetActiveVirus().symptom;
}

const VirusDefinition& PatientCardGame::GetActiveVirus() const {
    int count = 0;
    GetVirusMatrix(&count);
    int index = std::clamp(activeVirusIndex, 0, count - 1);
    return VIRUS_MATRIX[index];
}

const char* PatientCardGame::GetActiveSymptomName() const {
    return SymptomEvidenceName(GetActiveSymptom());
}

void PatientCardGame::GeneratePatientCards() {
    patientCards.clear();
    portraitIndices.clear();
    SymptomEvidence symptom = GetActiveSymptom();
    for (int i = 0; i < 5; ++i) {
        patientCards.emplace_back(PickSymptomText(symptom));
        if (!patientPortraits.empty()) {
            std::uniform_int_distribution<int> dist(0, (int)patientPortraits.size() - 1);
            portraitIndices.push_back(dist(PatientRng()));
        } else {
            portraitIndices.push_back(-1);
        }
    }
}

PatientCardResult PatientCardGame::ChooseSymptom(SymptomEvidence symptom) {
    lastChosenSymptomName = SymptomEvidenceName(symptom);
    active = false;
    return symptom == GetActiveSymptom() ? PatientCardResult::Correct : PatientCardResult::Wrong;
}
