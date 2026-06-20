#include "ZGPookingEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace zg {
namespace {
constexpr double kEpsilon = 0.0001;
constexpr Color kWhite{1.0, 1.0, 1.0, 0.94};
constexpr Color kCyan{0.10, 0.86, 1.0, 0.88};
constexpr Color kGold{1.0, 0.84, 0.10, 0.96};
constexpr Color kGreen{0.70, 1.00, 0.12, 0.70};
constexpr Color kPurple{0.48, 0.38, 1.0, 0.52};
constexpr Color kOrange{1.0, 0.55, 0.14, 0.74};
constexpr Color kRed{1.0, 0.20, 0.20, 0.62};

double pointSegmentDistance(const Point &p, const Point &a, const Point &b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 <= kEpsilon) return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::max(0.0, std::min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
    const Point projected{a.x + t * dx, a.y + t * dy};
    return std::hypot(p.x - projected.x, p.y - projected.y);
}

bool rayRailHit(const Point &start, const Point &dir, const Rect &table, Point &hit, Point &reflected) {
    double bestT = std::numeric_limits<double>::max();
    int reflectedAxis = -1;

    auto consider = [&](double t, int axis) {
        if (t <= 1.0 || t >= bestT) return;
        const Point candidate{start.x + dir.x * t, start.y + dir.y * t};
        const double margin = 2.0;
        if (candidate.x < table.x - margin || candidate.x > table.x + table.width + margin ||
            candidate.y < table.y - margin || candidate.y > table.y + table.height + margin) {
            return;
        }
        bestT = t;
        reflectedAxis = axis;
        hit = candidate;
    };

    if (dir.x < -kEpsilon) consider((table.x - start.x) / dir.x, 0);
    if (dir.x > kEpsilon) consider((table.x + table.width - start.x) / dir.x, 0);
    if (dir.y < -kEpsilon) consider((table.y - start.y) / dir.y, 1);
    if (dir.y > kEpsilon) consider((table.y + table.height - start.y) / dir.y, 1);

    if (reflectedAxis < 0) return false;
    reflected = dir;
    if (reflectedAxis == 0) {
        reflected.x = -reflected.x;
    } else {
        reflected.y = -reflected.y;
    }
    return true;
}

Point reflectPointAcrossRail(const Point &point, const Rect &table, int rail) {
    if (rail == 0) return {point.x, table.y * 2.0 - point.y};
    if (rail == 1) return {point.x, (table.y + table.height) * 2.0 - point.y};
    if (rail == 2) return {table.x * 2.0 - point.x, point.y};
    return {(table.x + table.width) * 2.0 - point.x, point.y};
}

Point reflectDirectionAcrossRail(const Point &dir, int rail) {
    if (rail == 0 || rail == 1) return {dir.x, -dir.y};
    return {-dir.x, dir.y};
}

bool raySpecificRailHit(const Point &start, const Point &dir, const Rect &table, int rail, Point &hit) {
    double t = 0.0;
    if (rail == 0 || rail == 1) {
        if (std::abs(dir.y) <= kEpsilon) return false;
        const double y = rail == 0 ? table.y : table.y + table.height;
        t = (y - start.y) / dir.y;
    } else {
        if (std::abs(dir.x) <= kEpsilon) return false;
        const double x = rail == 2 ? table.x : table.x + table.width;
        t = (x - start.x) / dir.x;
    }
    if (t <= 1.0) return false;

    hit = {start.x + dir.x * t, start.y + dir.y * t};
    const double margin = 2.0;
    return hit.x >= table.x - margin && hit.x <= table.x + table.width + margin &&
           hit.y >= table.y - margin && hit.y <= table.y + table.height + margin;
}

bool guidedGhostPoint(const Point &cue, const Point &aim, const Point &object, double radius, Point &ghost, double &signedMiss) {
    const Point toBall{object.x - cue.x, object.y - cue.y};
    const double projection = toBall.x * aim.x + toBall.y * aim.y;
    if (projection <= radius * 1.2) return false;

    signedMiss = toBall.x * aim.y - toBall.y * aim.x;
    const double collisionRadius = std::max(radius * 2.0, radius + 8.0);
    const double missAbs = std::abs(signedMiss);
    if (missAbs > collisionRadius * 1.02) return false;

    const double clampedMiss = std::min(missAbs, collisionRadius * 0.995);
    const double centerToImpact = std::sqrt(std::max(0.0, collisionRadius * collisionRadius - clampedMiss * clampedMiss));
    const double impactDistance = projection - centerToImpact;
    if (impactDistance <= radius * 0.4) return false;

    ghost = {cue.x + aim.x * impactDistance, cue.y + aim.y * impactDistance};
    return true;
}
}

