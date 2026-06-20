#include "ZGPookingFrameScanner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace zg {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 0.000001;

struct Pixel {
    int r = 0;
    int g = 0;
    int b = 0;
};

struct Component {
    int count = 0;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = 0;
    int maxY = 0;
    double sumX = 0.0;
    double sumY = 0.0;
    double sumR = 0.0;
    double sumG = 0.0;
    double sumB = 0.0;
};

struct TableCandidate {
    bool valid = false;
    Rect rect;
    double confidence = 0.0;
    int feltSamples = 0;
};

struct BallCandidate {
    Ball ball;
    double cueScore = 0.0;
    double objectScore = 0.0;
    double shapeScore = 0.0;
    double aimScore = 0.0;
};

struct AimCandidate {
    bool valid = false;
    double score = 0.0;
    double angle = 0.0;
    Point end;
    int guideHits = 0;
    int cueHits = 0;
    int guideRun = 0;
};

Pixel readPixel(const std::uint8_t *bytes, std::size_t bytesPerRow, int x, int y, PixelFormat format) {
    const std::uint8_t *p = bytes + static_cast<std::size_t>(y) * bytesPerRow + static_cast<std::size_t>(x) * 4;
    if (format == PixelFormat::BGRA8888) {
        return {p[2], p[1], p[0]};
    }
    return {p[0], p[1], p[2]};
}

int max3(const Pixel &p) {
    return std::max(p.r, std::max(p.g, p.b));
}

int min3(const Pixel &p) {
    return std::min(p.r, std::min(p.g, p.b));
}

