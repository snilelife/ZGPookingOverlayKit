#pragma once

#include "ZGOverlayTypes.hpp"

#include <cstddef>

namespace zg {

class PredictionEngine {
public:
    Result compute(const GameState &state, const Settings &settings) const;

private:
    struct ShotChoice {
        Point cue;
        Point object;
        Point pocket;
        Point ghost;
        Point objectTarget;
        bool hasObjectTarget = false;
        std::vector<Point> bankGuide;
        double score = 0.0;
        int pocketIndex = 0;
        double ballRadius = 12.0;
    };

    static const char *routeName(ScanRoute route);
    static const char *shotModeName(ShotMode mode);
    static const char *styleName(PredictionStyle style);
    static std::vector<Point> pocketPoints(const Rect &table);
    static Point nearestPocket(const Point &point, const std::vector<Point> &pockets);
    static Point pointToward(const Point &start, const Point &end, double distance);
    static Point clamp(const Point &point, const Rect &rect);
    static double distance(const Point &a, const Point &b);
    static double cross(const Point &a, const Point &b);
    static double dot(const Point &a, const Point &b);
    static Line makeLine(const Point &start, const Point &end, Color color, double width, LineRole role = LineRole::Unknown);
    static Circle makeCircle(const Point &center, double radius, Color color, double width);
    static void addStyledLine(Result &result, const Point &start, const Point &end, Color color, double width, PredictionStyle style, LineRole role = LineRole::Unknown);
    static void addLadderLine(Result &result, const Point &start, const Point &end, Color color, double width, PredictionStyle style, LineRole role = LineRole::CueGuide);
    static void addBankGuideSegments(Result &result, const std::vector<Point> &guide, Color color, double width, PredictionStyle style, std::size_t startIndex);
    static void recordBankGuideSegments(std::vector<Line> &lines, const std::vector<Point> &guide, Color color, double width, std::size_t startIndex);
    static void addFourLinePack(Result &result, const ShotChoice &choice, const GameState &state, const Settings &settings);
    static void recordHiddenLines(Result &result, const ShotChoice &choice, const GameState &state, const Settings &settings);
    static std::vector<Point> bouncePath(const Point &start, const Point &target, const Rect &table, int count);
    static std::vector<Point> cueAfterHitPath(const ShotChoice &choice, const Rect &table, int count);
    static void addCollisionWarnings(Result &result, const ShotChoice &choice, const GameState &state, PredictionStyle style);
    static void addCaromTargets(Result &result, const ShotChoice &choice, const GameState &state, PredictionStyle style);
    static std::vector<std::vector<Point>> bankGuides(const Point &object, const Point &pocket, const Rect &table);
    static bool railGuide(const Point &object, const Point &pocket, const Rect &table, int rail, std::vector<Point> &out);
    static bool railSequenceGuide(const Point &object, const Point &pocket, const Rect &table, const std::vector<int> &rails, std::vector<Point> &out);
    static double guideLength(const std::vector<Point> &guide);
    static double pathBlockPenalty(const Point &start, const Point &end, const GameState &state, const Ball *ignoreA, const Ball *ignoreB, double clearanceScale);
    static double guideBlockPenalty(const std::vector<Point> &guide, const GameState &state, const Ball *ignoreBall);
    static Point shotObjectTarget(const ShotChoice &choice);
    static Point ghostForTarget(const Point &object, const Point &target, double radius);
    static ShotChoice chooseGuidedShot(const GameState &state, const Settings &settings, const std::vector<Point> &pockets);
    static ShotChoice chooseShot(const GameState &state, const Settings &settings, const std::vector<Point> &pockets);
};

} // namespace zg