Result PredictionEngine::compute(const GameState &state, const Settings &settings) const {
    Result result;
    result.table = state.table;
    result.routeName = routeName(settings.scanRoute);
    result.shotModeName = shotModeName(settings.shotMode);
    result.styleName = styleName(settings.predictionStyle);

    if (!settings.predictionEnabled || state.table.width <= 0.0 || state.table.height <= 0.0) {
        return result;
    }

    const auto pockets = pocketPoints(state.table);
    const int selectedPocket = std::max(0, std::min(settings.selectedPocket, static_cast<int>(pockets.size()) - 1));
    result.selectedPocket = selectedPocket;

    const Point cue = state.hasCueBall
        ? state.cueBall
        : Point{state.table.x + state.table.width * 0.24, state.table.y + state.table.height * 0.5};
    const double ballRadius = std::max(8.0, state.table.width * 0.018);
    const PredictionStyle style = settings.predictionStyle;
    const ScanRoute route = settings.scanRoute;
    const ShotMode mode = settings.shotMode;

    if (settings.pocketPredictionEnabled && (settings.pocketHeatEnabled || style == PredictionStyle::ProVideo)) {
        for (std::size_t i = 0; i < pockets.size(); ++i) {
            const double emphasis = settings.manualPocket && static_cast<int>(i) == selectedPocket ? 0.92 : 0.42;
            result.circles.push_back(makeCircle(pockets[i],
                                                std::max(10.0, state.table.width * (settings.manualPocket && static_cast<int>(i) == selectedPocket ? 0.026 : 0.016)),
                                                Color{0.08, 0.95, 1.0, emphasis},
                                                settings.manualPocket && static_cast<int>(i) == selectedPocket ? 3.4 : 2.0));
        }
    }

    bool usedGuide = false;
    bool usedChoice = false;
    ShotChoice choice;

    if (settings.cuePredictionEnabled &&
        (route == ScanRoute::AutoHybrid || route == ScanRoute::GuideLock) &&
        state.guide.valid) {
        choice = chooseGuidedShot(state, settings, pockets);
        usedChoice = choice.ballRadius > 0.0;

        if (!usedChoice) {
            const auto path = bouncePath(state.guide.start, state.guide.end, state.table, std::max(1, settings.maxBounces + 1));
            for (std::size_t i = 0; i + 1 < path.size(); ++i) {
                const Color color = i == 0 ? kWhite : Color{0.18, 0.88, 1.0, 0.74};
                const double width = i == 0 ? (style == PredictionStyle::ProVideo ? 4.2 : 3.2) : 2.2;
                const LineRole role = i == 0 ? LineRole::CueGuide : LineRole::BouncePath;
                if (i == 0 && settings.ladderGuideEnabled) {
                    addLadderLine(result, path[i], path[i + 1], color, width, style, role);
                } else {
                    addStyledLine(result, path[i], path[i + 1], color, width, style, role);
                }
                if (settings.hiddenLineRecordingEnabled) {
                    result.hiddenLines.push_back(makeLine(path[i], path[i + 1],
                                                          Color{color.r, color.g, color.b, color.a * 0.24},
                                                          style == PredictionStyle::ProVideo ? 1.3 : 1.0,
                                                          role));
                }
            }
            usedGuide = true;
        }
    }

    if (!usedGuide && !usedChoice && route != ScanRoute::CornerLock) {
        choice = chooseShot(state, settings, pockets);
        usedChoice = choice.ballRadius > 0.0;
    }

    if (!usedGuide && usedChoice) {
        if (settings.fourLinePredictionEnabled) {
            addFourLinePack(result, choice, state, settings);
        } else {
            if (settings.cuePredictionEnabled) {
                if (settings.ladderGuideEnabled) {
                    addLadderLine(result, choice.cue, choice.ghost, kWhite, style == PredictionStyle::ProVideo ? 4.0 : 3.1, style, LineRole::CueGuide);
                } else {
                    addStyledLine(result, choice.cue, choice.ghost, kWhite, style == PredictionStyle::ProVideo ? 4.0 : 3.1, style, LineRole::CueGuide);
                }
            }
            if (settings.pocketPredictionEnabled) {
                addStyledLine(result, choice.object, shotObjectTarget(choice), kCyan, style == PredictionStyle::ProVideo ? 3.1 : 2.4, style, LineRole::ObjectPath);
            }
        }
        if (settings.collisionWarningEnabled) {
            addCollisionWarnings(result, choice, state, style);
        }
        if (settings.hiddenLineRecordingEnabled) {
            recordHiddenLines(result, choice, state, settings);
        }

        if (!settings.fourLinePredictionEnabled &&
            (style == PredictionStyle::ProVideo || mode == ShotMode::BankShot || mode == ShotMode::CaromShot)) {
            if (settings.cuePredictionEnabled || settings.pocketPredictionEnabled) {
                addStyledLine(result, choice.ghost, choice.object, Color{1.0, 0.84, 0.10, 0.78}, 2.0, style, LineRole::GhostContact);
            }
            if (settings.bankPredictionEnabled) {
                if (choice.bankGuide.size() >= 3) {
                    const Color bankColor = mode == ShotMode::BankShot ? kOrange : Color{0.72, 0.40, 1.0, 0.36};
                    addBankGuideSegments(result, choice.bankGuide, bankColor, mode == ShotMode::BankShot ? 2.5 : 1.8, style, 1);
                } else {
                    for (const auto &guide : bankGuides(choice.object, choice.pocket, state.table)) {
                        if (guide.size() >= 3) {
                            const Color bankColor = mode == ShotMode::BankShot ? kOrange : Color{0.72, 0.40, 1.0, 0.36};
                            addBankGuideSegments(result, guide, bankColor, mode == ShotMode::BankShot ? 2.5 : 1.8, style, 0);
                        }
                    }
                }
            }
        }

        if (settings.showGhostBall) {
            result.circles.push_back(makeCircle(choice.ghost, ballRadius, kGold, style == PredictionStyle::ProVideo ? 3.2 : 2.6));
            if (style == PredictionStyle::ProVideo) {
                result.circles.push_back(makeCircle(choice.object, ballRadius * 1.18, Color{0.10, 0.86, 1.0, 0.66}, 2.2));
            }
        }

        if (settings.cuePredictionEnabled && settings.caromPredictionEnabled &&
            (style != PredictionStyle::Simple || mode == ShotMode::CaromShot)) {
            const auto after = cueAfterHitPath(choice, state.table, settings.maxBounces);
            for (std::size_t i = 0; i + 1 < after.size(); ++i) {
                addStyledLine(result, after[i], after[i + 1], kGreen, mode == ShotMode::CaromShot ? 2.8 : (style == PredictionStyle::ProVideo ? 2.5 : 2.0), style, LineRole::CaromPath);
            }
            if (mode == ShotMode::CaromShot) {
                addCaromTargets(result, choice, state, style);
            }
        }
    }

    if (!usedGuide && !usedChoice) {
        const Point target = settings.manualPocket ? pockets[selectedPocket] : nearestPocket(cue, pockets);
        const double reach = std::hypot(state.table.width, state.table.height) * settings.lineLength;
        const Point aimEnd = clamp(pointToward(cue, target, reach), state.table);
        if (settings.cuePredictionEnabled) {
            if (settings.ladderGuideEnabled) {
                addLadderLine(result, cue, aimEnd, kWhite, style == PredictionStyle::ProVideo ? 3.8 : 2.8, style, LineRole::CueGuide);
            } else {
                addStyledLine(result, cue, aimEnd, kWhite, style == PredictionStyle::ProVideo ? 3.8 : 2.8, style, LineRole::CueGuide);
            }
        }
        if (settings.pocketPredictionEnabled && style != PredictionStyle::Simple) {
            addStyledLine(result, aimEnd, target, kCyan, style == PredictionStyle::ProVideo ? 2.8 : 2.2, style, LineRole::ObjectPath);
        }
    }

    if (settings.showSideLines) {
        const double alpha = style == PredictionStyle::ProVideo ? 0.26 : 0.18;
        result.lines.push_back(makeLine(Point{state.table.x, state.table.y + state.table.height * 0.5},
                                        Point{state.table.x + state.table.width, state.table.y + state.table.height * 0.5},
                                        Color{1.0, 0.86, 0.18, alpha}, style == PredictionStyle::ProVideo ? 1.4 : 1.0, LineRole::CenterLine));
        result.lines.push_back(makeLine(Point{state.table.x + state.table.width * 0.5, state.table.y},
                                        Point{state.table.x + state.table.width * 0.5, state.table.y + state.table.height},
                                        Color{1.0, 0.86, 0.18, alpha}, style == PredictionStyle::ProVideo ? 1.4 : 1.0, LineRole::CenterLine));
    }

    if (settings.bankPredictionEnabled && (style != PredictionStyle::Simple || mode == ShotMode::BankShot) && !usedGuide) {
        const Point bounceStart = usedChoice ? choice.cue : cue;
        const Point bounceTarget = usedChoice ? choice.pocket : pockets[selectedPocket];
        const auto bounces = bouncePath(bounceStart, bounceTarget, state.table, settings.maxBounces);
        for (std::size_t i = 0; i + 1 < bounces.size(); ++i) {
            addStyledLine(result, bounces[i], bounces[i + 1], mode == ShotMode::BankShot ? kOrange : kPurple,
                          mode == ShotMode::BankShot ? 2.2 : (style == PredictionStyle::ProVideo ? 1.9 : 1.5), style, LineRole::BouncePath);
        }
    }

    result.circles.push_back(makeCircle(cue, ballRadius, kWhite, style == PredictionStyle::ProVideo ? 3.6 : 2.8));
    if (settings.pocketPredictionEnabled) {
        const Point selected = usedChoice ? choice.pocket : pockets[selectedPocket];
        result.circles.push_back(makeCircle(selected, std::max(13.0, state.table.width * (style == PredictionStyle::ProVideo ? 0.030 : 0.023)),
                                            Color{0.1, 0.88, 1.0, 0.94}, style == PredictionStyle::ProVideo ? 4.2 : 3.0));
    }

    if (settings.showDetectedBalls) {
        const std::size_t limit = style == PredictionStyle::ProVideo ? 16 : style == PredictionStyle::Advanced ? 10 : 6;
        for (std::size_t i = 0; i < std::min(limit, state.balls.size()); ++i) {
            const auto &ball = state.balls[i];
            result.circles.push_back(makeCircle(ball.center, ball.radius > 0.0 ? ball.radius : ballRadius,
                                                ball.cue ? Color{1.0, 1.0, 1.0, 0.75} : kRed,
                                                style == PredictionStyle::ProVideo ? 2.2 : 1.7));
        }
    }

    result.valid = true;
    return result;
}

