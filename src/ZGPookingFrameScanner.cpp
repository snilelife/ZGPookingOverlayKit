#include "ZGPookingFrameScanner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace zg {
namespace {
constexpr double kScanEpsilon = 0.0001;

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

struct RayProbe {
    double score = 0.0;
    Point end;
    int hits = 0;
    int longestRun = 0;
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

bool isTablePixel(const Pixel &p) {
    const int hi = max3(p);
    const int lo = min3(p);
    const int spread = hi - lo;
    const int light = brightness(p);

    if (light < 42 || light > 226) return false;
    if (p.r > 185 && p.g > 145 && p.b < 110) return false;
    if (p.r > 175 && p.g < 105 && p.b < 105) return false;

    const bool grayBlueFelt = spread < 78 && p.b + p.g >= p.r * 2 - 18;
    const bool greenFelt = p.g > p.r + 16 && p.g >= p.b - 22;
    const bool blueFelt = p.b > p.r + 12 && p.g > p.r - 14;
    return grayBlueFelt || greenFelt || blueFelt;
}

bool isBallLikePixel(const Pixel &p) {
    const int hi = max3(p);
    const int lo = min3(p);
    const int spread = hi - lo;
    const int light = brightness(p);
    const bool whiteBall = light > 176 && spread < 48;
    const bool blackBall = light < 58 && spread < 45;
    const bool coloredBall = spread > 48 && light > 48 && light < 244;
    return whiteBall || blackBall || coloredBall;
}

bool isWhiteBallPixel(const Pixel &p) {
    const int hi = max3(p);
    const int lo = min3(p);
    const int spread = hi - lo;
    const int light = brightness(p);
    return light > 176 && spread < 52;
}

bool isStrongColoredBallPixel(const Pixel &p) {
    const int hi = max3(p);
    const int lo = min3(p);
    const int spread = hi - lo;
    const int light = brightness(p);
    return spread > 88 && light > 48 && light < 238;
}

bool isPaleGuidePixel(const Pixel &p) {
    const int hi = max3(p);
    const int lo = min3(p);
    const int spread = hi - lo;
    const int light = brightness(p);
    return light > 154 && spread < 76;
}

bool isGuideLikePixel(const Pixel &p) {
    const bool paleGuide = isPaleGuidePixel(p);
    const bool cyanGuide = p.g > 136 && p.b > 136 && p.r < 146;
    const bool violetGuide = p.b > 124 && p.r > 102 && p.g < 132;
    return paleGuide || cyanGuide || violetGuide;
}

bool isTexturedGuideHit(const std::uint8_t *bytes,
                        std::size_t width,
                        std::size_t height,
                        std::size_t bytesPerRow,
                        PixelFormat format,
                        int x,
                        int y,
                        double nx,
                        double ny) {
    const Pixel center = readPixel(bytes, bytesPerRow, x, y, format);
    if (!isGuideLikePixel(center)) return false;
    if (!isTablePixel(center)) return true;

    int neighborSum = 0;
    int neighborCount = 0;
    for (int side : {-1, 1}) {
        for (int offset : {5, 8}) {
            const int sx = static_cast<int>(std::round(x + nx * side * offset));
            const int sy = static_cast<int>(std::round(y + ny * side * offset));
            if (sx < 0 || sy < 0 || sx >= static_cast<int>(width) || sy >= static_cast<int>(height)) continue;
            neighborSum += brightness(readPixel(bytes, bytesPerRow, sx, sy, format));
            neighborCount += 1;
        }
    }

    if (neighborCount == 0) {
        return brightness(center) > 210;
    }
    const double neighborLight = neighborSum / static_cast<double>(neighborCount);
    return brightness(center) >= neighborLight + 16.0 || brightness(center) > 214;
}

Rect clampRect(const Rect &rect, std::size_t width, std::size_t height) {
    const double x = std::max(0.0, std::min(rect.x, static_cast<double>(width - 1)));
    const double y = std::max(0.0, std::min(rect.y, static_cast<double>(height - 1)));
    const double right = std::max(x + 1.0, std::min(rect.x + rect.width, static_cast<double>(width)));
    const double bottom = std::max(y + 1.0, std::min(rect.y + rect.height, static_cast<double>(height)));
    return {x, y, right - x, bottom - y};
}

bool insideRect(const Point &p, const Rect &rect, double inset) {
    return p.x >= rect.x + inset &&
           p.x <= rect.x + rect.width - inset &&
           p.y >= rect.y + inset &&
           p.y <= rect.y + rect.height - inset;
}

double pointDistance(const Point &a, const Point &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

bool nearAnyBall(const Point &p, const std::vector<Ball> &balls, double extra) {
    for (const auto &ball : balls) {
        const double radius = ball.radius > 0.0 ? ball.radius : 10.0;
        if (pointDistance(p, ball.center) <= radius + extra) {
            return true;
        }
    }
    return false;
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
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

double adaptiveBlend(double movement, double snapDistance, double baseSmoothing, double confidence) {
    if (snapDistance <= kScanEpsilon || movement >= snapDistance) {
        return 1.0;
    }
    const double normalized = clamp01(movement / snapDistance);
    const double confidenceBoost = clamp01(confidence) * 0.18;
    return clamp01(baseSmoothing + normalized * 0.48 + confidenceBoost);
}

void extractComponents(const std::vector<std::uint8_t> &mask,
                       int gridW,
                       int gridH,
                       int ix0,
                       int iy0,
                       int step,
                       const std::uint8_t *bytes,
                       std::size_t bytesPerRow,
                       PixelFormat pixelFormat,
                       double maxRatio,
                       double minFill,
                       std::vector<Component> &components) {
    std::vector<std::uint8_t> visited(mask.size(), 0);
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy * gridW + gx);
            if (!mask[idx] || visited[idx]) continue;

            Component c;
            std::queue<std::pair<int, int>> q;
            q.push({gx, gy});
            visited[idx] = 1;

            while (!q.empty()) {
                const auto [cx, cy] = q.front();
                q.pop();
                const int px = ix0 + cx * step;
                const int py = iy0 + cy * step;
                const Pixel p = readPixel(bytes, bytesPerRow, px, py, pixelFormat);
                c.count += 1;
                c.minX = std::min(c.minX, px);
                c.minY = std::min(c.minY, py);
                c.maxX = std::max(c.maxX, px);
                c.maxY = std::max(c.maxY, py);
                c.sumX += px;
                c.sumY += py;
                c.sumR += p.r;
                c.sumG += p.g;
                c.sumB += p.b;

                for (const auto &dir : dirs) {
                    const int nx = cx + dir[0];
                    const int ny = cy + dir[1];
                    if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny * gridW + nx);
                    if (!mask[nidx] || visited[nidx]) continue;
                    visited[nidx] = 1;
                    q.push({nx, ny});
                }
            }

            const int pixelArea = c.count * step * step;
            const int bw = c.maxX - c.minX + step;
            const int bh = c.maxY - c.minY + step;
            const double ratio = static_cast<double>(std::max(bw, bh)) / std::max(1, std::min(bw, bh));
            const double fill = pixelArea / static_cast<double>(std::max(1, bw * bh));
            if (pixelArea >= 30 && pixelArea <= 2200 && ratio <= maxRatio && fill >= minFill) {
                components.push_back(c);
            }
        }
    }
}