int brightness(const Pixel &p) {
    return (p.r + p.g + p.b) / 3;
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

double distance(const Point &a, const Point &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

Point lerpPoint(const Point &a, const Point &b, double t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

Rect lerpRect(const Rect &a, const Rect &b, double t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.width + (b.width - a.width) * t,
        a.height + (b.height - a.height) * t
    };
}

Rect clampRect(const Rect &rect, std::size_t width, std::size_t height) {
    const double x = std::max(0.0, std::min(rect.x, static_cast<double>(width - 1)));
    const double y = std::max(0.0, std::min(rect.y, static_cast<double>(height - 1)));
    const double right = std::max(x + 1.0, std::min(rect.x + rect.width, static_cast<double>(width)));
    const double bottom = std::max(y + 1.0, std::min(rect.y + rect.height, static_cast<double>(height)));
    return {x, y, right - x, bottom - y};
}

Point clampPoint(const Point &point, const Rect &rect) {
    return {
        std::max(rect.x, std::min(rect.x + rect.width, point.x)),
        std::max(rect.y, std::min(rect.y + rect.height, point.y))
    };
}

bool insideRect(const Point &point, const Rect &rect, double inset) {
    return point.x >= rect.x + inset &&
           point.x <= rect.x + rect.width - inset &&
           point.y >= rect.y + inset &&
           point.y <= rect.y + rect.height - inset;
}

bool isPookingFelt(const Pixel &p) {
    const int light = brightness(p);
    const int spread = max3(p) - min3(p);
    if (light < 34 || light > 188) return false;
    if (p.r > 168 && p.g > 120 && p.b < 112) return false;
    if (p.r > 150 && p.g < 105 && p.b < 105) return false;

    const bool green = p.g >= p.r + 12 && p.g >= p.b + 6 &&
                       p.r >= 22 && p.r <= 138 &&
                       p.g >= 52 && p.g <= 188 &&
                       p.b >= 14 && p.b <= 138;
    const bool grayBlue = spread <= 84 &&
                          p.g + p.b >= p.r * 2 - 18 &&
                          light >= 48 && light <= 174;
    return green || grayBlue;
}

bool isCueBallPixel(const Pixel &p) {
    const int light = brightness(p);
    const int spread = max3(p) - min3(p);
    return light >= 142 && spread <= 82 && p.r >= 124 && p.g >= 122 && p.b >= 94;
}

bool isObjectBallPixel(const Pixel &p) {
    const int light = brightness(p);
    const int spread = max3(p) - min3(p);
    const bool black = light <= 68 && spread <= 60;
    const bool saturated = spread >= 56 && light >= 42 && light <= 236;
    const bool yellowOrange = p.r >= 122 && p.g >= 86 && p.b <= 116 &&
                              p.r >= p.b + 38 && light >= 68;
    return black || saturated || yellowOrange;
}

bool isBallPixel(const Pixel &p) {
    if (isPookingFelt(p)) return false;
    return isCueBallPixel(p) || isObjectBallPixel(p);
}

bool isGuidePixel(const Pixel &p) {
    const int light = brightness(p);
    const int spread = max3(p) - min3(p);
    const bool pale = light >= 128 && spread <= 92;
    const bool yellow = p.r >= 132 && p.g >= 108 && p.b <= 118 &&
                        p.r >= p.b + 42 && p.g >= p.b + 24;
    const bool cyan = p.g >= 128 && p.b >= 128 && p.r <= 150;
    const bool violet = p.r >= 98 && p.b >= 118 && p.g <= 142;
    return pale || yellow || cyan || violet;
}

bool isCueStickPixel(const Pixel &p) {
    const int light = brightness(p);
    const int spread = max3(p) - min3(p);
    const bool tan = p.r >= 108 && p.g >= 66 && p.b <= 96 &&
                     p.r >= p.b + 38 && p.g >= p.b + 16 &&
                     light >= 68 && light <= 224;
    const bool darkWood = p.r >= 68 && p.g >= 36 && p.b <= 48 &&
                          p.r >= p.g + 7 && p.g >= p.b + 10 &&
                          spread >= 44;
    return tan || darkWood;
}

void extractComponents(const std::vector<std::uint8_t> &mask,
                       int gridW,
                       int gridH,
                       int originX,
                       int originY,
                       int step,
                       const std::uint8_t *bytes,
                       std::size_t bytesPerRow,
                       PixelFormat format,
                       std::vector<Component> &out) {
    std::vector<std::uint8_t> seen(mask.size(), 0);
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy * gridW + gx);
            if (!mask[idx] || seen[idx]) continue;

            Component component;
            std::queue<std::pair<int, int>> queue;
            queue.push({gx, gy});
            seen[idx] = 1;

            while (!queue.empty()) {
                const auto [cx, cy] = queue.front();
                queue.pop();

                const int px = originX + cx * step;
                const int py = originY + cy * step;
                const Pixel pixel = readPixel(bytes, bytesPerRow, px, py, format);
                component.count += 1;
                component.minX = std::min(component.minX, px);
                component.minY = std::min(component.minY, py);
                component.maxX = std::max(component.maxX, px);
                component.maxY = std::max(component.maxY, py);
                component.sumX += px;
                component.sumY += py;
                component.sumR += pixel.r;
                component.sumG += pixel.g;
                component.sumB += pixel.b;

                for (const auto &dir : dirs) {
                    const int nx = cx + dir[0];
                    const int ny = cy + dir[1];
                    if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny * gridW + nx);
                    if (!mask[nidx] || seen[nidx]) continue;
                    seen[nidx] = 1;
                    queue.push({nx, ny});
                }
            }

            out.push_back(component);
        }
    }
}

