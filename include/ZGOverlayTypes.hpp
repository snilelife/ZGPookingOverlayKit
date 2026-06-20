#pragma once

#include <cstdint>
#include <vector>

namespace zg {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct Color {
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;
    double a = 1.0;
};

struct Ball {
    Point center;
    double radius = 12.0;
    int number = -1;
    bool cue = false;
};

struct GuideLine {
    bool valid = false;
    Point start;
    Point end;
};

enum class ScanRoute : std::uint8_t {
    AutoHybrid = 0,
    GuideLock = 1,
    BallGeometry = 2,
    CornerLock = 3
};

enum class PredictionStyle : std::uint8_t {
    Simple = 0,
    Advanced = 1,
    ProVideo = 2
};

enum class ShotMode : std::uint8_t {
    Auto = 0,
    LongShot = 1,
    CaromShot = 2,
    BankShot = 3
};

enum class LineRole : std::uint8_t {
    Unknown = 0,
    CueGuide = 1,
    ObjectPath = 2,
    GhostContact = 3,
    CaromPath = 4,
    BankPath = 5,
    BouncePath = 6,
    CollisionWarning = 7,
    CenterLine = 8
};

struct Settings {
    bool predictionEnabled = false;
    bool cuePredictionEnabled = true;
    bool pocketPredictionEnabled = true;
    bool bankPredictionEnabled = true;
    bool caromPredictionEnabled = true;
    bool ladderGuideEnabled = true;
    bool collisionWarningEnabled = true;
    bool pocketHeatEnabled = true;
    bool fourLinePredictionEnabled = true;
    bool hiddenLineRecordingEnabled = true;
    bool manualPocket = false;
    bool showSideLines = true;
    bool showDetectedBalls = true;
    bool showGhostBall = true;
    double lineLength = 1.10;
    int maxBounces = 4;
    int selectedPocket = 1;
    ScanRoute scanRoute = ScanRoute::AutoHybrid;
    ShotMode shotMode = ShotMode::Auto;
    PredictionStyle predictionStyle = PredictionStyle::Advanced;
};

struct GameState {
    Rect table;
    Point cueBall;
    bool hasCueBall = false;
    GuideLine guide;
    std::vector<Ball> balls;
};

struct Line {
    Point start;
    Point end;
    Color color;
    double width = 2.0;
    LineRole role = LineRole::Unknown;
};

struct Circle {
    Point center;
    double radius = 10.0;
    Color color;
    double width = 2.0;
};

struct Result {
    bool valid = false;
    const char *routeName = "unknown";
    const char *shotModeName = "unknown";
    const char *styleName = "unknown";
    Rect table;
    int selectedPocket = 0;
    std::vector<Line> lines;
    std::vector<Line> hiddenLines;
    std::vector<Circle> circles;
};

} // namespace zg
