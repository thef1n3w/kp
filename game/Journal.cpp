#include "GameState.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>


namespace {

struct VirusArticle {
    const char* description;
    const char* properties;
    const char* weaknesses;
};

const VirusArticle VIRUS_ARTICLES[] = {
    {
        u8"Агрессивный геморрагический штамм, повреждающий стенки сосудов и нарушающий свертываемость крови.",
        u8"Провоцирует внутренние кровоизлияния и быстро повышает смертность при перегрузке больниц.",
        u8"Плохо переносит раннюю изоляцию носителей и терапию, стабилизирующую сосудистую систему."
    },
    {
        u8"Острый респираторный штамм, поражающий альвеолы. Быстро распространяется в густонаселенных мегаполисах.",
        u8"Вызывает тяжелую гипоксию, увеличивает смертность в регионах с холодным климатом.",
        u8"Крайне чувствителен к ультрафиолету и базовым санитарным протоколам."
    },
    {
        u8"Дерматотропный штамм, проникающий через поврежденные кожные покровы и вызывающий обширные воспаления.",
        u8"Долго сохраняется на поверхностях и создает устойчивые очаги в регионах с жарким влажным климатом.",
        u8"Уязвим для антисептической обработки, карантина контактных групп и ранней местной терапии."
    },
    {
        u8"ДНК-содержащий легочный штамм с медленным началом и тяжелым поражением нижних дыхательных путей.",
        u8"Способен долго оставаться незамеченным и резко ухудшать состояние пациентов после инкубационного периода.",
        u8"Чувствителен к массовому тестированию, фильтрации воздуха и раннему применению кислородной поддержки."
    },
    {
        u8"РНК-содержащий кожный штамм, вызывающий болезненные высыпания и быстро меняющий поверхностные белки.",
        u8"Легко распространяется при тесном контакте и снижает эффективность первичных профилактических мер.",
        u8"Плохо выживает вне организма и сдерживается защитной одеждой, дезинфекцией и ограничением контактов."
    }
};

Vector2 MeasureJournalText(Font font, const char* text, float size) {
    return MeasureTextEx(font, text, size, 0.0f);
}

void DrawJournalText(Font font, const char* text, Vector2 position, float size, Color color) {
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    DrawTextEx(font, text, position, size, 0.0f, color);
}

void DrawCenteredJournalText(Font font, const char* text, Rectangle bounds, float size, Color color) {
    Vector2 measured = MeasureJournalText(font, text, size);
    DrawJournalText(font, text,
                    {bounds.x + (bounds.width - measured.x) * 0.5f,
                     bounds.y + (bounds.height - measured.y) * 0.5f},
                    size, color);
}

float DrawWrappedJournalText(Font font, const char* text, Rectangle bounds, float size, Color color,
                             float lineGap = 5.0f) {
    std::string source(text ? text : "");
    std::string line;
    float y = bounds.y;
    size_t start = 0;

    while (start < source.size() && y <= bounds.y + bounds.height - size) {
        size_t end = source.find(' ', start);
        std::string word = source.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::string candidate = line.empty() ? word : line + " " + word;

        if (!line.empty() && MeasureJournalText(font, candidate.c_str(), size).x > bounds.width) {
            DrawJournalText(font, line.c_str(), {bounds.x, y}, size, color);
            y += size + lineGap;
            line = word;
        } else {
            line = candidate;
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (!line.empty() && y <= bounds.y + bounds.height - size) {
        DrawJournalText(font, line.c_str(), {bounds.x, y}, size, color);
        y += size + lineGap;
    }
    return y;
}

Rectangle JournalRect() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float width = std::min(1540.0f, sw - 72.0f);
    float height = std::min(880.0f, sh - 58.0f);
    return {std::round((sw - width) * 0.5f), std::round((sh - height) * 0.5f), width, height};
}

Rectangle LeftPage(Rectangle journal) {
    return {journal.x + 24.0f, journal.y + 72.0f, journal.width * 0.44f - 34.0f, journal.height - 96.0f};
}

Rectangle RightPage(Rectangle journal) {
    float x = journal.x + journal.width * 0.44f + 12.0f;
    return {x, journal.y + 72.0f, journal.x + journal.width - 24.0f - x, journal.height - 96.0f};
}

Rectangle CloseRect(Rectangle journal) {
    return {journal.x + journal.width - 56.0f, journal.y + 20.0f, 34.0f, 34.0f};
}

Rectangle EvidenceRect(Rectangle left, int index) {
    float gap = 10.0f;
    float width = (left.width - 48.0f - gap) * 0.5f;
    int column = index % 2;
    int row = index / 2;
    return {left.x + 24.0f + column * (width + gap), left.y + 82.0f + row * 66.0f, width, 54.0f};
}

Rectangle VirusRowRect(Rectangle left, int index) {
    return {left.x + 24.0f, left.y + 314.0f + index * 50.0f, left.width - 48.0f, 40.0f};
}

Rectangle CrossRect(Rectangle row) {
    return {row.x + row.width - 40.0f, row.y + 7.0f, 32.0f, 32.0f};
}

Rectangle ConfirmRect(Rectangle right) {
    return {right.x + 34.0f, right.y + right.height - 66.0f, right.width - 68.0f, 46.0f};
}

Rectangle JournalTabRect(Rectangle journal, int index) {
    return {journal.x + 430.0f + index * 164.0f, journal.y + 19.0f, 150.0f, 36.0f};
}

Rectangle EncyclopediaRowRect(Rectangle left, int index) {
    return {left.x + 24.0f, left.y + 126.0f + index * 88.0f, left.width - 48.0f, 72.0f};
}

Rectangle MethodCardRect(Rectangle left, int index) {
    return {left.x + 24.0f, left.y + 76.0f + index * 126.0f, left.width - 48.0f, 114.0f};
}

Rectangle MethodLaunchRect(Rectangle card) {
    return {card.x + card.width - 148.0f, card.y + card.height - 40.0f, 132.0f, 28.0f};
}

const char* SelectedSymptomLabel(int value) {
    switch (value) {
        case 0: return u8"Легочная";
        case 1: return u8"Кожная";
        case 2: return u8"Геморрагическая";
        default: return u8"Выберите улику";
    }
}

const char* SelectedPathogenLabel(int value) {
    switch (value) {
        case 0: return u8"РНК";
        case 1: return u8"ДНК";
        default: return u8"Выберите улику";
    }
}

const char* SelectedMorphologyLabel(int value) {
    switch (value) {
        case 0: return u8"Сферическая";
        case 1: return u8"Нитевидная";
        case 2: return u8"Кристаллическая";
        default: return u8"Выберите улику";
    }
}

const char* SelectedImmuneLabel(int value) {
    switch (value) {
        case 0: return u8"Сильная";
        case 1: return u8"Слабая";
        case 2: return u8"Отсутствует";
        default: return u8"Выберите улику";
    }
}

const char* SelectedThermalLabel(int value) {
    switch (value) {
        case 0: return u8"Холод";
        case 1: return u8"Тепло";
        case 2: return u8"Нейтральная";
        default: return u8"Выберите улику";
    }
}

int EvidenceOptionCount(int evidenceIndex) {
    return evidenceIndex == 1 ? 2 : 3;
}

const char* EvidenceOptionLabel(int evidenceIndex, int optionIndex) {
    if (optionIndex < 0) return u8"Не выбрано";
    switch (evidenceIndex) {
        case 0: return SelectedSymptomLabel(optionIndex);
        case 1: return SelectedPathogenLabel(optionIndex);
        case 2: return SelectedMorphologyLabel(optionIndex);
        case 3: return SelectedImmuneLabel(optionIndex);
        case 4: return SelectedThermalLabel(optionIndex);
    }
    return u8"Не выбрано";
}

Rectangle EvidenceOptionRect(Rectangle selector, int optionRow) {
    return {selector.x, selector.y + selector.height + 5.0f + optionRow * 34.0f,
            selector.width, 32.0f};
}

void DrawSelector(Font font, Rectangle bounds, const char* caption, const char* value,
                  bool open, bool selected) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color accent = open ? Color{105, 225, 245, 255}
                        : hovered ? Color{92, 214, 255, 255} : Color{69, 172, 214, 255};
    Color fill = open ? Color{18, 54, 70, 250}
                      : hovered ? Color{19, 43, 59, 245} : Color{13, 29, 42, 240};
    DrawRectangleRounded({bounds.x + 2.0f, bounds.y + 4.0f, bounds.width, bounds.height},
                         0.16f, 10, {0, 0, 0, 70});
    DrawRectangleRounded(bounds, 0.16f, 10, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 10, open ? 2.0f : 1.0f,
                                {accent.r, accent.g, accent.b, (unsigned char)(open ? 210 : 145)});
    DrawJournalText(font, caption, {bounds.x + 16.0f, bounds.y + 8.0f}, 12.5f, {139, 164, 181, 255});
    DrawJournalText(font, value, {bounds.x + 16.0f, bounds.y + 28.0f}, 14.5f,
                    selected ? WHITE : Color{132, 151, 163, 255});

    Vector2 arrow = {bounds.x + bounds.width - 21.0f, bounds.y + 30.0f};
    if (open) {
        DrawTriangle({arrow.x - 5.0f, arrow.y + 3.0f}, {arrow.x + 5.0f, arrow.y + 3.0f},
                     {arrow.x, arrow.y - 3.0f}, accent);
    } else {
        DrawTriangle({arrow.x - 5.0f, arrow.y - 3.0f}, {arrow.x, arrow.y + 3.0f},
                     {arrow.x + 5.0f, arrow.y - 3.0f}, accent);
    }
}

void DrawEvidenceDropdown(Font font, Rectangle left, int evidenceIndex, int selectedValue) {
    if (evidenceIndex < 0) return;
    Rectangle selector = EvidenceRect(left, evidenceIndex);
    int optionCount = EvidenceOptionCount(evidenceIndex);
    Vector2 mouse = GetMousePosition();

    Rectangle shadow = {selector.x + 4.0f, selector.y + selector.height + 9.0f,
                        selector.width, (optionCount + 1) * 34.0f};
    DrawRectangleRounded(shadow, 0.07f, 8, {0, 0, 0, 120});

    for (int row = 0; row <= optionCount; ++row) {
        int value = row - 1;
        Rectangle option = EvidenceOptionRect(selector, row);
        bool hovered = CheckCollisionPointRec(mouse, option);
        bool active = selectedValue == value;
        Color fill = active ? Color{24, 91, 108, 252}
                            : hovered ? Color{24, 52, 67, 252} : Color{10, 25, 37, 252};
        DrawRectangleRec(option, fill);
        DrawRectangleLinesEx(option, 1.0f,
                             active ? Color{101, 221, 240, 180} : Color{77, 102, 117, 70});
        if (active) {
            DrawCircleV({option.x + 16.0f, option.y + option.height * 0.5f}, 5.0f,
                        {105, 225, 245, 255});
        }
        DrawJournalText(font, EvidenceOptionLabel(evidenceIndex, value),
                        {option.x + 30.0f, option.y + 8.0f}, 13.0f,
                        active ? WHITE : Color{193, 208, 217, 255});
    }
}

bool VirusMatchesSelectedEvidence(const VirusDefinition& virus, int symptom, int pathogen,
                                  int morphology, int immuneResponse, int thermalStability) {
    if (symptom >= 0 && (int)virus.symptom != symptom) return false;
    if (pathogen >= 0 && (int)virus.pathogen != pathogen) return false;
    if (morphology >= 0 && (int)virus.morphology != morphology) return false;
    if (immuneResponse >= 0 && (int)virus.immuneResponse != immuneResponse) return false;
    if (thermalStability >= 0 && (int)virus.thermalStability != thermalStability) return false;
    return true;
}

void DrawSectionLabel(Font font, const char* text, Vector2 position, float width) {
    DrawJournalText(font, text, position, 17.0f, {166, 220, 239, 255});
    DrawRectangleRec({position.x, position.y + 28.0f, width, 1.0f}, {70, 195, 255, 72});
}

void DrawArticleField(Font font, const char* title, const char* body, Rectangle area) {
    DrawJournalText(font, title, {area.x, area.y}, 14.0f, {91, 203, 241, 255});
    DrawWrappedJournalText(font, body, {area.x, area.y + 25.0f, area.width, area.height - 25.0f},
                           15.0f, {215, 226, 234, 255}, 5.0f);
}

void DrawJournalTab(Font font, Rectangle bounds, const char* label, bool active) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Color accent = active ? Color{83, 211, 242, 255} : Color{105, 135, 151, 255};
    DrawRectangleRounded(bounds, 0.20f, 9,
                         active ? Color{24, 77, 94, 245}
                                : hovered ? Color{20, 42, 55, 240} : Color{13, 28, 39, 225});
    DrawRectangleRoundedLinesEx(bounds, 0.20f, 9, 1.0f,
                                {accent.r, accent.g, accent.b, (unsigned char)(active ? 170 : 75)});
    DrawCenteredJournalText(font, label, bounds, 13.0f, active ? WHITE : Color{175, 193, 204, 255});
}

