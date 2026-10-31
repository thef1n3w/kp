#pragma once

// Shared types and interfaces for the nine coursework modules.
#include "raylib.h"
#include <cstdint>
#include <string>
#include <vector>

// ---- Difficulty ----

enum Difficulty {
    DIFFICULTY_HOPE = 0,
    DIFFICULTY_CONTROL,
    DIFFICULTY_CRISIS,
    DIFFICULTY_COLLAPSE,
    DIFFICULTY_COUNT
};

struct DifficultyProfile {
    const char* name;
    const char* subtitle;
    const char* description;
    const char* factors[4];
    float spreadMultiplier;
    float cureMultiplier;
    float mortalityMultiplier;
    float transportTransmissionMultiplier;
    float mutationChancePerDay;
    float mutationMinimumInfectedShare;
    float mutationSpreadBoost;
    float mutationMortalityBoost;
    float scienceTrust;
    float internationalCooperation;
    float publicCompliance;
    int startingResources;
    float actionCostMultiplier;
};

const DifficultyProfile& GetDifficultyProfile(Difficulty difficulty);

// ---- PatientCardGame ----

enum class SymptomEvidence {
    Pulmonary = 0,
    Cutaneous,
    Hemorrhagic
};

enum class PathogenEvidence {
    RNA = 0,
    DNA
};

enum class MorphologyEvidence {
    Spherical = 0,
    Filamentous,
    Crystalline
};

enum class ImmuneResponseEvidence {
    Strong = 0,
    Weak,
    Absent
};

enum class ThermalStabilityEvidence {
    Cold = 0,
    Heat,
    Neutral
};

struct VirusDefinition {
    const char* name;
    SymptomEvidence symptom;
    PathogenEvidence pathogen;
    MorphologyEvidence morphology;
    ImmuneResponseEvidence immuneResponse;
    ThermalStabilityEvidence thermalStability;
};

enum class PatientCardResult {
    None = 0,
    Cancelled,
    Correct,
    Wrong
};

class PatientCardGame {
public:
    void SelectRandomVirus();
    void Start();
    PatientCardResult Update();
    void Draw() const;

    bool IsActive() const { return active; }
    bool HasActiveVirus() const { return activeVirusIndex >= 0; }
    SymptomEvidence GetActiveSymptom() const;
    const VirusDefinition& GetActiveVirus() const;
    const char* GetLastChosenSymptomName() const { return lastChosenSymptomName.c_str(); }
    const char* GetActiveSymptomName() const;
    static void LoadPortraitAssets();
    static void UnloadPortraitAssets();

private:
    int activeVirusIndex = -1;
    bool active = false;
    std::vector<std::string> patientCards;
    std::vector<int> portraitIndices;
    std::string lastChosenSymptomName;

    void GeneratePatientCards();
    PatientCardResult ChooseSymptom(SymptomEvidence symptom);
};

const VirusDefinition* GetVirusMatrix(int* count);
const char* SymptomEvidenceName(SymptomEvidence symptom);
const char* PathogenEvidenceName(PathogenEvidence pathogen);
const char* MorphologyEvidenceName(MorphologyEvidence morphology);
const char* ImmuneResponseEvidenceName(ImmuneResponseEvidence response);
const char* ThermalStabilityEvidenceName(ThermalStabilityEvidence stability);

// ---- PathogenSequencerGame ----

enum class PathogenSequencerResult {
    None = 0,
    Cancelled,
    Correct,
    Wrong
};

class PathogenSequencerGame {
public:
    void Start(PathogenEvidence target);
    PathogenSequencerResult Update(float delta);
    void Draw(Font font) const;

    bool IsActive() const { return active; }
    const char* GetDetectedMarkerName() const;

private:
    PathogenEvidence targetPathogen = PathogenEvidence::RNA;
    bool active = false;
    int capturedFragments = 0;
    int errors = 0;
    float scannerPosition = 0.0f;
    float targetPositions[3] = {0.24f, 0.58f, 0.81f};

    bool IsInterpretationStage() const { return capturedFragments >= 3; }
    bool TryCapture();
    PathogenSequencerResult ChoosePathogen(PathogenEvidence choice);
};

// ---- ElectronMicroscopeGame ----

enum class ElectronMicroscopeResult {
    None = 0,
    Cancelled,
    Correct,
    Wrong
};

class ElectronMicroscopeGame {
public:
    void Start(MorphologyEvidence target);
    ElectronMicroscopeResult Update(float delta);
    void Draw(Font font) const;

    bool IsActive() const { return active; }

private:
    MorphologyEvidence targetMorphology = MorphologyEvidence::Spherical;
    bool active = false;
    int focusedLayers = 0;
    int errors = 0;
    float focusPosition = 0.0f;
    float focusDirection = 1.0f;
    float targetFocus[3] = {0.25f, 0.62f, 0.43f};