const char *PredictionEngine::routeName(ScanRoute route) {
    switch (route) {
        case ScanRoute::GuideLock: return "guide_lock";
        case ScanRoute::BallGeometry: return "ball_geometry";
        case ScanRoute::CornerLock: return "corner_lock";
        case ScanRoute::AutoHybrid:
        default: return "auto_hybrid";
    }
}

const char *PredictionEngine::shotModeName(ShotMode mode) {
    switch (mode) {
        case ShotMode::LongShot: return "long_shot";
        case ShotMode::CaromShot: return "carom_shot";
        case ShotMode::BankShot: return "bank_shot";
        case ShotMode::Auto:
        default: return "auto";
    }
}

const char *PredictionEngine::styleName(PredictionStyle style) {
    switch (style) {
        case PredictionStyle::Simple: return "simple";
        case PredictionStyle::ProVideo: return "pro_video";
        case PredictionStyle::Advanced:
        default: return "advanced";
    }
}

std::vector<Point> PredictionEngine::pocketPoints(const Rect &table) {
    return {
        {table.x, table.y},
        {table.x + table.width * 0.5, table.y},
        {table.x + table.width, table.y},
        {table.x, table.y + table.height},
        {table.x + table.width * 0.5, table.y + table.height},
        {table.x + table.width, table.y + table.height}
    };
}

Point PredictionEngine::nearestPocket(const Point &point, const std::vector<Point> &pockets) {
    return *std::min_element(pockets.begin(), pockets.end(), [&](const Point &a, const Point &b) {
        return distance(point, a) < distance(point, b);
    });
}

Point PredictionEngine::pointToward(const Point &start, const Point &end, double d) {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double len = std::max(kEpsilon, std::hypot(dx, dy));
    return {start.x + dx / len * d, start.y + dy / len * d};
}

Point PredictionEngine::clamp(const Point &point, const Rect &rect) {
    return {
        std::min(std::max(point.x, rect.x), rect.x + rect.width),
        std::min(std::max(point.y, rect.y), rect.y + rect.height)
    };
}