TableCandidate detectTable(const std::uint8_t *bytes,
                           std::size_t width,
                           std::size_t height,
                           std::size_t bytesPerRow,
                           PixelFormat format,
                           int step) {
    TableCandidate table;
    const int x0 = static_cast<int>(width * 0.02);
    const int y0 = static_cast<int>(height * 0.06);
    const int x1 = static_cast<int>(width * 0.99);
    const int y1 = static_cast<int>(height * 0.92);
    const int gridW = std::max(1, (x1 - x0 + step - 1) / step);
    const int gridH = std::max(1, (y1 - y0 + step - 1) / step);

    std::vector<std::uint8_t> mask(static_cast<std::size_t>(gridW * gridH), 0);
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const int x = x0 + gx * step;
            const int y = y0 + gy * step;
            if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) continue;
            if (isPookingFelt(readPixel(bytes, bytesPerRow, x, y, format))) {
                mask[static_cast<std::size_t>(gy * gridW + gx)] = 1;
                table.feltSamples += 1;
            }
        }
    }

    std::vector<Component> components;
    extractComponents(mask, gridW, gridH, x0, y0, step, bytes, bytesPerRow, format, components);
    if (components.empty()) return table;

    const Component *best = &components.front();
    for (const auto &component : components) {
        if (component.count > best->count) best = &component;
    }

    const double screenSamples = static_cast<double>(width * height) / (step * step);
    const double sampleRatio = best->count / std::max(1.0, screenSamples);
    const double feltWidth = best->maxX - best->minX + step;
    const double feltHeight = best->maxY - best->minY + step;
    const double aspect = feltWidth / std::max(1.0, feltHeight);

    if (best->count < std::max(180, static_cast<int>(screenSamples * 0.018))) return table;
    if (feltWidth < width * 0.34 || feltHeight < height * 0.24) return table;
    if (aspect < 1.15) return table;

    const double expandX = std::max(12.0, feltWidth * 0.032);
    const double expandY = std::max(10.0, feltHeight * 0.040);
    table.rect = clampRect({
        best->minX - expandX,
        best->minY - expandY,
        feltWidth + expandX * 2.0,
        feltHeight + expandY * 2.0
    }, width, height);
    table.confidence = clamp01(0.48 + sampleRatio * 3.8 + clamp01((aspect - 1.20) / 1.8) * 0.18);
    table.valid = table.confidence >= 0.55;
    return table;
}

double roundness(const Component &component, int step) {
    const double w = component.maxX - component.minX + step;
    const double h = component.maxY - component.minY + step;
    const double shortLong = std::min(w, h) / std::max(1.0, std::max(w, h));
    const double area = component.count * step * step;
    const double radius = std::sqrt(area / kPi);
    const double diameter = std::max(1.0, radius * 2.0);
    const double diameterFit = 1.0 - std::min(1.0, std::abs(std::max(w, h) - diameter) / diameter);
    return clamp01(shortLong * 0.70 + diameterFit * 0.30);
}

std::vector<BallCandidate> detectBalls(const std::uint8_t *bytes,
                                       std::size_t width,
                                       std::size_t height,
                                       std::size_t bytesPerRow,
                                       PixelFormat format,
                                       const Rect &table,
                                       int step,
                                       int maxBalls) {
    const int x0 = std::max(0, static_cast<int>(table.x + table.width * 0.030));
    const int y0 = std::max(0, static_cast<int>(table.y + table.height * 0.040));
    const int x1 = std::min(static_cast<int>(width), static_cast<int>(table.x + table.width * 0.970));
    const int y1 = std::min(static_cast<int>(height), static_cast<int>(table.y + table.height * 0.960));
    const int gridW = std::max(1, (x1 - x0 + step - 1) / step);
    const int gridH = std::max(1, (y1 - y0 + step - 1) / step);

    std::vector<std::uint8_t> mask(static_cast<std::size_t>(gridW * gridH), 0);
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const int x = x0 + gx * step;
            const int y = y0 + gy * step;
            if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) continue;
            const Pixel pixel = readPixel(bytes, bytesPerRow, x, y, format);
            if (isBallPixel(pixel)) {
                mask[static_cast<std::size_t>(gy * gridW + gx)] = 1;
            }
        }
    }

    std::vector<Component> components;
    extractComponents(mask, gridW, gridH, x0, y0, step, bytes, bytesPerRow, format, components);

    const double minRadius = std::max(4.0, table.width * 0.0070);
    const double maxRadius = std::max(minRadius + 4.0, table.width * 0.0360);
    std::vector<BallCandidate> balls;

    for (const auto &component : components) {
        const double area = component.count * step * step;
        const double radius = std::sqrt(area / kPi);
        const double shape = roundness(component, step);
        const double w = component.maxX - component.minX + step;
        const double h = component.maxY - component.minY + step;
        const double fill = area / std::max(1.0, w * h);
        if (radius < minRadius || radius > maxRadius || shape < 0.30 || fill < 0.12) continue;

        const double inv = 1.0 / std::max(1, component.count);
        const Point center{component.sumX * inv, component.sumY * inv};
        if (!insideRect(center, table, std::max(5.0, table.width * 0.008))) continue;

        const double r = component.sumR * inv;
        const double g = component.sumG * inv;
        const double b = component.sumB * inv;
        const double light = (r + g + b) / 3.0;
        const double spread = std::max(r, std::max(g, b)) - std::min(r, std::min(g, b));

        BallCandidate candidate;
        candidate.ball.center = center;
        candidate.ball.radius = std::max(minRadius, std::min(maxRadius, radius * 1.10));
        candidate.ball.number = static_cast<int>(balls.size());
        candidate.ball.cue = false;
        candidate.shapeScore = shape;
        candidate.cueScore = clamp01((light - 112.0) / 112.0) * clamp01((92.0 - spread) / 92.0) * shape;
        candidate.objectScore = std::max(clamp01((spread - 38.0) / 130.0), clamp01((78.0 - light) / 78.0)) * shape;
        balls.push_back(candidate);
    }

    std::sort(balls.begin(), balls.end(), [](const BallCandidate &a, const BallCandidate &b) {
        const double scoreA = std::max(a.cueScore, a.objectScore) + a.shapeScore * 0.35;
        const double scoreB = std::max(b.cueScore, b.objectScore) + b.shapeScore * 0.35;
        return scoreA > scoreB;
    });

    const std::size_t keep = static_cast<std::size_t>(std::max(1, maxBalls));
    if (balls.size() > keep) balls.resize(keep);
    return balls;
}