    bool IsClassificationStage() const { return focusedLayers >= 3; }
    void TryLockFocus();
    ElectronMicroscopeResult ChooseMorphology(MorphologyEvidence choice);
};

// ---- SerologyAssayGame ----

enum class SerologyAssayResult { None = 0, Cancelled, Correct, Wrong };

class SerologyAssayGame {
public:
    void Start(ImmuneResponseEvidence target);
    SerologyAssayResult Update();
    void Draw(Font font) const;
    bool IsActive() const { return active; }

private:
    ImmuneResponseEvidence targetResponse = ImmuneResponseEvidence::Strong;
    bool active = false;
    int protocolStep = 0;
    int errors = 0;
    int reagentOrder[3] = {0, 1, 2};

    bool IsResultStage() const { return protocolStep >= 3; }
    SerologyAssayResult Choose(ImmuneResponseEvidence choice);
};

// ---- ThermalChamberGame ----

enum class ThermalChamberResult { None = 0, Cancelled, Correct, Wrong };

class ThermalChamberGame {
public:
    void Start(ThermalStabilityEvidence target);
    ThermalChamberResult Update(float delta);
    void Draw(Font font) const;
    bool IsActive() const { return active; }

private:
    ThermalStabilityEvidence targetStability = ThermalStabilityEvidence::Neutral;
    bool active = false;
    float temperature = 20.0f;
    float exposure = 0.0f;
    int errors = 0;
    bool IsResultStage() const { return exposure >= 3.0f; }
    ThermalChamberResult Choose(ThermalStabilityEvidence choice);
};

// ---- VirologistJournal ----

enum class JournalResult {
    None = 0,
    Closed,
    LaunchPatientCards,
    LaunchPathogenSequencer,
    LaunchElectronMicroscope,
    LaunchSerologyAssay,
    LaunchThermalChamber,
    CorrectDiagnosis,
    WrongDiagnosis
};

class VirologistJournal {
public:
    void Reset();
    JournalResult Update(const VirusDefinition& trueVirus, bool patientCardsUnlocked,
                         bool sequencerUnlocked, bool microscopeUnlocked,
                         bool serologyUnlocked, bool thermalUnlocked);
    void Draw(Font font, bool patientCardsUnlocked, bool sequencerUnlocked,
              bool microscopeUnlocked, bool serologyUnlocked, bool thermalUnlocked) const;

    int GetSelectedVirusIndex() const { return selectedVirusIndex; }

private:
    int currentPage = 0;
    int openEvidenceDropdown = -1;
    int selectedSymptom = -1;
    int selectedPathogen = -1;
    int selectedMorphology = -1;
    int selectedImmuneResponse = -1;
    int selectedThermalStability = -1;
    int selectedVirusIndex = -1;
    bool crossedOut[5] = {false};
};

// ---- TransportHub ----

struct Region;

enum TransportHubType {
    HUB_AIRPORT = 0,
    HUB_PORT = 1
};

struct TransportHub {
    TransportHubType type = HUB_AIRPORT;
    Vector2 mapPosition = {0.0f, 0.0f};
    bool closed = false;
    char name[96] = {0};
};

const char* TransportHubTypeToString(TransportHubType type);
TransportHubType TransportHubTypeFromString(const char* text);
void AddTransportHub(Region& region, TransportHubType type, Vector2 mapPosition, const char* name, bool closed = false);
int CountTransportHubs(const Region& region, TransportHubType type, bool onlyOpen = false);
int PickRandomTransportHubIndex(const Region& region, TransportHubType type, bool onlyOpen = true);
int CountRegionsWithOpenHub(const std::vector<Region>& regions, TransportHubType type);
bool LoadTransportHubsFromJSON(std::vector<Region>& regions, const char* path);
bool SaveTransportHubsToJSON(const std::vector<Region>& regions, const char* path);

// ---- Region ----

struct Region {
    char name[128];
    Vector2 mapPosition;
    float infectionRate;
    float cureProgress;
    int population;
    int infected;
    int dead;
    bool isSelected;
    Color overlayColor;
    std::vector<TransportHub> transportHubs;
};

bool LoadRegionsFromJSON(std::vector<Region>& regions, const char* path);
void InitializeRegions(std::vector<Region>& regions);
void UpdateRegionOverlayColor(Region& region);
void UpdateAllRegionOverlayColors(std::vector<Region>& regions);
void SimulateRegionDay(Region& region, float spreadMultiplier, float cureMultiplier, float mortalityMultiplier, bool vaccineReady);
float GetRegionRadius(const Region& region, float screenScale);
int GetRegionAtScreenPosition(const std::vector<Region>& regions, Vector2 mouse, int screenWidth, int screenHeight);
long long GetTotalPopulationPeople(const std::vector<Region>& regions);
long long GetTotalInfected(const std::vector<Region>& regions);
long long GetTotalDead(const std::vector<Region>& regions);
bool AreAllRegionsCured(const std::vector<Region>& regions);