double PredictionEngine::distance(const Point &a, const Point &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

double PredictionEngine::cross(const Point &a, const Point &b) {
    return a.x * b.y - a.y * b.x;
}

double PredictionEngine::dot(const Point &a, const Point &b) {
    return a.x * b.x + a.y * b.y;
}

Line PredictionEngine::makeLine(const Point &start, const Point &end, Color color, double width, LineRole role) {
    return {start, end, color, width, role};
}

Circle PredictionEngine::makeCircle(const Point &center, double radius, Color color, double width) {
    return {center, radius, color, width};
}

void PredictionEngine::addStyledLine(Result &result, const Point &start, const Point &end, Color color, double width, PredictionStyle style, LineRole role) {
    if (style == PredictionStyle::ProVideo) {
        result.lines.push_back(makeLine(start, end, Color{color.r, color.g, color.b, color.a * 0.20}, width * 3.1, role));
        result.lines.push_back(makeLine(start, end, Color{color.r, color.g, color.b, color.a * 0.34}, width * 1.85, role));
    }
    result.lines.push_back(makeLine(start, end, color, width, role));
}

void PredictionEngine::addLadderLine(Result &result, const Point &start, const Point &end, Color color, double width, PredictionStyle style, LineRole role) {
    addStyledLine(result, start, end, color, width, style, role);

    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double len = std::max(kEpsilon, std::hypot(dx, dy));
    const double nx = -dy / len;
    const double ny = dx / len;
    const double rungHalf = std::max(5.0, width * 2.2);
    const double step = std::max(18.0, width * 6.0);
    const int rungCount = std::max(0, static_cast<int>(len / step));

    for (int i = 1; i < rungCount; ++i) {
        const double t = (step * i) / len;
        const Point center{start.x + dx * t, start.y + dy * t};
        result.lines.push_back(makeLine(Point{center.x - nx * rungHalf, center.y - ny * rungHalf},
                                        Point{center.x + nx * rungHalf, center.y + ny * rungHalf},
                                        Color{color.r, color.g, color.b, color.a * 0.60},
                                        std::max(1.2, width * 0.50),
                                        role));
    }
}

void PredictionEngine::addBankGuideSegments(Result &result,
                                            const std::vector<Point> &guide,
                                            Color color,
                                            double width,
                                            PredictionStyle style,
                                            std::size_t startIndex) {
    if (guide.size() < 2 || startIndex >= guide.size() - 1) return;
    for (std::size_t i = startIndex; i + 1 < guide.size(); ++i) {
        addStyledLine(result, guide[i], guide[i + 1], color, width, style, LineRole::BankPath);
    }
}

void PredictionEngine::recordBankGuideSegments(std::vector<Line> &lines,
                                               const std::vector<Point> &guide,
                                               Color color,
                                               double width,
                                               std::size_t startIndex) {
    if (guide.size() < 2 || startIndex >= guide.size() - 1) return;
    for (std::size_t i = startIndex; i + 1 < guide.size(); ++i) {
        lines.push_back(makeLine(guide[i], guide[i + 1], color, width, LineRole::BankPath));
    }
}

void PredictionEngine::addFourLinePack(Result &result, const ShotChoice &choice, const GameState &state, const Settings &settings) {
    const PredictionStyle style = settings.predictionStyle;
    const bool pro = style == PredictionStyle::ProVideo;
    const Point target = shotObjectTarget(choice);

    if (settings.cuePredictionEnabled) {
        if (settings.ladderGuideEnabled) {
            addLadderLine(result, choice.cue, choice.ghost, kWhite, pro ? 4.1 : 3.1, style, LineRole::CueGuide);
        } else {
            addStyledLine(result, choice.cue, choice.ghost, kWhite, pro ? 4.1 : 3.1, style, LineRole::CueGuide);
        }
    }

    if (settings.pocketPredictionEnabled) {
        addStyledLine(result, choice.object, target, kCyan, pro ? 3.2 : 2.4, style, LineRole::ObjectPath);
        addStyledLine(result, choice.ghost, choice.object, kGold, pro ? 2.2 : 1.8, style, LineRole::GhostContact);
    }

    if (settings.caromPredictionEnabled) {
        const auto after = cueAfterHitPath(choice, state.table, std::max(1, settings.maxBounces));
        if (after.size() >= 2) {
            const std::size_t limit = settings.shotMode == ShotMode::CaromShot ? after.size() : std::min<std::size_t>(after.size(), 3);
            for (std::size_t i = 0; i + 1 < limit; ++i) {
                addStyledLine(result, after[i], after[i + 1], kGreen, settings.shotMode == ShotMode::CaromShot ? 2.8 : (pro ? 2.3 : 1.9), style, LineRole::CaromPath);
            }
        }
    }

    if (settings.bankPredictionEnabled) {
        bool drewBank = false;
        if (choice.bankGuide.size() >= 3) {
            const Color bankColor = settings.shotMode == ShotMode::BankShot ? kOrange : kPurple;
            addBankGuideSegments(result, choice.bankGuide, bankColor, settings.shotMode == ShotMode::BankShot ? 2.5 : (pro ? 2.0 : 1.5), style, 1);
            drewBank = true;
        } else {
            for (const auto &guide : bankGuides(choice.object, choice.pocket, state.table)) {
                if (guide.size() < 3) continue;
                const Color bankColor = settings.shotMode == ShotMode::BankShot ? kOrange : kPurple;
                addBankGuideSegments(result, guide, bankColor, settings.shotMode == ShotMode::BankShot ? 2.5 : (pro ? 2.0 : 1.5), style, 0);
                drewBank = true;
                break;
            }
        }

        if (!drewBank) {
            const auto bounces = bouncePath(choice.cue, choice.pocket, state.table, std::max(1, settings.maxBounces));
            const std::size_t limit = std::min<std::size_t>(bounces.size(), 3);
            for (std::size_t i = 0; i + 1 < limit; ++i) {
                addStyledLine(result, bounces[i], bounces[i + 1], kPurple, pro ? 1.8 : 1.4, style, LineRole::BouncePath);
            }
        }
    }
}

void PredictionEngine::recordHiddenLines(Result &result, const ShotChoice &choice, const GameState &state, const Settings &settings) {
    const double hiddenWidth = settings.predictionStyle == PredictionStyle::ProVideo ? 1.4 : 1.0;
    const Color hiddenCue{1.0, 1.0, 1.0, 0.18};
    const Color hiddenPocket{0.10, 0.86, 1.0, 0.20};
    const Color hiddenBank{1.0, 0.55, 0.14, 0.20};
    const Color hiddenCarom{0.70, 1.0, 0.12, 0.18};
    const Point target = shotObjectTarget(choice);

    result.hiddenLines.push_back(makeLine(choice.cue, choice.ghost, hiddenCue, hiddenWidth, LineRole::CueGuide));
    result.hiddenLines.push_back(makeLine(choice.object, target, hiddenPocket, hiddenWidth, LineRole::ObjectPath));

    const auto after = cueAfterHitPath(choice, state.table, std::max(1, settings.maxBounces));
    for (std::size_t i = 0; i + 1 < after.size(); ++i) {
        result.hiddenLines.push_back(makeLine(after[i], after[i + 1], hiddenCarom, hiddenWidth, LineRole::CaromPath));
    }

    if (choice.bankGuide.size() >= 3) {
        recordBankGuideSegments(result.hiddenLines, choice.bankGuide, hiddenBank, hiddenWidth, 1);
    }

    for (const auto &guide : bankGuides(choice.object, choice.pocket, state.table)) {
        if (guide.size() < 3) continue;
        recordBankGuideSegments(result.hiddenLines, guide, hiddenBank, hiddenWidth, 0);
    }

    const auto bounce = bouncePath(choice.cue, choice.pocket, state.table, std::max(1, settings.maxBounces));
    for (std::size_t i = 0; i + 1 < bounce.size(); ++i) {
        result.hiddenLines.push_back(makeLine(bounce[i], bounce[i + 1], Color{0.48, 0.38, 1.0, 0.18}, hiddenWidth, LineRole::BouncePath));
    }
}

std::vector<Point> PredictionEngine::bouncePath(const Point &start, const Point &target, const Rect &table, int count) {
    std::vector<Point> points;
    if (count <= 0) return points;
    points.push_back(start);

    Point current = start;
    double vx = target.x - start.x;
    double vy = target.y - start.y;
    const double rawLen = std::hypot(vx, vy);
    if (rawLen <= kEpsilon) {
        points.push_back(start);
        return points;
    }
    const double len = std::max(kEpsilon, rawLen);
    vx /= len;
    vy /= len;

    for (int i = 0; i < count; ++i) {
        const double tx = vx > 0 ? (table.x + table.width - current.x) / vx : (table.x - current.x) / vx;
        const double ty = vy > 0 ? (table.y + table.height - current.y) / vy : (table.y - current.y) / vy;
        const double t = std::max(kEpsilon, std::min(std::abs(tx), std::abs(ty)));
        Point next{current.x + vx * t, current.y + vy * t};
        points.push_back(next);

        if (std::abs(next.x - table.x) < 1.0 || std::abs(next.x - (table.x + table.width)) < 1.0) vx = -vx;
        if (std::abs(next.y - table.y) < 1.0 || std::abs(next.y - (table.y + table.height)) < 1.0) vy = -vy;
        current = next;
    }

    return points;
}

std::vector<Point> PredictionEngine::cueAfterHitPath(const ShotChoice &choice, const Rect &table, int count) {
    Point incoming{choice.ghost.x - choice.cue.x, choice.ghost.y - choice.cue.y};
    Point normal{choice.object.x - choice.ghost.x, choice.object.y - choice.ghost.y};
    const double incomingLen = std::hypot(incoming.x, incoming.y);
    const double normalLen = std::hypot(normal.x, normal.y);
    if (incomingLen <= kEpsilon || normalLen <= kEpsilon) {
        return {choice.ghost, choice.ghost};
    }

    incoming.x /= incomingLen;
    incoming.y /= incomingLen;
    normal.x /= normalLen;
    normal.y /= normalLen;

    double normalSpeed = dot(incoming, normal);
    if (normalSpeed < 0.0) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normalSpeed = -normalSpeed;
    }

    Point tangent{incoming.x - normal.x * normalSpeed, incoming.y - normal.y * normalSpeed};
    const double tangentLen = std::hypot(tangent.x, tangent.y);
    const double radius = std::max(6.0, choice.ballRadius);
    const double tableDiag = std::max(radius * 8.0, std::hypot(table.width, table.height));

    if (tangentLen <= 0.06) {
        const Point stopEnd{choice.ghost.x - normal.x * radius * 1.35,
                            choice.ghost.y - normal.y * radius * 1.35};
        return {choice.ghost, stopEnd};
    }

    tangent.x /= tangentLen;
    tangent.y /= tangentLen;
    const Point end{choice.ghost.x + tangent.x * tableDiag,
                    choice.ghost.y + tangent.y * tableDiag};
    return bouncePath(choice.ghost, end, table, std::max(1, count));
}