void DrawMethodCard(Font font, Rectangle card, const char* index, const char* title,
                    const char* description, const char* evidence, const char* requirement,
                    bool unlocked, Color accent) {
    Vector2 mouse = GetMousePosition();
    Rectangle launch = MethodLaunchRect(card);
    bool launchHovered = unlocked && CheckCollisionPointRec(mouse, launch);

    DrawRectangleRounded(card, 0.055f, 12, unlocked ? Color{12, 34, 43, 244} : Color{18, 25, 34, 240});
    DrawRectangleRoundedLinesEx(card, 0.055f, 12, 1.0f,
                                unlocked ? Color{accent.r, accent.g, accent.b, 130}
                                         : Color{82, 98, 110, 90});
    DrawCircleV({card.x + 32.0f, card.y + 31.0f}, 17.0f,
                unlocked ? Color{accent.r, accent.g, accent.b, 210} : Color{46, 57, 66, 255});
    DrawCenteredJournalText(font, index, {card.x + 15.0f, card.y + 14.0f, 34.0f, 34.0f},
                            12.0f, unlocked ? Color{4, 23, 28, 255} : Color{145, 155, 163, 255});
    DrawJournalText(font, title, {card.x + 60.0f, card.y + 14.0f}, 16.0f,
                    unlocked ? WHITE : Color{136, 149, 158, 255});
    DrawJournalText(font, unlocked ? u8"МЕТОД ДОСТУПЕН" : u8"ОБОРУДОВАНИЕ НЕ УСТАНОВЛЕНО",
                    {card.x + 60.0f, card.y + 38.0f}, 10.0f,
                    unlocked ? accent : Color{154, 104, 108, 255});
    DrawWrappedJournalText(font, description,
                           {card.x + 16.0f, card.y + 61.0f, card.width - 180.0f, 38.0f},
                           10.5f, unlocked ? Color{203, 218, 226, 255} : Color{119, 132, 141, 255}, 2.0f);

    std::string output = std::string(u8"Результат: ") + evidence;
    DrawJournalText(font, output.c_str(), {card.x + 16.0f, card.y + 96.0f}, 9.5f,
                    unlocked ? Color{155, 206, 211, 255} : Color{104, 117, 126, 255});
    if (!unlocked) {
        DrawJournalText(font, requirement, {card.x + 180.0f, card.y + 39.0f}, 9.5f,
                        {203, 139, 109, 255});
    }

    DrawRectangleRounded(launch, 0.18f, 9,
                         unlocked ? (launchHovered ? Color{accent.r, accent.g, accent.b, 205}
                                                  : Color{accent.r, accent.g, accent.b, 120})
                                  : Color{34, 45, 53, 235});
    DrawRectangleRoundedLinesEx(launch, 0.18f, 9, 1.0f,
                                unlocked ? Color{accent.r, accent.g, accent.b, 180}
                                         : Color{75, 90, 101, 90});
    DrawCenteredJournalText(font, unlocked ? u8"ЗАПУСТИТЬ" : u8"ЗАКРЫТО",
                            launch, 10.5f, unlocked ? WHITE : Color{121, 135, 144, 255});
}

} // namespace