// ---- Vehicle ----

struct Vehicle {
    Vector2 position;
    Vector2 origin;
    Vector2 destination;
    float progress;
    float speed;
    bool active;
    bool isPlane;
    bool carryingVirus;
    int fromRegion;
    int toRegion;
    int fromHub;
    int toHub;
    std::vector<Vector2> route;
    int routeSegment;
};

void InitializeVehiclePool(std::vector<Vehicle>& vehicles, const std::vector<Region>& regions, const std::vector<std::vector<Vector2>>& landPolygons);
void UpdateVehicles(std::vector<Vehicle>& vehicles, std::vector<Region>& regions, const std::vector<std::vector<Vector2>>& landPolygons, float delta, int speedMultiplier, float transmissionMultiplier);
void DrawVehicles(const std::vector<Vehicle>& vehicles, Texture2D planeIcon, bool planeIconLoaded, Texture2D shipIcon, bool shipIconLoaded, Rectangle mapRect);

// ---- GameState ----

struct AVFormatContext;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct SwsContext;

enum GameScreen {
    SCREEN_BOOT,
    SCREEN_MENU,
    SCREEN_SETTINGS,
    SCREEN_DIFFICULTY,
    SCREEN_GAMEPLAY,
    SCREEN_HOWTOPLAY,
    SCREEN_LOAD
};

struct VideoPlayer {
    bool ready = false;
    int width = 0;
    int height = 0;
    double frameTime = 1.0 / 30.0;
    double accumulator = 0.0;
    Texture2D texture = {0};
    unsigned char* pixels = nullptr;
    int videoStream = -1;
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbaFrame = nullptr;
    SwsContext* swsCtx = nullptr;
};

struct MenuState {
    Texture2D logoTexture = {0};
    bool logoLoaded = false;
    VideoPlayer backgroundVideo;
};

struct SettingsState {
    char labName[64] = "AEGIS LAB";
    int defaultSpeedMultiplier = 1;
};

struct Notification {
    char text[192] = {0};
    float age = 0.0f;
    float duration = 6.0f;
    Color accent = {70, 195, 255, 255};
};

struct ModalAlert {
    bool active = false;
    bool previousPaused = false;
    bool closing = false;
    float animation = 0.0f;
    char title[96] = {0};
    char body[256] = {0};
    Color accent = {70, 195, 255, 255};
};

enum LabStage {
    LAB_STAGE_SAMPLE = 0,
    LAB_STAGE_RESEARCH,
    LAB_STAGE_TRIALS,
    LAB_STAGE_PRODUCTION,
    LAB_STAGE_DISTRIBUTION,
    LAB_STAGE_COMPLETE,
    LAB_STAGE_COUNT
};

enum LabUpgradeBranch {
    LAB_BRANCH_RESEARCH = 0,
    LAB_BRANCH_FUNDING,
    LAB_BRANCH_PUBLIC,
    LAB_BRANCH_COOPERATION,
    LAB_BRANCH_COUNT
};

constexpr int LAB_UPGRADES_PER_BRANCH = 3;
constexpr int LAB_UPGRADE_COUNT = LAB_BRANCH_COUNT * LAB_UPGRADES_PER_BRANCH;

struct LabState {
    int resources = 0;
    int boostCost = 0;
    float stageProgress = 0.0f;
    float overallProgress = 0.0f;
    LabStage stage = LAB_STAGE_SAMPLE;
    bool canBoost = true;
    bool menuOpen = false;
    bool previousPaused = false;
    int selectedUpgrade = 0;
    LabUpgradeBranch activeBranch = LAB_BRANCH_RESEARCH;
    int trust = 12;
    bool upgrades[LAB_UPGRADE_COUNT] = {false};
};

struct ResourceMarker {
    bool active = false;
    int regionIndex = -1;
    int reward = 0;
    float age = 0.0f;
    float lifetime = 5.0f;
};

struct GameplayState {
    Difficulty difficulty = DIFFICULTY_CONTROL;
    Texture2D mapTexture = {0};
    Texture2D planeIcon = {0};
    Texture2D shipIcon = {0};
    Texture2D airportIcon = {0};
    Texture2D portIcon = {0};
    bool mapLoaded = false;
    bool planeIconLoaded = false;
    bool shipIconLoaded = false;
    bool airportIconLoaded = false;
    bool portIconLoaded = false;