void PredictionEngine::addCollisionWarnings(Result &result, const ShotChoice &choice, const GameState &state, PredictionStyle style) {
    double nearestProjection = std::numeric_limits<double>::max();
    const Ball *blockingBall = nullptr;
    const double pathLength = distance(choice.cue, choice.ghost);
    if (pathLength <= kEpsilon) return;

    const Point dir{(choice.ghost.x - choice.cue.x) / pathLength, (choice.ghost.y - choice.cue.y) / pathLength};
    for (const auto &ball : state.balls) {
        if (ball.cue) continue;
        if (distance(ball.center, choice.object) < std::max(10.0, ball.radius * 1.5)) continue;
        const double projection = (ball.center.x - choice.cue.x) * dir.x + (ball.center.y - choice.cue.y) * dir.y;
        if (projection <= 0.0 || projection >= pathLength) continue;
        const double radius = ball.radius > 0.0 ? ball.radius : std::max(8.0, state.table.width * 0.018);
        const double miss = pointSegmentDistance(ball.center, choice.cue, choice.ghost);
        if (miss <= radius * 1.12 && projection < nearestProjection) {
            nearestProjection = projection;
            blockingBall = &ball;
        }
    }

    if (!blockingBall) return;
    const double radius = blockingBall->radius > 0.0 ? blockingBall->radius : std::max(8.0, state.table.width * 0.018);
    addStyledLine(result, choice.cue, blockingBall->center, Color{1.0, 0.10, 0.10, 0.72}, style == PredictionStyle::ProVideo ? 2.7 : 2.1, style, LineRole::CollisionWarning);
    result.circles.push_back(makeCircle(blockingBall->center, radius * 1.55, Color{1.0, 0.10, 0.10, 0.90}, style == PredictionStyle::ProVideo ? 3.4 : 2.6));
}