bool lineSampleHit(const std::uint8_t *bytes,
                   std::size_t width,
                   std::size_t height,
                   std::size_t bytesPerRow,
                   PixelFormat format,
                   const Point &point,
                   double nx,
                   double ny,
                   bool guide) {
    int hits = 0;
    for (int side = -2; side <= 2; ++side) {
        const int x = static_cast<int>(std::round(point.x + nx * side * 2.6));
        const int y = static_cast<int>(std::round(point.y + ny * side * 2.6));
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) continue;
        const Pixel pixel = readPixel(bytes, bytesPerRow, x, y, format);
        if (guide ? isGuidePixel(pixel) : isCueStickPixel(pixel)) hits += 1;
    }
    return hits > 0;
}

AimCandidate probeAim(const std::uint8_t *bytes,
                      std::size_t width,
                      std::size_t height,
                      std::size_t bytesPerRow,
                      PixelFormat format,
                      const Rect &table,
                      const std::vector<BallCandidate> &balls,
                      const Point &cue,
                      double angle,
                      int step) {
    AimCandidate aim;
    aim.angle = angle;
    const double dx = std::cos(angle);
    const double dy = std::sin(angle);
    const double nx = -dy;
    const double ny = dx;
    const double diag = std::hypot(table.width, table.height);
    const double cueRadius = std::max(6.0, table.width * 0.016);
    double farthestGuide = 0.0;
    int gaps = 0;
    int run = 0;

    for (double d = cueRadius * 1.05; d <= diag; d += std::max(2, step)) {
        Point p{cue.x + dx * d, cue.y + dy * d};
        if (!insideRect(p, table, std::max(4.0, table.width * 0.006))) {
            if (aim.guideHits > 0) break;
            continue;
        }

        bool crossedObject = false;
        for (const auto &ball : balls) {
            if (distance(ball.ball.center, cue) < cueRadius * 1.4) continue;
            if (distance(p, ball.ball.center) <= ball.ball.radius + 4.0) {
                crossedObject = true;
                break;
            }
        }
        if (crossedObject && aim.guideHits > 0) break;

        if (lineSampleHit(bytes, width, height, bytesPerRow, format, p, nx, ny, true)) {
            aim.guideHits += 1;
            run += 1;
            aim.guideRun = std::max(aim.guideRun, run);
            farthestGuide = d;
            aim.score += 3.4 + run * 0.55 + d * 0.009;
            gaps = 0;
        } else {
            run = 0;
            if (++gaps > 18 && aim.guideHits > 0) break;
        }
    }

    for (double d = cueRadius * 1.4; d <= diag * 0.55; d += std::max(2, step)) {
        Point p{cue.x - dx * d, cue.y - dy * d};
        if (p.x < 0 || p.y < 0 || p.x >= static_cast<double>(width) || p.y >= static_cast<double>(height)) break;
        if (lineSampleHit(bytes, width, height, bytesPerRow, format, p, nx, ny, false)) {
            aim.cueHits += 1;
            aim.score += 1.7 + d * 0.004;
        }
    }

    if (aim.guideHits >= 3 || (aim.guideHits >= 1 && aim.cueHits >= 5) || aim.cueHits >= 12) {
        aim.valid = true;
        if (aim.guideRun >= 3) aim.score += 18.0;
        if (aim.cueHits >= 6) aim.score += 14.0;
        const double reach = std::max(farthestGuide + table.width * 0.22, table.width * 0.36);
        aim.end = clampPoint({cue.x + dx * reach, cue.y + dy * reach}, table);
    }
    return aim;
}