RayProbe probeGuideRay(const std::uint8_t *bytes,
                       std::size_t width,
                       std::size_t height,
                       std::size_t bytesPerRow,
                       const FrameScanOptions &options,
                       const GameState &state,
                       double angle,
                       int step) {
    RayProbe probe;
    const double dx = std::cos(angle);
    const double dy = std::sin(angle);
    const double nx = -dy;
    const double ny = dx;
    const double maxReach = std::hypot(state.table.width, state.table.height);
    const double startDistance = std::max(14.0, state.table.width * 0.024);
    const double inset = std::max(4.0, state.table.width * 0.012);
    const int rayStep = std::max(2, step);
    int currentRun = 0;
    int gaps = 0;
    double farthestHit = 0.0;

    for (double d = startDistance; d <= maxReach; d += rayStep) {
        const Point center{state.cueBall.x + dx * d, state.cueBall.y + dy * d};
        if (!insideRect(center, state.table, inset)) {
            if (probe.hits > 0) break;
            continue;
        }
        if (nearAnyBall(center, state.balls, std::max(2.0, state.table.width * 0.004))) {
            currentRun = 0;
            gaps += 1;
            continue;
        }

        int localHits = 0;
        for (int side = -2; side <= 2; ++side) {
            const int sx = static_cast<int>(std::round(center.x + nx * side * 2.0));
            const int sy = static_cast<int>(std::round(center.y + ny * side * 2.0));
            if (sx < 0 || sy < 0 || sx >= static_cast<int>(width) || sy >= static_cast<int>(height)) continue;
            if (isTexturedGuideHit(bytes, width, height, bytesPerRow, options.pixelFormat, sx, sy, nx, ny)) {
                localHits += 1;
            }
        }

        if (localHits > 0) {
            probe.hits += localHits;
            currentRun += 1;
            probe.longestRun = std::max(probe.longestRun, currentRun);
            farthestHit = d;
            probe.score += localHits * 3.0 + currentRun * 0.55 + d * 0.010;
            gaps = 0;
        } else {
            currentRun = 0;
            if (++gaps > 18 && probe.hits > 0) {
                break;
            }
        }
    }

    if (probe.hits > 0) {
        const double reach = std::max(farthestHit + state.table.width * 0.18,
                                      state.table.width * 0.34);
        probe.end = {state.cueBall.x + dx * reach, state.cueBall.y + dy * reach};
        probe.end.x = std::max(state.table.x, std::min(state.table.x + state.table.width, probe.end.x));
        probe.end.y = std::max(state.table.y, std::min(state.table.y + state.table.height, probe.end.y));
    }
    return probe;
}