void PredictionEngine::addCaromTargets(Result &result, const ShotChoice &choice, const GameState &state, PredictionStyle style) {
    const auto after = cueAfterHitPath(choice, state.table, 1);
    if (after.size() < 2) return;

    const Point start = after.front();
    const Point end = after.back();
    double bestDistance = std::numeric_limits<double>::max();
    const Ball *bestBall = nullptr;

    for (const auto &ball : state.balls) {
        if (ball.cue) continue;
        if (distance(ball.center, choice.object) < std::max(10.0, ball.radius * 1.5)) continue;
        const double d = pointSegmentDistance(ball.center, start, end);
        if (d < bestDistance) {
            bestDistance = d;
            bestBall = &ball;
        }
    }

    if (!bestBall) return;
    const double radius = bestBall->radius > 0.0 ? bestBall->radius : std::max(8.0, state.table.width * 0.018);
    addStyledLine(result, start, bestBall->center, Color{0.70, 1.0, 0.12, 0.78}, style == PredictionStyle::ProVideo ? 2.6 : 2.1, style, LineRole::CaromPath);
    result.circles.push_back(makeCircle(bestBall->center, radius * 1.35, Color{0.70, 1.0, 0.12, 0.86}, style == PredictionStyle::ProVideo ? 3.2 : 2.4));
}

std::vector<std::vector<Point>> PredictionEngine::bankGuides(const Point &object, const Point &pocket, const Rect &table) {
    std::vector<std::vector<Point>> guides;
    for (int rail = 0; rail < 4; ++rail) {
        std::vector<Point> guide;
        if (railGuide(object, pocket, table, rail, guide)) {
            guides.push_back(guide);
        }
    }

    for (int firstRail = 0; firstRail < 4; ++firstRail) {
        for (int secondRail = 0; secondRail < 4; ++secondRail) {
            if (firstRail == secondRail) continue;
            std::vector<Point> guide;
            if (railSequenceGuide(object, pocket, table, {firstRail, secondRail}, guide)) {
                guides.push_back(guide);
            }
        }
    }

    std::sort(guides.begin(), guides.end(), [](const auto &a, const auto &b) {
        return guideLength(a) < guideLength(b);
    });
    if (guides.size() > 6) guides.resize(6);
    return guides;
}

bool PredictionEngine::railGuide(const Point &object, const Point &pocket, const Rect &table, int rail, std::vector<Point> &out) {
    return railSequenceGuide(object, pocket, table, {rail}, out);
}

bool PredictionEngine::railSequenceGuide(const Point &object,
                                         const Point &pocket,
                                         const Rect &table,
                                         const std::vector<int> &rails,
                                         std::vector<Point> &out) {
    if (rails.empty()) return false;

    Point mirror = pocket;
    for (auto it = rails.rbegin(); it != rails.rend(); ++it) {
        mirror = reflectPointAcrossRail(mirror, table, *it);
    }

    Point dir{mirror.x - object.x, mirror.y - object.y};
    const double len = std::hypot(dir.x, dir.y);
    if (len <= kEpsilon) return false;
    dir.x /= len;
    dir.y /= len;

    out.clear();
    out.push_back(object);
    Point current = object;

    for (const int rail : rails) {
        Point hit;
        if (!raySpecificRailHit(current, dir, table, rail, hit)) return false;
        if (distance(hit, current) < 4.0) return false;
        out.push_back(hit);
        current = hit;
        dir = reflectDirectionAcrossRail(dir, rail);
    }

    if (distance(current, pocket) < 4.0) return false;
    out.push_back(pocket);
    return true;
}

double PredictionEngine::guideLength(const std::vector<Point> &guide) {
    if (guide.size() < 2) return std::numeric_limits<double>::max();
    double length = 0.0;
    for (std::size_t i = 0; i + 1 < guide.size(); ++i) {
        length += distance(guide[i], guide[i + 1]);
    }
    return length;
}

