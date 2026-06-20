#include "ZGPookingEngine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

double dist(const zg::Point &a, const zg::Point &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

zg::Ball ball(double x, double y, int number, bool cue = false) {
    zg::Ball b;
    b.center = {x, y};
    b.radius = 12.0;
    b.number = number;
    b.cue = cue;
    return b;
}

zg::GameState baseState(zg::Point guideEnd) {
    zg::GameState state;
    state.table = {80, 120, 760, 430};
    state.cueBall = {225, 345};
    state.hasCueBall = true;
    state.guide = {true, state.cueBall, guideEnd};
    state.balls = {
        ball(state.cueBall.x, state.cueBall.y, 0, true),
        ball(505, 345, 3),
        ball(388, 260, 5),
        ball(612, 420, 8),
        ball(685, 236, 11)
    };
    return state;
}

bool hasRole(const std::vector<zg::Line> &lines, zg::LineRole role) {
    for (const auto &line : lines) {
        if (line.role == role) return true;
    }
    return false;
}

bool hasVisibleOrHiddenRole(const zg::Result &result, zg::LineRole role) {
    return hasRole(result.lines, role) || hasRole(result.hiddenLines, role);
}

bool finitePoint(const zg::Point &point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool allFinite(const zg::Result &result) {
    for (const auto &line : result.lines) {
        if (!finitePoint(line.start) || !finitePoint(line.end) || !std::isfinite(line.width)) return false;
    }
    for (const auto &line : result.hiddenLines) {
        if (!finitePoint(line.start) || !finitePoint(line.end) || !std::isfinite(line.width)) return false;
    }
    for (const auto &circle : result.circles) {
        if (!finitePoint(circle.center) || !std::isfinite(circle.radius) || !std::isfinite(circle.width)) return false;
    }
    return true;
}

const zg::Line *firstVisibleRole(const zg::Result &result, zg::LineRole role) {
    for (const auto &line : result.lines) {
        if (line.role == role) return &line;
    }
    return nullptr;
}

std::vector<zg::Line> visibleRoleLines(const zg::Result &result, zg::LineRole role) {
    std::vector<zg::Line> lines;
    for (const auto &line : result.lines) {
        if (line.role == role) lines.push_back(line);
    }
    return lines;
}

bool nearRail(const zg::Point &point, const zg::Rect &table) {
    const double edge = 2.5;
    return std::abs(point.x - table.x) <= edge ||
           std::abs(point.x - (table.x + table.width)) <= edge ||
           std::abs(point.y - table.y) <= edge ||
           std::abs(point.y - (table.y + table.height)) <= edge;
}

const char *modeName(zg::ShotMode mode) {
    switch (mode) {
        case zg::ShotMode::LongShot: return "long";
        case zg::ShotMode::CaromShot: return "carom";
        case zg::ShotMode::BankShot: return "bank";
        case zg::ShotMode::Auto:
        default: return "auto";
    }
}

const char *styleName(zg::PredictionStyle style) {
    switch (style) {
        case zg::PredictionStyle::Simple: return "simple";
        case zg::PredictionStyle::ProVideo: return "video";
        case zg::PredictionStyle::Advanced:
        default: return "advanced";
    }
}

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(EXIT_FAILURE);
    }
}

zg::Settings settingsFor(zg::PredictionStyle style, zg::ShotMode mode) {
    zg::Settings settings;
    settings.predictionEnabled = true;
    settings.cuePredictionEnabled = true;
    settings.pocketPredictionEnabled = true;
    settings.bankPredictionEnabled = true;
    settings.caromPredictionEnabled = true;
    settings.ladderGuideEnabled = true;
    settings.collisionWarningEnabled = true;
    settings.pocketHeatEnabled = true;
    settings.fourLinePredictionEnabled = true;
    settings.hiddenLineRecordingEnabled = true;
    settings.showSideLines = true;
    settings.showDetectedBalls = true;
    settings.showGhostBall = true;
    settings.maxBounces = 4;
    settings.scanRoute = zg::ScanRoute::AutoHybrid;
    settings.shotMode = mode;
    settings.predictionStyle = style;
    return settings;
}

} // namespace