void VirologistJournal::Reset() {
    currentPage = 0;
    openEvidenceDropdown = -1;
    selectedSymptom = -1;
    selectedPathogen = -1;
    selectedMorphology = -1;
    selectedImmuneResponse = -1;
    selectedThermalStability = -1;
    selectedVirusIndex = -1;
    std::fill(std::begin(crossedOut), std::end(crossedOut), false);
}

JournalResult VirologistJournal::Update(const VirusDefinition& trueVirus,
                                         bool patientCardsUnlocked, bool sequencerUnlocked,
                                         bool microscopeUnlocked, bool serologyUnlocked,
                                         bool thermalUnlocked) {
    Rectangle journal = JournalRect();
    Rectangle left = LeftPage(journal);
    Rectangle right = RightPage(journal);

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_J)) return JournalResult::Closed;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return JournalResult::None;

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, CloseRect(journal))) return JournalResult::Closed;
    for (int page = 0; page < 3; ++page) {
        if (CheckCollisionPointRec(mouse, JournalTabRect(journal, page))) {
            currentPage = page;
            openEvidenceDropdown = -1;
            if (currentPage == 2 && selectedVirusIndex < 0) selectedVirusIndex = 0;
            return JournalResult::None;
        }
    }

    if (currentPage == 1) {
        Rectangle patientCard = MethodCardRect(left, 0);
        Rectangle sequencerCard = MethodCardRect(left, 1);
        Rectangle microscopeCard = MethodCardRect(left, 2);
        Rectangle serologyCard = MethodCardRect(left, 3);
        Rectangle thermalCard = MethodCardRect(left, 4);
        if (patientCardsUnlocked && CheckCollisionPointRec(mouse, MethodLaunchRect(patientCard))) {
            return JournalResult::LaunchPatientCards;
        }
        if (sequencerUnlocked && CheckCollisionPointRec(mouse, MethodLaunchRect(sequencerCard))) {
            return JournalResult::LaunchPathogenSequencer;
        }
        if (microscopeUnlocked && CheckCollisionPointRec(mouse, MethodLaunchRect(microscopeCard))) {
            return JournalResult::LaunchElectronMicroscope;
        }
        if (serologyUnlocked && CheckCollisionPointRec(mouse, MethodLaunchRect(serologyCard))) {
            return JournalResult::LaunchSerologyAssay;
        }
        if (thermalUnlocked && CheckCollisionPointRec(mouse, MethodLaunchRect(thermalCard))) {
            return JournalResult::LaunchThermalChamber;
        }
        return JournalResult::None;
    }

    if (currentPage == 2) {
        int virusCount = 0;
        GetVirusMatrix(&virusCount);
        for (int i = 0; i < virusCount; ++i) {
            if (CheckCollisionPointRec(mouse, EncyclopediaRowRect(left, i))) {
                selectedVirusIndex = i;
                return JournalResult::None;
            }
        }
        return JournalResult::None;
    }

    if (openEvidenceDropdown >= 0) {
        Rectangle selector = EvidenceRect(left, openEvidenceDropdown);
        int optionCount = EvidenceOptionCount(openEvidenceDropdown);
        for (int row = 0; row <= optionCount; ++row) {
            if (!CheckCollisionPointRec(mouse, EvidenceOptionRect(selector, row))) continue;
            int value = row - 1;
            switch (openEvidenceDropdown) {
                case 0: selectedSymptom = value; break;
                case 1: selectedPathogen = value; break;
                case 2: selectedMorphology = value; break;
                case 3: selectedImmuneResponse = value; break;
                case 4: selectedThermalStability = value; break;
            }
            openEvidenceDropdown = -1;
            return JournalResult::None;
        }
        if (CheckCollisionPointRec(mouse, selector)) {
            openEvidenceDropdown = -1;
            return JournalResult::None;
        }
        openEvidenceDropdown = -1;
        return JournalResult::None;
    }

    for (int evidence = 0; evidence < 5; ++evidence) {
        if (CheckCollisionPointRec(mouse, EvidenceRect(left, evidence))) {
            openEvidenceDropdown = evidence;
            return JournalResult::None;
        }
    }

    int virusCount = 0;
    const VirusDefinition* viruses = GetVirusMatrix(&virusCount);
    for (int i = 0; i < virusCount; ++i) {
        Rectangle row = VirusRowRect(left, i);
        Rectangle cross = CrossRect(row);
        if (CheckCollisionPointRec(mouse, cross)) {
            crossedOut[i] = !crossedOut[i];
            return JournalResult::None;
        }
        if (CheckCollisionPointRec(mouse, row)) {
            selectedVirusIndex = i;
            return JournalResult::None;
        }
    }

    if (selectedVirusIndex >= 0 && CheckCollisionPointRec(mouse, ConfirmRect(right))) {
        return std::strcmp(viruses[selectedVirusIndex].name, trueVirus.name) == 0
                   ? JournalResult::CorrectDiagnosis
                   : JournalResult::WrongDiagnosis;
    }

    return JournalResult::None;
}