GuideLine detectAim(const std::uint8_t *bytes,
                    std::size_t width,
                    std::size_t height,
                    std::size_t bytesPerRow,
                    PixelFormat format,
                    const Rect &table,
                    std::vector<BallCandidate> &balls,
                    int step) {
    GuideLine guide;
    if (balls.empty()) return guide;

    std::size_t bestCue = std::numeric_limits<std::size_t>::max();
    AimCandidate bestAim;
    const double angleStep = kPi / 144.0;

    for (std::size_t i = 0; i < balls.size(); ++i) {
        const double cuePrior = balls[i].cueScore * 28.0 + balls[i].shapeScore * 6.0;
        if (cuePrior < 3.5) continue;

        for (int a = 0; a < 288; ++a) {
            AimCandidate aim = probeAim(bytes, width, height, bytesPerRow, format, table, balls, balls[i].ball.center, a * angleStep, step);
            if (!aim.valid) continue;
            aim.score += cuePrior;
            if (aim.score > bestAim.score) {
                bestAim = aim;
                bestCue = i;
            }
        }
    }

    if (bestCue != std::numeric_limits<std::size_t>::max() && bestAim.score > 24.0) {
        for (auto &ball : balls) ball.ball.cue = false;
        balls[bestCue].ball.cue = true;
        balls[bestCue].aimScore = bestAim.score;
        guide.valid = true;
        guide.start = balls[bestCue].ball.center;
        guide.end = bestAim.end;
        return guide;
    }

    std::size_t fallback = std::numeric_limits<std::size_t>::max();
    double fallbackScore = -1.0;
    for (std::size_t i = 0; i < balls.size(); ++i) {
        const double score = balls[i].cueScore * 18.0 + balls[i].shapeScore * 2.0;
        if (score > fallbackScore) {
            fallbackScore = score;
            fallback = i;
        }
    }
    if (fallback != std::numeric_limits<std::size_t>::max() && fallbackScore > 5.5) {
        balls[fallback].ball.cue = true;
    }
    return guide;
}

double adaptiveBlend(double movement, double snapDistance, double baseSmoothing, double confidence) {
    if (movement >= snapDistance || snapDistance <= kEpsilon) return 1.0;
    const double movementBoost = clamp01(movement / snapDistance) * 0.52;
    const double confidenceBoost = clamp01(confidence) * 0.16;
    return clamp01(baseSmoothing + movementBoost + confidenceBoost);
}

} // namespace

void FrameStabilizer::reset() {
    hasState_ = false;
    state_ = {};
}