int main() {
    zg::PredictionEngine engine;
    const std::vector<zg::PredictionStyle> styles = {
        zg::PredictionStyle::Simple,
        zg::PredictionStyle::Advanced,
        zg::PredictionStyle::ProVideo
    };
    const std::vector<zg::ShotMode> modes = {
        zg::ShotMode::Auto,
        zg::ShotMode::LongShot,
        zg::ShotMode::CaromShot,
        zg::ShotMode::BankShot
    };

    int cases = 0;
    for (const auto style : styles) {
        for (const auto mode : modes) {
            const auto result = engine.compute(baseState({650, 345}), settingsFor(style, mode));
            if (!result.valid) {
                std::cerr << "invalid result for " << styleName(style) << "/" << modeName(mode) << "\n";
                return EXIT_FAILURE;
            }
            if (!allFinite(result)) {
                std::cerr << "non-finite geometry for " << styleName(style) << "/" << modeName(mode) << "\n";
                return EXIT_FAILURE;
            }
            if (!hasVisibleOrHiddenRole(result, zg::LineRole::CueGuide) ||
                !hasVisibleOrHiddenRole(result, zg::LineRole::ObjectPath) ||
                !hasVisibleOrHiddenRole(result, zg::LineRole::GhostContact) ||
                !hasVisibleOrHiddenRole(result, zg::LineRole::CaromPath) ||
                (!hasVisibleOrHiddenRole(result, zg::LineRole::BankPath) &&
                 !hasVisibleOrHiddenRole(result, zg::LineRole::BouncePath))) {
                std::cerr << "missing four-line roles for " << styleName(style) << "/" << modeName(mode)
                          << " visible=" << result.lines.size()
                          << " hidden=" << result.hiddenLines.size() << "\n";
                return EXIT_FAILURE;
            }
            if (result.hiddenLines.size() < 6) {
                std::cerr << "hidden recording too small for " << styleName(style) << "/" << modeName(mode) << "\n";
                return EXIT_FAILURE;
            }
            cases += 1;
        }
    }

    const auto horizontal = engine.compute(baseState({650, 345}), settingsFor(zg::PredictionStyle::Advanced, zg::ShotMode::Auto));
    const auto angled = engine.compute(baseState({400, 258}), settingsFor(zg::PredictionStyle::Advanced, zg::ShotMode::Auto));
    const zg::Line *hCue = firstVisibleRole(horizontal, zg::LineRole::CueGuide);
    const zg::Line *aCue = firstVisibleRole(angled, zg::LineRole::CueGuide);
    const zg::Line *hObject = firstVisibleRole(horizontal, zg::LineRole::ObjectPath);
    const zg::Line *aObject = firstVisibleRole(angled, zg::LineRole::ObjectPath);
    const zg::Line *hCarom = firstVisibleRole(horizontal, zg::LineRole::CaromPath);
    const zg::Line *aCarom = firstVisibleRole(angled, zg::LineRole::CaromPath);
    require(hCue && aCue, "missing cue guide in guide-movement test");
    require(hObject && aObject, "missing object path in guide-movement test");
    require(hCarom && aCarom, "missing carom path in guide-movement test");
    require(dist(hCue->end, aCue->end) > 80.0, "guide movement did not change cue trajectory enough");
    require(dist(hObject->end, aObject->end) > 80.0, "guide movement did not change object trajectory enough");
    require(dist(hCarom->end, aCarom->end) > 80.0, "guide movement did not change cue-after-hit trajectory enough");
    require(dist(hCarom->start, hCarom->end) < 35.0, "head-on cue-after-hit path should be a short stop marker");

    const auto bankGuided = engine.compute(baseState({650, 345}), settingsFor(zg::PredictionStyle::Advanced, zg::ShotMode::BankShot));
    const zg::Line *bankObject = firstVisibleRole(bankGuided, zg::LineRole::ObjectPath);
    const zg::Line *bankPocket = firstVisibleRole(bankGuided, zg::LineRole::BankPath);
    require(bankObject && bankPocket, "bank-shot result missing object or bank path");
    require(nearRail(bankObject->end, bankGuided.table), "bank-shot object path did not target a rail");
    require(dist(bankPocket->start, bankObject->end) < 3.0, "bank-shot pocket leg did not continue from rail contact");
    require(dist(bankPocket->end, bankObject->end) > 40.0, "bank-shot rail target collapsed into direct pocket aim");

    const auto bankStraight = engine.compute(baseState({650, 345}), settingsFor(zg::PredictionStyle::Simple, zg::ShotMode::BankShot));
    const auto bankAngled = engine.compute(baseState({400, 258}), settingsFor(zg::PredictionStyle::Simple, zg::ShotMode::BankShot));
    const zg::Line *bankStraightObject = firstVisibleRole(bankStraight, zg::LineRole::ObjectPath);
    const zg::Line *bankAngledObject = firstVisibleRole(bankAngled, zg::LineRole::ObjectPath);
    const auto bankAngledLegs = visibleRoleLines(bankAngled, zg::LineRole::BankPath);
    require(bankStraightObject && bankAngledObject, "bank movement result missing object path");
    require(dist(bankStraightObject->end, bankAngledObject->end) > 80.0, "bank guide movement did not change rail target enough");
    require(bankAngledLegs.size() >= 2, "angled bank shot did not draw a multi-rail bank path");
    require(dist(bankAngledLegs[0].start, bankAngledObject->end) < 3.0, "multi-rail bank path did not start at first rail contact");
    require(dist(bankAngledLegs[1].start, bankAngledLegs[0].end) < 3.0, "multi-rail bank path legs are not chained");

    zg::GameState blocked;
    blocked.table = {80, 120, 760, 430};
    blocked.cueBall = {210, 335};
    blocked.hasCueBall = true;
    blocked.guide = {false, {}, {}};
    blocked.balls = {
        ball(blocked.cueBall.x, blocked.cueBall.y, 0, true),
        ball(430, 335, 2),
        ball(560, 335, 9),
        ball(430, 240, 4)
    };
    auto blockedSettings = settingsFor(zg::PredictionStyle::Advanced, zg::ShotMode::Auto);
    blockedSettings.scanRoute = zg::ScanRoute::BallGeometry;
    const auto blockedResult = engine.compute(blocked, blockedSettings);
    const zg::Line *objectPath = firstVisibleRole(blockedResult, zg::LineRole::ObjectPath);
    require(blockedResult.valid && objectPath, "blocked-path result missing object path");
    require(dist(objectPath->start, {430, 240}) < 35.0, "blocked direct lane was not avoided");

    std::cout << "modeCases=" << cases
              << " guideDelta=" << dist(hCue->end, aCue->end)
              << " hidden=" << horizontal.hiddenLines.size()
              << "\n";
    return EXIT_SUCCESS;
}
