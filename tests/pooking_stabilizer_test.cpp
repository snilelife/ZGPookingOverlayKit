#include "ZGPookingEngine.hpp"
#include "ZGPookingFrameScanner.hpp"

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

zg::GameState stateWithGuide(zg::Point cue, zg::Point guideEnd) {
    zg::GameState state;
    state.table = {80, 120, 760, 430};
    state.cueBall = cue;
    state.hasCueBall = true;
    state.guide = {true, cue, guideEnd};
    state.balls = {
        ball(cue.x, cue.y, 0, true),
        ball(505, 345, 3),
        ball(388, 260, 5),
        ball(612, 420, 8),
        ball(685, 236, 11)
    };
    return state;
}

zg::Settings settings() {
    zg::Settings s;
    s.predictionEnabled = true;
    s.fourLinePredictionEnabled = true;
    s.hiddenLineRecordingEnabled = true;
    s.scanRoute = zg::ScanRoute::AutoHybrid;
    s.predictionStyle = zg::PredictionStyle::Advanced;
    s.shotMode = zg::ShotMode::Auto;
    return s;
}

const zg::Line *firstRole(const zg::Result &result, zg::LineRole role) {
    for (const auto &line : result.lines) {
        if (line.role == role) return &line;
    }
    return nullptr;
}

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    zg::FrameStabilizer stabilizer;
    zg::FrameStabilizerOptions options;
    options.enabled = true;
    options.baseSmoothing = 0.28;

    const auto rawA = stateWithGuide({225, 345}, {650, 345});
    const auto rawJitter = stateWithGuide({227, 344}, {657, 349});
    const auto rawBigMove = stateWithGuide({225, 345}, {400, 258});

    const auto stableA = stabilizer.update(rawA, 0.86, options);
    const auto stableJitter = stabilizer.update(rawJitter, 0.86, options);
    const double rawJitterDistance = dist(rawA.guide.end, rawJitter.guide.end);
    const double stableJitterDistance = dist(stableA.guide.end, stableJitter.guide.end);

    require(stableJitterDistance < rawJitterDistance * 0.72, "small guide jitter was not smoothed enough");
    require(dist(stableJitter.cueBall, rawJitter.cueBall) < dist(rawA.cueBall, rawJitter.cueBall), "cue jitter was not smoothed");

    const auto stableBigMove = stabilizer.update(rawBigMove, 0.86, options);
    require(dist(stableBigMove.guide.end, rawBigMove.guide.end) < 2.0, "large guide move did not snap to new aim");

    zg::PredictionEngine engine;
    const auto resultA = engine.compute(stableA, settings());
    const auto resultJitter = engine.compute(stableJitter, settings());
    const auto resultBig = engine.compute(stableBigMove, settings());
    const zg::Line *cueA = firstRole(resultA, zg::LineRole::CueGuide);
    const zg::Line *cueJitter = firstRole(resultJitter, zg::LineRole::CueGuide);
    const zg::Line *cueBig = firstRole(resultBig, zg::LineRole::CueGuide);
    require(cueA && cueJitter && cueBig, "missing cue guide after stabilization");
    require(dist(cueA->end, cueJitter->end) < 18.0, "jitter changed prediction too much");
    require(dist(cueA->end, cueBig->end) > 80.0, "large aim change did not change prediction enough");

    std::cout << "rawJitter=" << rawJitterDistance
              << " stableJitter=" << stableJitterDistance
              << " bigMoveSnap=" << dist(stableBigMove.guide.end, rawBigMove.guide.end)
              << " predictionDelta=" << dist(cueA->end, cueBig->end)
              << "\n";
    return EXIT_SUCCESS;
}