GameState FrameStabilizer::update(const GameState &raw, double confidence, const FrameStabilizerOptions &options) {
    if (!options.enabled || !hasState_ || raw.table.width <= 0.0 || raw.table.height <= 0.0) {
        state_ = raw;
        hasState_ = true;
        return state_;
    }

    const double diag = std::max(kEpsilon, std::hypot(raw.table.width, raw.table.height));
    const double snap = std::max(18.0, diag * options.snapDistanceRatio);
    const double base = clamp01(options.baseSmoothing);

    GameState next = raw;
    const double tableMove = distance({raw.table.x, raw.table.y}, {state_.table.x, state_.table.y}) +
                             std::abs(raw.table.width - state_.table.width) +
                             std::abs(raw.table.height - state_.table.height);
    next.table = lerpRect(state_.table, raw.table, adaptiveBlend(tableMove, snap, base * 0.70, confidence));

    if (raw.hasCueBall && state_.hasCueBall) {
        const double cueMove = distance(raw.cueBall, state_.cueBall);
        next.cueBall = lerpPoint(state_.cueBall, raw.cueBall, adaptiveBlend(cueMove, snap * 0.55, base, confidence));
        next.hasCueBall = true;
    }

    next.guide = raw.guide;
    if (raw.guide.valid && state_.guide.valid) {
        const double guideMove = distance(raw.guide.end, state_.guide.end);
        next.guide.start = next.hasCueBall ? next.cueBall : raw.guide.start;
        next.guide.end = lerpPoint(state_.guide.end, raw.guide.end, adaptiveBlend(guideMove, snap * 0.72, base, confidence));
    }

    next.balls.clear();
    const double matchDistance = std::max(22.0, diag * options.maxBallMatchDistanceRatio);
    std::vector<bool> used(state_.balls.size(), false);
    for (const auto &rawBall : raw.balls) {
        double best = matchDistance;
        std::size_t bestIndex = state_.balls.size();
        for (std::size_t i = 0; i < state_.balls.size(); ++i) {
            if (used[i] || rawBall.cue != state_.balls[i].cue) continue;
            const double d = distance(rawBall.center, state_.balls[i].center);
            if (d < best) {
                best = d;
                bestIndex = i;
            }
        }

        Ball ball = rawBall;
        if (bestIndex != state_.balls.size()) {
            used[bestIndex] = true;
            const double blend = adaptiveBlend(best, snap * 0.58, base, confidence);
            ball.center = lerpPoint(state_.balls[bestIndex].center, rawBall.center, blend);
            ball.radius = state_.balls[bestIndex].radius + (rawBall.radius - state_.balls[bestIndex].radius) * blend;
        }
        if (ball.cue && next.hasCueBall) ball.center = next.cueBall;
        next.balls.push_back(ball);
    }

    state_ = next;
    hasState_ = true;
    return state_;
}

FrameScanResult FrameScanner::scan(const std::uint8_t *bytes,
                                   std::size_t width,
                                   std::size_t height,
                                   std::size_t bytesPerRow,
                                   const FrameScanOptions &options) {
    FrameScanResult result;
    if (!bytes || width < 120 || height < 120 || bytesPerRow < width * 4) return result;

    const int step = std::max(2, std::min(5, options.sampleStep));
    const TableCandidate table = detectTable(bytes, width, height, bytesPerRow, options.pixelFormat, step);
    if (!table.valid) return result;

    std::vector<BallCandidate> candidates = detectBalls(bytes,
                                                        width,
                                                        height,
                                                        bytesPerRow,
                                                        options.pixelFormat,
                                                        table.rect,
                                                        step,
                                                        options.maxBalls);
    if (candidates.empty()) return result;

    GameState state;
    state.table = table.rect;
    state.guide = detectAim(bytes, width, height, bytesPerRow, options.pixelFormat, table.rect, candidates, step);

    std::sort(candidates.begin(), candidates.end(), [](const BallCandidate &a, const BallCandidate &b) {
        if (a.ball.cue != b.ball.cue) return a.ball.cue;
        const double scoreA = std::max(a.cueScore, a.objectScore) + a.shapeScore * 0.35 + a.aimScore * 0.010;
        const double scoreB = std::max(b.cueScore, b.objectScore) + b.shapeScore * 0.35 + b.aimScore * 0.010;
        return scoreA > scoreB;
    });

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        candidates[i].ball.number = static_cast<int>(i);
        state.balls.push_back(candidates[i].ball);
        if (candidates[i].ball.cue) {
            state.hasCueBall = true;
            state.cueBall = candidates[i].ball.center;
            state.guide.start = state.cueBall;
        }
    }

    if (!state.hasCueBall) return result;

    result.valid = true;
    result.state = state;
    result.confidence = std::min(0.99,
                                 table.confidence * 0.58 +
                                 std::min(0.22, state.balls.size() * 0.022) +
                                 (state.guide.valid ? 0.20 : 0.04));
    return result;
}

} // namespace zg