    // Country border selection via color mask
    Image    maskImage         = {0};
    bool     maskImageLoaded   = false;
    Image    borderImage       = {0};   // RGBA overlay updated on hover/select change
    Texture2D borderTexture    = {0};
    bool     borderTexLoaded   = false;
    std::vector<std::vector<int>> regionBorderPixels; // flat pixel indices per region
    int      borderLastHovered  = -2;   // sentinel so first frame triggers update
    int      borderLastSelected = -2;

    // User-drawn polygons (normalized 0..1) loaded from regions_drawn.json
    std::vector<std::vector<Vector2>> regionPolygons;
    std::vector<std::vector<Vector2>> seaObstacles;
    bool     polygonsLoaded = false;

    std::vector<Region> regions;
    std::vector<Vehicle> vehicles;
    int selectedRegion = -1;
    int hoveredRegion = -1;
    int day = 25;
    int month = 5;
    int year = 2026;
    float dayAccumulator = 0.0f;
    float newsOffset = 0.0f;
    int newsIndex = 0;
    float notificationTimer = 0.0f;
    std::vector<Notification> notifications;
    ModalAlert modalAlert;
    PatientCardGame patientCardGame;
    bool patientCardPreviousPaused = false;
    PathogenSequencerGame pathogenSequencerGame;
    ElectronMicroscopeGame electronMicroscopeGame;
    SerologyAssayGame serologyAssayGame;
    ThermalChamberGame thermalChamberGame;
    VirologistJournal journal;
    bool showJournal = false;
    bool journalPreviousPaused = false;
    bool diagnosisConfirmed = false;
    LabState lab;
    ResourceMarker resourceMarker;
    std::vector<float> observedInfectionRates;
    std::vector<unsigned char> infectionRewardMilestones;
    int labRegionIndex = -1;
    bool labPlacementActive = false;
    unsigned int milestoneAlerts = 0;
    int speedMultiplier = 1;
    bool paused = false;
    bool pauseMenuOpen = false;
    bool gameWon = false;
    bool gameLost = false;
    bool virusMutationTriggered = false;
    int debugEditorMode = 0; // 0 countries, 1 transport
    bool debugShowCoords = false;
    bool debugDrawMode = false;
    bool debugTypingName = false;
    int debugTransportMode = 0; // 0 airport, 1 port
    int debugSelectedHub = -1;
    std::vector<Vector2> debugPolygon;
    char debugRegionName[64] = {0};
    int debugNameLen = 0;
};

struct GameState {
    GameScreen currentScreen = SCREEN_BOOT;
    bool shouldExit = false;
    Font font = {0};
    bool fontLoaded = false;
    MenuState menu;
    SettingsState settings;
    GameplayState gameplay;
};

extern GameState gGameState;

// ---- UI ----

float GetUIScale();
Color LerpColor(Color a, Color b, float t);
std::string ResourcePath(const char* fileName);
bool LoadUIFont();
void UnloadUIFont();
Vector2 MeasureTextUI(const char* text, float fontSize, float spacing = 0.0f);
void DrawTextUI(const char* text, Vector2 position, float fontSize, Color color, float spacing = 0.0f);
void DrawCenteredTextUI(const char* text, Rectangle bounds, float fontSize, Color color);
void DrawCapsuleGradient(Rectangle bounds, Color top, Color bottom, Color border, Color shadow);
void DrawVignette(int screenWidth, int screenHeight, unsigned char alpha);
std::string FormatNumber(long long value);

// ---- LoadingScreen ----

void InitLoadingScreen();
void UpdateLoadingScreen(float delta);
void DrawLoadingScreen();
bool AreStartupAssetsLoaded();

// ---- Menu ----

void UpdateMenuBackground(float delta);
void LoadMenuAssets();
void UnloadMenuAssets();
void UpdateMenu(float delta);
void DrawMenu();

// ---- DifficultyScreen ----

void PrepareDifficultyScreen();
void UpdateDifficultyScreen(float delta);
void DrawDifficultyScreen();

// ---- SettingsScreen ----

void LoadAppSettings();
void SaveAppSettings();
const char* GetLabDisplayName();
int GetDefaultGameplaySpeed();

void PrepareSettingsScreen();
void UpdateSettingsScreen(float delta);
void DrawSettingsScreen();

// ---- HowToPlayScreen ----

void PrepareHowToPlayScreen();
void UpdateHowToPlayScreen(float delta);
void DrawHowToPlayScreen();

// ---- Gameplay ----

void LoadGameplayAssets();
void UnloadGameplayAssets();
void ResetGameplaySession();
void UpdateGameplay(float delta);
void DrawGameplay();