double PredictionEngine::pathBlockPenalty(const Point &start,
                                          const Point &end,
                                          const GameState &state,
                                          const Ball *ignoreA,
                                          const Ball *ignoreB,
                                          double clearanceScale) {
    const double length = distance(start, end);
    if (length <= kEpsilon) return 0.0;

    double penalty = 0.0;
    for (const auto &ball : state.balls) {
        if (&ball == ignoreA || &ball == ignoreB || ball.cue) continue;

        const double radius = ball.radius > 0.0 ? ball.radius : std::max(8.0, state.table.width * 0.018);
        if (distance(ball.center, start) < radius * 1.4 || distance(ball.center, end) < radius * 1.4) continue;

        const Point ray{end.x - start.x, end.y - start.y};
        const double projection = ((ball.center.x - start.x) * ray.x + (ball.center.y - start.y) * ray.y) / std::max(kEpsilon, length);
        if (projection <= radius * 0.4 || projection >= length - radius * 0.4) continue;

        const double miss = pointSegmentDistance(ball.center, start, end);
        const double clearance = radius * clearanceScale;
        if (miss < clearance) {
            const double overlap = clearance - miss;
            const double centerWeight = 1.0 - std::abs(projection / length - 0.5) * 0.45;
            penalty += overlap * overlap * centerWeight;
        }
    }
    return penalty;
}

double PredictionEngine::guideBlockPenalty(const std::vector<Point> &guide, const GameState &state, const Ball *ignoreBall) {
    if (guide.size() < 2) return 1000000.0;
    double penalty = 0.0;
    for (std::size_t i = 0; i + 1 < guide.size(); ++i) {
        penalty += pathBlockPenalty(guide[i], guide[i + 1], state, ignoreBall, nullptr, 1.0);
    }
    return penalty;
}

Point PredictionEngine::shotObjectTarget(const ShotChoice &choice) {
    return choice.hasObjectTarget ? choice.objectTarget : choice.pocket;
}

Point PredictionEngine::ghostForTarget(const Point &object, const Point &target, double radius) {
    const double len = std::max(kEpsilon, distance(object, target));
    const double ux = (target.x - object.x) / len;
    const double uy = (target.y - object.y) / len;
    return {object.x - ux * radius * 2.0, object.y - uy * radius * 2.0};
}

PredictionEngine::ShotChoice PredictionEngine::chooseGuidedShot(const GameState &state, const Settings &settings, const std::vector<Point> &pockets) {
    ShotChoice best;
    best.score = std::numeric_limits<double>::max();
    if (!state.guide.valid) {
        best.ballRadius = 0.0;
        return best;
    }

    const Point cue = state.hasCueBall ? state.cueBall : state.guide.start;
    Point aim{state.guide.end.x - state.guide.start.x, state.guide.end.y - state.guide.start.y};
    const double aimLength = std::max(kEpsilon, std::hypot(aim.x, aim.y));
    aim.x /= aimLength;
    aim.y /= aimLength;

    const std::vector<int> pocketIndexes = settings.manualPocket
        ? std::vector<int>{std::max(0, std::min(settings.selectedPocket, static_cast<int>(pockets.size()) - 1))}
        : std::vector<int>{0, 1, 2, 3, 4, 5};
    const double tableDiag = std::hypot(state.table.width, state.table.height);

    for (const auto &ball : state.balls) {
        const double radius = ball.radius > 0.0 ? ball.radius : std::max(8.0, state.table.width * 0.018);
        if (ball.cue) continue;

        const Point toBall{ball.center.x - cue.x, ball.center.y - cue.y};
        const double projection = dot(toBall, aim);
        if (projection <= radius * 1.5) continue;

        const double miss = std::abs(cross(toBall, aim));
        const double hitWindow = radius * (settings.shotMode == ShotMode::LongShot ? 2.65 : 2.25);
        if (miss > hitWindow) continue;

        Point ghost;
        double signedMiss = 0.0;
        if (!guidedGhostPoint(cue, aim, ball.center, radius, ghost, signedMiss)) continue;
        Point impactDir{ball.center.x - ghost.x, ball.center.y - ghost.y};
        const double impactLen = std::max(kEpsilon, std::hypot(impactDir.x, impactDir.y));
        impactDir.x /= impactLen;
        impactDir.y /= impactLen;
        const double cuePenalty = pathBlockPenalty(cue, ghost, state, nullptr, &ball, 1.18);

        if (settings.shotMode == ShotMode::BankShot) {
            std::vector<Point> route{ball.center};
            Point current = ball.center;
            Point direction = impactDir;
            const int railLimit = std::max(1, std::min(settings.maxBounces, 3));

            for (int railIndex = 0; railIndex < railLimit; ++railIndex) {
                Point railHit;
                Point reflected;
                if (!rayRailHit(current, direction, state.table, railHit, reflected)) break;

                route.push_back(railHit);
                const Point reflectedEnd{railHit.x + reflected.x * tableDiag * 1.8,
                                         railHit.y + reflected.y * tableDiag * 1.8};

                for (const int pocketIndex : pocketIndexes) {
                    const Point pocket = pockets[pocketIndex];
                    const Point toPocket{pocket.x - railHit.x, pocket.y - railHit.y};
                    const double forward = dot(toPocket, reflected);
                    const double pocketMiss = pointSegmentDistance(pocket, railHit, reflectedEnd);

                    std::vector<Point> candidateGuide = route;
                    candidateGuide.push_back(pocket);
                    const double bankLen = guideLength(candidateGuide);
                    const double railPenalty = guideBlockPenalty(candidateGuide, state, &ball);
                    double score = miss * 24.0 + projection * 0.28 + bankLen * 0.38 +
                                   pocketMiss * 13.0 + cuePenalty * 9.0 + railPenalty * 5.5 +
                                   railIndex * 45.0;
                    if (forward <= 0.0) score += 700.0;

                    if (score < best.score) {
                        ShotChoice candidate;
                        candidate.cue = cue;
                        candidate.object = ball.center;
                        candidate.pocket = pocket;
                        candidate.ghost = ghost;
                        candidate.objectTarget = route[1];
                        candidate.hasObjectTarget = true;
                        candidate.bankGuide = candidateGuide;
                        candidate.score = score;
                        candidate.pocketIndex = pocketIndex;
                        candidate.ballRadius = radius;
                        best = candidate;
                    }
                }

                current = railHit;
                direction = reflected;
            }
            continue;
        }

        for (const int pocketIndex : pocketIndexes) {
            const Point pocket = pockets[pocketIndex];
            const Point objectPath{pocket.x - ball.center.x, pocket.y - ball.center.y};
            const double objectLen = std::max(kEpsilon, std::hypot(objectPath.x, objectPath.y));
            const Point objectDir{objectPath.x / objectLen, objectPath.y / objectLen};
            const double alignment = dot(impactDir, objectDir);
            const double objectPenalty = pathBlockPenalty(ball.center, pocket, state, &ball, nullptr, 1.06);
            double score = miss * 24.0 + projection * 0.28 + objectLen * 0.52 - alignment * 110.0 +
                           cuePenalty * 9.0 + objectPenalty * 7.0;

            if (settings.shotMode == ShotMode::LongShot) {
                score -= objectLen * 0.18;
                score += miss * 8.0;
            } else if (settings.shotMode == ShotMode::CaromShot) {
                score -= projection * 0.10;
                score += cuePenalty * 3.0;
            }

            if (score < best.score) {
                ShotChoice candidate;
                candidate.cue = cue;
                candidate.object = ball.center;
                candidate.pocket = pocket;
                candidate.ghost = ghost;
                candidate.objectTarget = pocket;
                candidate.hasObjectTarget = true;
                candidate.score = score;
                candidate.pocketIndex = pocketIndex;
                candidate.ballRadius = radius;
                best = candidate;
            }
        }
    }

    if (best.score == std::numeric_limits<double>::max()) {
        best.ballRadius = 0.0;
    }
    return best;
}