GuideLine detectGuideLine(const std::uint8_t *bytes,
                          std::size_t width,
                          std::size_t height,
                          std::size_t bytesPerRow,
                          const FrameScanOptions &options,
                          const GameState &state,
                          int step) {
    GuideLine guide;
    if (!state.hasCueBall || state.table.width <= 0.0 || state.table.height <= 0.0) {
        return guide;
    }

    RayProbe best;
    const double angleStep = 3.141592653589793 / 90.0;
    for (int i = 0; i < 90; ++i) {
        const double angle = i * angleStep;
        RayProbe forward = probeGuideRay(bytes, width, height, bytesPerRow, options, state, angle, step);
        if (forward.score > best.score) best = forward;
        RayProbe backward = probeGuideRay(bytes, width, height, bytesPerRow, options, state, angle + 3.141592653589793, step);
        if (backward.score > best.score) best = backward;
    }

    const double minScore = std::max(34.0, state.table.width * 0.075);
    if (best.score >= minScore && best.hits >= 8 && best.longestRun >= 3) {
        guide.valid = true;
        guide.start = state.cueBall;
        guide.end = best.end;
    }
    return guide;
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

    const double diagonal = std::max(kScanEpsilon, std::hypot(raw.table.width, raw.table.height));
    const double snapDistance = std::max(18.0, diagonal * options.snapDistanceRatio);
    const double base = clamp01(options.baseSmoothing);
    GameState next = raw;

    const Point rawTableOrigin{raw.table.x, raw.table.y};
    const Point oldTableOrigin{state_.table.x, state_.table.y};
    const double tableMovement = pointDistance(rawTableOrigin, oldTableOrigin) +
                                 std::abs(raw.table.width - state_.table.width) +
                                 std::abs(raw.table.height - state_.table.height);
    const double tableBlend = adaptiveBlend(tableMovement, snapDistance, base * 0.72, confidence);
    next.table = lerpRect(state_.table, raw.table, tableBlend);

    if (raw.hasCueBall && state_.hasCueBall) {
        const double cueMovement = pointDistance(raw.cueBall, state_.cueBall);
        next.cueBall = lerpPoint(state_.cueBall, raw.cueBall, adaptiveBlend(cueMovement, snapDistance * 0.62, base, confidence));
        next.hasCueBall = true;
    } else {
        next.cueBall = raw.cueBall;
        next.hasCueBall = raw.hasCueBall;
    }

    next.guide = raw.guide;
    if (raw.guide.valid && state_.guide.valid) {
        const double guideMovement = pointDistance(raw.guide.end, state_.guide.end);
        const double guideBlend = adaptiveBlend(guideMovement, snapDistance, base, confidence);
        next.guide.start = next.hasCueBall ? next.cueBall : lerpPoint(state_.guide.start, raw.guide.start, guideBlend);
        next.guide.end = lerpPoint(state_.guide.end, raw.guide.end, guideBlend);
    }

    next.balls.clear();
    const double matchDistance = std::max(24.0, diagonal * options.maxBallMatchDistanceRatio);
    std::vector<bool> used(state_.balls.size(), false);
    for (const auto &rawBall : raw.balls) {
        double best = matchDistance;
        std::size_t bestIndex = state_.balls.size();
        for (std::size_t i = 0; i < state_.balls.size(); ++i) {
            if (used[i]) continue;
            if (rawBall.cue != state_.balls[i].cue) continue;
            const double d = pointDistance(rawBall.center, state_.balls[i].center);
            if (d < best) {
                best = d;
                bestIndex = i;
            }
        }

        Ball ball = rawBall;
        if (bestIndex != state_.balls.size()) {
            used[bestIndex] = true;
            const double blend = adaptiveBlend(best, snapDistance * 0.58, base, confidence);
            ball.center = lerpPoint(state_.balls[bestIndex].center, rawBall.center, blend);
            ball.radius = state_.balls[bestIndex].radius + (rawBall.radius - state_.balls[bestIndex].radius) * blend;
            if (ball.cue && next.hasCueBall) {
                ball.center = next.cueBall;
            }
        }
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
    if (!bytes || width < 80 || height < 120 || bytesPerRow < width * 4) {
        return result;
    }

    const int step = std::max(1, options.sampleStep);
    const int x0 = static_cast<int>(width * 0.05);
    const int x1 = static_cast<int>(width * 0.95);
    const int y0 = static_cast<int>(height * 0.08);
    const int y1 = static_cast<int>(height * 0.94);

    std::vector<int> colHits(width, 0);
    std::vector<int> rowHits(height, 0);
    int tableHits = 0;

    for (int y = y0; y < y1; y += step) {
        for (int x = x0; x < x1; x += step) {
            const Pixel p = readPixel(bytes, bytesPerRow, x, y, options.pixelFormat);
            if (!isTablePixel(p)) continue;
            colHits[x] += 1;
            rowHits[y] += 1;
            tableHits += 1;
        }
    }

    const int colThreshold = std::max(3, (y1 - y0) / (step * 18));
    const int rowThreshold = std::max(3, (x1 - x0) / (step * 18));
    int minX = x1;
    int maxX = x0;
    int minY = y1;
    int maxY = y0;

    for (int x = x0; x < x1; ++x) {
        if (colHits[x] >= colThreshold) {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
        }
    }
    for (int y = y0; y < y1; ++y) {
        if (rowHits[y] >= rowThreshold) {
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }

    if (tableHits < static_cast<int>((width * height) / (step * step * 34)) ||
        maxX <= minX + 40 || maxY <= minY + 80) {
        return result;
    }

    Rect table = clampRect({static_cast<double>(minX),
                            static_cast<double>(minY),
                            static_cast<double>(maxX - minX),
                            static_cast<double>(maxY - minY)}, width, height);

    const double insetX = table.width * 0.055;
    const double insetY = table.height * 0.060;
    const int ix0 = static_cast<int>(table.x + insetX);
    const int iy0 = static_cast<int>(table.y + insetY);
    const int ix1 = static_cast<int>(table.x + table.width - insetX);
    const int iy1 = static_cast<int>(table.y + table.height - insetY);
    const int gridW = std::max(1, (ix1 - ix0 + step - 1) / step);
    const int gridH = std::max(1, (iy1 - iy0 + step - 1) / step);
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(gridW * gridH), 0);
    std::vector<std::uint8_t> whiteMask(static_cast<std::size_t>(gridW * gridH), 0);

    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const int x = ix0 + gx * step;
            const int y = iy0 + gy * step;
            if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) continue;
            const Pixel p = readPixel(bytes, bytesPerRow, x, y, options.pixelFormat);
            if (((isBallLikePixel(p) && !isTablePixel(p)) || isStrongColoredBallPixel(p)) && !isPaleGuidePixel(p)) {
                mask[static_cast<std::size_t>(gy * gridW + gx)] = 1;
            }
            if (isWhiteBallPixel(p) && !isTablePixel(p)) {
                whiteMask[static_cast<std::size_t>(gy * gridW + gx)] = 1;
            }
        }
    }

    std::vector<Component> components;
    extractComponents(mask, gridW, gridH, ix0, iy0, step, bytes, bytesPerRow, options.pixelFormat, 2.35, 0.18, components);
    extractComponents(whiteMask, gridW, gridH, ix0, iy0, step, bytes, bytesPerRow, options.pixelFormat, 1.75, 0.34, components);

    std::sort(components.begin(), components.end(), [](const Component &a, const Component &b) {
        return a.count > b.count;
    });

    GameState state;
    state.table = table;
    state.guide.valid = false;
    const std::size_t maxBalls = static_cast<std::size_t>(std::max(1, options.maxBalls));
    std::size_t cueIndex = std::numeric_limits<std::size_t>::max();
    double cueScore = -1.0;

    for (std::size_t i = 0; i < std::min(maxBalls, components.size()); ++i) {
        const Component &c = components[i];
        const double inv = 1.0 / std::max(1, c.count);
        const Point center{c.sumX * inv, c.sumY * inv};
        const double radius = std::max(5.0, std::sqrt((c.count * step * step) / 3.141592653589793));
        const double r = c.sumR * inv;
        const double g = c.sumG * inv;
        const double b = c.sumB * inv;
        const double brightness = (r + g + b) / 3.0;
        const double spread = std::max(r, std::max(g, b)) - std::min(r, std::min(g, b));

        Ball ball;
        ball.center = center;
        ball.radius = radius;
        ball.number = static_cast<int>(i);
        ball.cue = false;
        state.balls.push_back(ball);

        const double whiteScore = (brightness > 162.0 && spread < 58.0)
            ? center.y + brightness * 0.20 - spread
            : -1.0;
        if (whiteScore > cueScore) {
            cueScore = whiteScore;
            cueIndex = state.balls.size() - 1;
        }
    }

    if (cueIndex != std::numeric_limits<std::size_t>::max()) {
        state.hasCueBall = true;
        state.cueBall = state.balls[cueIndex].center;
        state.balls[cueIndex].cue = true;
    }

    state.guide = detectGuideLine(bytes, width, height, bytesPerRow, options, state, step);

    result.valid = true;
    result.state = state;
    result.confidence = std::min(0.98, 0.40 + std::min(0.35, tableHits / static_cast<double>((x1 - x0) * (y1 - y0) / (step * step) + 1)) +
                                      std::min(0.23, state.balls.size() * 0.018));
    return result;
}

} // namespace zg