void VirologistJournal::Draw(Font font, bool patientCardsUnlocked, bool sequencerUnlocked,
                             bool microscopeUnlocked, bool serologyUnlocked,
                             bool thermalUnlocked) const {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle journal = JournalRect();
    Rectangle left = LeftPage(journal);
    Rectangle right = RightPage(journal);
    Vector2 mouse = GetMousePosition();

    DrawRectangle(0, 0, (int)sw, (int)sh, {1, 5, 11, 218});
    DrawRectangleRounded({journal.x + 10.0f, journal.y + 14.0f, journal.width, journal.height},
                         0.025f, 18, {0, 0, 0, 120});
    DrawRectangleRounded(journal, 0.025f, 18, {7, 15, 25, 250});
    DrawRectangleRoundedLinesEx(journal, 0.025f, 18, 1.0f, {83, 184, 220, 105});

    DrawJournalText(font, u8"БЛОКНОТ ВИРУСОЛОГА", {journal.x + 28.0f, journal.y + 22.0f},
                    24.0f, {225, 242, 250, 255});
    DrawJournalTab(font, JournalTabRect(journal, 0), u8"ДЕДУКЦИЯ", currentPage == 0);
    DrawJournalTab(font, JournalTabRect(journal, 1), u8"МИНИ-ИГРЫ", currentPage == 1);
    DrawJournalTab(font, JournalTabRect(journal, 2), u8"ВИРУСЫ", currentPage == 2);

    Rectangle close = CloseRect(journal);
    bool closeHovered = CheckCollisionPointRec(mouse, close);
    DrawRectangleRounded(close, 0.25f, 8, closeHovered ? Color{95, 38, 48, 245} : Color{34, 31, 42, 235});
    DrawCenteredJournalText(font, "X", close, 17.0f, closeHovered ? Color{255, 155, 165, 255} : Color{194, 204, 214, 255});

    DrawRectangleRounded(left, 0.035f, 14, {11, 24, 36, 244});
    DrawRectangleRounded(right, 0.035f, 14, {12, 23, 34, 244});
    DrawRectangleRoundedLinesEx(left, 0.035f, 14, 1.0f, {70, 195, 255, 82});
    DrawRectangleRoundedLinesEx(right, 0.035f, 14, 1.0f, {70, 195, 255, 82});

    float spineX = left.x + left.width + 11.0f;
    DrawRectangleGradientH((int)(spineX - 8.0f), (int)left.y, 16, (int)left.height,
                           {0, 0, 0, 15}, {0, 0, 0, 100});

    if (currentPage == 1) {
        DrawJournalText(font, u8"ПОЛЕВЫЕ ИССЛЕДОВАНИЯ", {left.x + 24.0f, left.y + 22.0f}, 21.0f, WHITE);
        DrawJournalText(font, u8"Методы открываются через исследовательскую ветку лаборатории.",
                        {left.x + 24.0f, left.y + 53.0f}, 13.0f, {142, 167, 180, 255});

        DrawMethodCard(font, MethodCardRect(left, 0), "01", u8"Карточки пациентов",
                       u8"Изучите три истории болезни, сравните симптомы со справочником и выберите общую группу.",
                       u8"тип симптоматики", u8"Требуется: Частная клиника",
                       patientCardsUnlocked, {75, 194, 255, 255});
        DrawMethodCard(font, MethodCardRect(left, 1), "02", u8"Геномный секвенатор",
                       u8"Поймайте три участка сканером. Затем: U означает РНК, T означает ДНК.",
                       u8"РНК или ДНК", u8"Требуется: Геномный секвенатор",
                       sequencerUnlocked, {74, 218, 187, 255});
        DrawMethodCard(font, MethodCardRect(left, 2), "03", u8"Электронный микроскоп",
                       u8"Настройте три слоя фокуса и классифицируйте форму наблюдаемого вириона.",
                       u8"морфология вириона", u8"Требуется: Электронный микроскоп",
                       microscopeUnlocked, {194, 119, 255, 255});
        DrawMethodCard(font, MethodCardRect(left, 3), "04", u8"Серологический тест",
                       u8"Смешайте реагенты по протоколу и оцените агглютинацию.",
                       u8"иммунная реакция", u8"Требуется: Серологический центр",
                       serologyUnlocked, {255, 122, 170, 255});
        DrawMethodCard(font, MethodCardRect(left, 4), "05", u8"Термокамера",
                       u8"Стабилизируйте температуру и оцените выживаемость культуры.",
                       u8"термоустойчивость", u8"Требуется: Криолаборатория",
                       thermalUnlocked, {255, 155, 73, 255});

        DrawJournalText(font, u8"ПРОТОКОЛ РАССЛЕДОВАНИЯ", {right.x + 34.0f, right.y + 24.0f}, 21.0f, WHITE);
        DrawRectangleRec({right.x + 34.0f, right.y + 57.0f, right.width - 68.0f, 1.0f},
                         {70, 195, 255, 76});

        const char* steps[] = {
            u8"Развивайте исследовательскую ветку лаборатории.",
            u8"Запускайте открытые методы с левой страницы.",
            u8"Получайте результат анализа, но не автоматическую отметку.",
            u8"Вернитесь во вкладку «Дедукция» и внесите улику вручную.",
            u8"Исключайте штаммы и подтверждайте окончательный диагноз."
        };
        for (int i = 0; i < 5; ++i) {
            float y = right.y + 100.0f + i * 88.0f;
            Color stepAccent = i < 2 ? Color{74, 218, 187, 255} : Color{76, 190, 235, 255};
            DrawCircleV({right.x + 61.0f, y + 20.0f}, 19.0f, {stepAccent.r, stepAccent.g, stepAccent.b, 52});
            DrawCircleLines((int)(right.x + 61.0f), (int)(y + 20.0f), 19.0f, stepAccent);
            DrawCenteredJournalText(font, TextFormat("%d", i + 1),
                                    {right.x + 42.0f, y + 1.0f, 38.0f, 38.0f},
                                    14.0f, WHITE);
            DrawWrappedJournalText(font, steps[i],
                                   {right.x + 96.0f, y + 3.0f, right.width - 138.0f, 54.0f},
                                   15.0f, {207, 220, 228, 255}, 4.0f);
            if (i < 4) {
                DrawLineEx({right.x + 61.0f, y + 42.0f}, {right.x + 61.0f, y + 76.0f},
                           2.0f, {stepAccent.r, stepAccent.g, stepAccent.b, 55});
            }
        }

        Rectangle note = {right.x + 34.0f, right.y + right.height - 100.0f, right.width - 68.0f, 68.0f};
        DrawRectangleRounded(note, 0.12f, 9, {18, 35, 45, 235});
        DrawRectangleRoundedLinesEx(note, 0.12f, 9, 1.0f, {242, 201, 83, 100});
        DrawJournalText(font, u8"ВАЖНО", {note.x + 16.0f, note.y + 12.0f}, 12.0f, {242, 201, 83, 255});
        DrawJournalText(font, u8"Блокнот не заполняется сам. Ошибка анализа расходует время и ресурсы.",
                        {note.x + 16.0f, note.y + 37.0f}, 13.5f, {211, 220, 226, 255});
        return;
    }

    int virusCount = 0;
    const VirusDefinition* viruses = GetVirusMatrix(&virusCount);
    if (currentPage == 0) {
        DrawJournalText(font, u8"АНАЛИЗ И ДЕДУКЦИЯ", {left.x + 24.0f, left.y + 22.0f}, 21.0f, WHITE);
        DrawSectionLabel(font, u8"УЛИКИ", {left.x + 24.0f, left.y + 55.0f}, left.width - 48.0f);
        DrawSelector(font, EvidenceRect(left, 0), u8"СИМПТОМАТИКА", SelectedSymptomLabel(selectedSymptom),
                     openEvidenceDropdown == 0, selectedSymptom >= 0);
        DrawSelector(font, EvidenceRect(left, 1), u8"ПАТОГЕН", SelectedPathogenLabel(selectedPathogen),
                     openEvidenceDropdown == 1, selectedPathogen >= 0);
        DrawSelector(font, EvidenceRect(left, 2), u8"МОРФОЛОГИЯ", SelectedMorphologyLabel(selectedMorphology),
                     openEvidenceDropdown == 2, selectedMorphology >= 0);
        DrawSelector(font, EvidenceRect(left, 3), u8"ИММУННЫЙ ОТВЕТ", SelectedImmuneLabel(selectedImmuneResponse),
                     openEvidenceDropdown == 3, selectedImmuneResponse >= 0);
        DrawSelector(font, EvidenceRect(left, 4), u8"ТЕРМОСТАБИЛЬНОСТЬ", SelectedThermalLabel(selectedThermalStability),
                     openEvidenceDropdown == 4, selectedThermalStability >= 0);
        DrawSectionLabel(font, u8"БАЗА ШТАММОВ", {left.x + 24.0f, left.y + 280.0f}, left.width - 48.0f);
    } else {
        DrawJournalText(font, u8"ЭНЦИКЛОПЕДИЯ ШТАММОВ", {left.x + 24.0f, left.y + 22.0f}, 21.0f, WHITE);
        DrawJournalText(font, u8"Выберите вирус, чтобы открыть полное медицинское досье.",
                        {left.x + 24.0f, left.y + 53.0f}, 13.0f, {142, 167, 180, 255});
        DrawSectionLabel(font, u8"ИЗВЕСТНЫЕ ВИРУСЫ", {left.x + 24.0f, left.y + 82.0f}, left.width - 48.0f);
    }

    for (int i = 0; i < virusCount; ++i) {
        Rectangle row = currentPage == 0 ? VirusRowRect(left, i) : EncyclopediaRowRect(left, i);
        Rectangle cross = CrossRect(row);
        bool selected = selectedVirusIndex == i;
        bool rowHovered = CheckCollisionPointRec(mouse, row);
        bool evidenceMismatch = currentPage == 0 &&
                                !VirusMatchesSelectedEvidence(
                                    viruses[i], selectedSymptom, selectedPathogen,
                                    selectedMorphology, selectedImmuneResponse,
                                    selectedThermalStability);
        bool visuallyCrossed = currentPage == 0 && (crossedOut[i] || evidenceMismatch);
        Color rowColor = selected ? Color{24, 66, 83, 245}
                                  : rowHovered ? Color{20, 43, 57, 235} : Color{14, 30, 43, 225};
        if (evidenceMismatch && !selected) rowColor = {18, 25, 33, 220};
        DrawRectangleRounded(row, 0.16f, 9, rowColor);
        DrawRectangleRoundedLinesEx(row, 0.16f, 9, 1.0f,
                                    selected ? Color{76, 207, 250, 190} : Color{83, 113, 132, 85});

        Color textColor = visuallyCrossed
                              ? Color{96, 111, 121, 255}
                              : Color{224, 235, 242, 255};
        DrawJournalText(font, viruses[i].name,
                        {row.x + 16.0f, row.y + (currentPage == 0 ? 13.0f : 12.0f)},
                        17.0f, textColor);
        if (currentPage == 0) {
            if (visuallyCrossed) {
                float width = MeasureJournalText(font, viruses[i].name, 17.0f).x;
                DrawLineEx({row.x + 13.0f, row.y + 23.0f}, {row.x + 19.0f + width, row.y + 23.0f},
                           2.0f, evidenceMismatch ? Color{111, 130, 143, 225}
                                                  : Color{153, 88, 94, 235});
            }

            bool crossHovered = CheckCollisionPointRec(mouse, cross);
            DrawRectangleRounded(cross, 0.22f, 7,
                                 crossedOut[i] ? Color{102, 43, 52, 245}
                                               : evidenceMismatch ? Color{35, 45, 54, 235}
                                               : crossHovered ? Color{57, 49, 58, 245} : Color{29, 39, 49, 235});
            DrawCenteredJournalText(font, "X", cross, 14.0f,
                                    crossedOut[i] ? Color{255, 139, 148, 255}
                                                  : evidenceMismatch ? Color{104, 121, 132, 255}
                                                                     : Color{166, 180, 190, 255});
        } else {
            std::string evidenceLine = std::string(SymptomEvidenceName(viruses[i].symptom)) +
                                       "  •  " + PathogenEvidenceName(viruses[i].pathogen) +
                                       "  •  " + MorphologyEvidenceName(viruses[i].morphology);
            DrawJournalText(font, evidenceLine.c_str(), {row.x + 16.0f, row.y + 42.0f},
                            13.0f, selected ? Color{112, 218, 235, 255} : Color{139, 164, 177, 255});
            DrawJournalText(font, u8"ОТКРЫТЬ  ›", {row.x + row.width - 104.0f, row.y + 27.0f},
                            11.5f, selected ? Color{108, 216, 238, 255} : Color{105, 126, 138, 255});
        }
    }

    if (currentPage == 0 && openEvidenceDropdown >= 0) {
        int selectedValue = -1;
        switch (openEvidenceDropdown) {
            case 0: selectedValue = selectedSymptom; break;
            case 1: selectedValue = selectedPathogen; break;
            case 2: selectedValue = selectedMorphology; break;
            case 3: selectedValue = selectedImmuneResponse; break;
            case 4: selectedValue = selectedThermalStability; break;
        }
        DrawEvidenceDropdown(font, left, openEvidenceDropdown, selectedValue);
    }

    DrawJournalText(font, currentPage == 2 ? u8"МЕДИЦИНСКОЕ ДОСЬЕ" : u8"МЕДИЦИНСКИЙ СПРАВОЧНИК",
                    {right.x + 34.0f, right.y + 24.0f}, 21.0f, WHITE);
    DrawRectangleRec({right.x + 34.0f, right.y + 57.0f, right.width - 68.0f, 1.0f}, {70, 195, 255, 76});

    if (selectedVirusIndex < 0 || selectedVirusIndex >= virusCount) {
        DrawCenteredJournalText(font, u8"Выберите штамм для изучения",
                                {right.x + 34.0f, right.y + 110.0f, right.width - 68.0f, right.height - 200.0f},
                                22.0f, {111, 137, 153, 255});
        DrawJournalText(font, u8"Данные справочника появятся на этой странице.",
                        {right.x + 52.0f, right.y + right.height - 92.0f}, 13.0f, {85, 107, 121, 255});
        return;
    }

    const VirusDefinition& virus = viruses[selectedVirusIndex];
    const VirusArticle& article = VIRUS_ARTICLES[selectedVirusIndex];
    DrawJournalText(font, virus.name, {right.x + 34.0f, right.y + 82.0f}, 34.0f, {91, 211, 247, 255});
    DrawJournalText(font, u8"КЛАССИФИКАЦИЯ / АКТИВНОЕ ДОСЬЕ",
                    {right.x + 36.0f, right.y + 124.0f}, 11.5f, {107, 136, 153, 255});

    float fieldX = right.x + 36.0f;
    float fieldWidth = right.width - 72.0f;
    DrawArticleField(font, u8"ОПИСАНИЕ", article.description,
                     {fieldX, right.y + 168.0f, fieldWidth, 100.0f});
    DrawArticleField(font, u8"ОСОБЫЕ СВОЙСТВА", article.properties,
                     {fieldX, right.y + 284.0f, fieldWidth, 100.0f});
    DrawArticleField(font, u8"СЛАБЫЕ СТОРОНЫ", article.weaknesses,
                     {fieldX, right.y + 400.0f, fieldWidth, 100.0f});

    Rectangle evidence = {fieldX, right.y + right.height - 142.0f, fieldWidth, 48.0f};
    DrawRectangleRounded(evidence, 0.15f, 9, {10, 34, 45, 240});
    DrawRectangleRoundedLinesEx(evidence, 0.15f, 9, 1.0f, {69, 187, 222, 105});
    std::string evidenceText = std::string(u8"Улики штамма: ") +
                               SymptomEvidenceName(virus.symptom) + ", " +
                               PathogenEvidenceName(virus.pathogen) + ", " +
                               MorphologyEvidenceName(virus.morphology) + ", " +
                               ImmuneResponseEvidenceName(virus.immuneResponse) + ", " +
                               ThermalStabilityEvidenceName(virus.thermalStability);
    DrawJournalText(font, evidenceText.c_str(), {evidence.x + 16.0f, evidence.y + 15.0f},
                    14.0f, {165, 211, 228, 255});

    if (currentPage == 0) {
        Rectangle confirm = ConfirmRect(right);
        bool confirmHovered = CheckCollisionPointRec(mouse, confirm);
        DrawRectangleRounded(confirm, 0.16f, 10,
                             confirmHovered ? Color{39, 139, 157, 255} : Color{24, 103, 124, 255});
        DrawRectangleRoundedLinesEx(confirm, 0.16f, 10, 1.0f, {111, 227, 244, 170});
        DrawCenteredJournalText(font, u8"ПОДТВЕРДИТЬ ДИАГНОЗ", confirm, 16.0f, WHITE);
    } else {
        DrawJournalText(font, u8"Для постановки диагноза вернитесь во вкладку «Дедукция».",
                        {right.x + 36.0f, right.y + right.height - 42.0f}, 12.5f,
                        {122, 147, 159, 255});
    }
}