PredictionEngine::ShotChoice PredictionEngine::chooseShot(const GameState &state, const Settings &settings, const std::vector<Point> &pockets) {
    const Point cue = state.hasCueBall ? state.cueBall : Point{state.table.x + state.table.width * 0.24, state.table.y + state.table.height * 0.5};
    ShotChoice best;
    best.score = std::numeric_limits<double>::max();

    const std::vector<int> pocketIndexes = settings.manualPocket
        ? std::vector<int>{std::max(0, std::min(settings.selectedPocket, static_cast<int>(pockets.size()) - 1))}
        : std::vector<int>{0, 1, 2, 3, 4, 5};

    for (const auto &ball : state.balls) {
        const double radius = ball.radius > 0.0 ? ball.radius : std::max(8.0, state.table.width * 0.018);
        if (ball.cue || distance(cue, ball.center) < radius * 2.5) continue;

        for (const int pocketIndex : pocketIndexes) {
            const Point pocket = pockets[pocketIndex];
            if (settings.shotMode == ShotMode::BankShot) {
                const auto rails = bankGuides(ball.center, pocket, state.table);
                for (const auto &rail : rails) {
                    if (rail.size() < 3) continue;
                    const Point target = rail[1];
                    const Point ghost = ghostForTarget(ball.center, target, radius);
                    const double cueToGhost = distance(cue, ghost);
                    const Point a{ghost.x - cue.x, ghost.y - cue.y};
                    const Point b{ball.center.x - cue.x, ball.center.y - cue.y};
                    const double directness = std::abs(cross(a, b)) / std::max(1.0, cueToGhost);
                    const double cuePenalty = pathBlockPenalty(cue, ghost, state, nullptr, &ball, 1.18);
                    const double railPenalty = guideBlockPenalty(rail, state, &ball);
                    double score = cueToGhost + guideLength(rail) * 0.62 + directness * 4.0 +
                                   cuePenalty * 9.0 + railPenalty * 7.0 - 230.0;

                    if (score < best.score) {
                        ShotChoice candidate;
                        candidate.cue = cue;
                        candidate.object = ball.center;
                        candidate.pocket = pocket;
                        candidate.ghost = ghost;
                        candidate.objectTarget = target;
                        candidate.hasObjectTarget = true;
                        candidate.bankGuide = rail;
                        candidate.score = score;
                        candidate.pocketIndex = pocketIndex;
                        candidate.ballRadius = radius;
                        best = candidate;
                    }
                }
                continue;
            }

            const double objectToPocket = std::max(kEpsilon, distance(ball.center, pocket));
            const Point ghost = ghostForTarget(ball.center, pocket, radius);
            const double cueToGhost = distance(cue, ghost);
            const Point a{ghost.x - cue.x, ghost.y - cue.y};
            const Point b{ball.center.x - cue.x, ball.center.y - cue.y};
            const double directness = std::abs(cross(a, b)) / std::max(1.0, cueToGhost);
            const double cuePenalty = pathBlockPenalty(cue, ghost, state, nullptr, &ball, 1.18);
            const double objectPenalty = pathBlockPenalty(ball.center, pocket, state, &ball, nullptr, 1.06);
            double score = cueToGhost + objectToPocket * 0.62 + directness * 4.0 +
                           cuePenalty * 9.0 + objectPenalty * 7.0;
            if (settings.shotMode == ShotMode::LongShot) {
                score -= objectToPocket * 0.18;
                score += directness * 2.0;
            } else if (settings.shotMode == ShotMode::CaromShot) {
                score -= cueToGhost * 0.08;
                score += cuePenalty * 3.0;
            }
            if (score < best.score) {
                ShotChoice candidate;
                candidate.cue = cue;
                candidate.object = ball.center;
                candidate.pocket = pocket;
                candidate.ghost = ghost;
                candidate.objectTarget = pocket;
                candidate.hasObjectTarget = true;
                candidate.score = score;
                candidate.pocketIndex = pocketIndex;
                candidate.ballRadius = radius;
                best = candidate;
            }
        }
    }

    if (best.score == std::numeric_limits<double>::max()) {
        best.ballRadius = 0.0;
    }
    return best;
}

} // namespace zg
