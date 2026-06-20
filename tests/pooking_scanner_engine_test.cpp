#include "ZGPookingEngine.hpp"
#include "ZGPookingFrameScanner.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr int kWidth = 600;
constexpr int kHeight = 1000;
constexpr int kChannels = 4;

double dist(const zg::Point &a, const zg::Point &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

void putPixel(std::vector<std::uint8_t> &frame, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    const std::size_t index = static_cast<std::size_t>((y * kWidth + x) * kChannels);
    frame[index + 0] = r;
    frame[index + 1] = g;
    frame[index + 2] = b;
    frame[index + 3] = 255;
}

void fillRect(std::vector<std::uint8_t> &frame, int x, int y, int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            putPixel(frame, xx, yy, r, g, b);
        }
    }
}

void fillCircle(std::vector<std::uint8_t> &frame, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (int yy = cy - radius; yy <= cy + radius; ++yy) {
        for (int xx = cx - radius; xx <= cx + radius; ++xx) {
            const int dx = xx - cx;
            const int dy = yy - cy;
            if (dx * dx + dy * dy <= radius * radius) {
                putPixel(frame, xx, yy, r, g, b);
            }
        }
    }
}

void drawLine(std::vector<std::uint8_t> &frame,
              zg::Point start,
              zg::Point end,
              int thickness,
              std::uint8_t r,
              std::uint8_t g,
              std::uint8_t b) {
    const int steps = static_cast<int>(std::max(std::abs(end.x - start.x), std::abs(end.y - start.y)));
    for (int i = 0; i <= steps; ++i) {
        const double t = steps == 0 ? 0.0 : static_cast<double>(i) / steps;
        const int x = static_cast<int>(std::round(start.x + (end.x - start.x) * t));
        const int y = static_cast<int>(std::round(start.y + (end.y - start.y) * t));
        fillCircle(frame, x, y, thickness, r, g, b);
    }
}

std::vector<std::uint8_t> makeFrame(zg::Point guideEnd,
                                    std::uint8_t guideR = 235,
                                    std::uint8_t guideG = 238,
                                    std::uint8_t guideB = 242) {
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(kWidth * kHeight * kChannels), 255);
    fillRect(frame, 0, 0, kWidth, kHeight, 15, 17, 26);
    fillRect(frame, 90, 145, 420, 720, 86, 106, 118);
    fillRect(frame, 115, 180, 370, 650, 98, 116, 128);

    const zg::Point cue{300, 755};
    const double dx = guideEnd.x - cue.x;
    const double dy = guideEnd.y - cue.y;
    const double len = std::max(1.0, std::hypot(dx, dy));
    const zg::Point guideStart{cue.x + dx / len * 28.0, cue.y + dy / len * 28.0};
    drawLine(frame, guideStart, guideEnd, 2, guideR, guideG, guideB);

    fillCircle(frame, 300, 755, 13, 238, 236, 222);
    fillCircle(frame, 415, 467, 12, 226, 70, 68);
    fillCircle(frame, 228, 460, 12, 72, 205, 96);
    fillCircle(frame, 335, 545, 12, 82, 102, 218);
    fillCircle(frame, 420, 665, 12, 234, 184, 64);
    fillCircle(frame, 205, 610, 12, 35, 34, 36);
    return frame;
}

bool hasRole(const zg::Result &result, zg::LineRole role) {
    for (const auto &line : result.hiddenLines) {
        if (line.role == role) return true;
    }
    for (const auto &line : result.lines) {
        if (line.role == role) return true;
    }
    return false;
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

const zg::Line *firstRole(const zg::Result &result, zg::LineRole role) {
    for (const auto &line : result.lines) {
        if (line.role == role) return &line;
    }
    return nullptr;
}

const char *roleName(zg::LineRole role) {
    switch (role) {
        case zg::LineRole::CueGuide: return "CueGuide";
        case zg::LineRole::ObjectPath: return "ObjectPath";
        case zg::LineRole::GhostContact: return "GhostContact";
        case zg::LineRole::CaromPath: return "CaromPath";
        case zg::LineRole::BankPath: return "BankPath";
        case zg::LineRole::BouncePath: return "BouncePath";
        case zg::LineRole::CollisionWarning: return "CollisionWarning";
        case zg::LineRole::CenterLine: return "CenterLine";
        case zg::LineRole::Unknown:
        default: return "Unknown";
    }
}

void dumpResult(const char *label, const zg::FrameScanResult &scan, const zg::Result &result) {
    std::cerr << label
              << " confidence=" << scan.confidence
              << " balls=" << scan.state.balls.size()
              << " guide=" << scan.state.guide.valid
              << " guideEnd=(" << scan.state.guide.end.x << "," << scan.state.guide.end.y << ")"
              << " visible=" << result.lines.size()
              << " hidden=" << result.hiddenLines.size()
              << "\n";
    for (const auto &ball : scan.state.balls) {
        std::cerr << "  ball cue=" << ball.cue
                  << " center=(" << ball.center.x << "," << ball.center.y << ")"
                  << " r=" << ball.radius << "\n";
    }
    for (const auto &line : result.lines) {
        std::cerr << "  line " << roleName(line.role)
                  << " (" << line.start.x << "," << line.start.y << ")"
                  << " -> (" << line.end.x << "," << line.end.y << ")\n";
    }
    for (const auto &line : result.hiddenLines) {
        std::cerr << "  hidden " << roleName(line.role)
                  << " (" << line.start.x << "," << line.start.y << ")"
                  << " -> (" << line.end.x << "," << line.end.y << ")\n";
    }
}

} // namespace

int main() {
    zg::FrameScanOptions scanOptions;
    scanOptions.pixelFormat = zg::PixelFormat::RGBA8888;
    scanOptions.maxBalls = 16;
    scanOptions.sampleStep = 3;

    const auto frameA = makeFrame({420, 455});
    const auto frameB = makeFrame({190, 390});
    const auto frameLowContrast = makeFrame({420, 455}, 181, 188, 194);

    const auto scanA = zg::FrameScanner::scan(frameA.data(), kWidth, kHeight, kWidth * kChannels, scanOptions);
    const auto scanB = zg::FrameScanner::scan(frameB.data(), kWidth, kHeight, kWidth * kChannels, scanOptions);
    const auto scanLowContrast = zg::FrameScanner::scan(frameLowContrast.data(), kWidth, kHeight, kWidth * kChannels, scanOptions);

    if (!scanA.valid || !scanB.valid) {
        std::cerr << "scanner did not validate frames\n";
        return EXIT_FAILURE;
    }
    if (!scanA.state.guide.valid || !scanB.state.guide.valid) {
        std::cerr << "scanner did not detect both guide lines\n";
        return EXIT_FAILURE;
    }
    if (!scanLowContrast.valid || !scanLowContrast.state.guide.valid) {
        std::cerr << "scanner did not detect lower-contrast textured guide line\n";
        return EXIT_FAILURE;
    }
    if (dist(scanA.state.guide.end, scanB.state.guide.end) < 120.0) {
        std::cerr << "guide line endpoints did not move enough\n";
        return EXIT_FAILURE;
    }

    zg::Settings settings;
    settings.predictionEnabled = true;
    settings.predictionStyle = zg::PredictionStyle::ProVideo;
    settings.scanRoute = zg::ScanRoute::AutoHybrid;
    settings.shotMode = zg::ShotMode::Auto;
    settings.fourLinePredictionEnabled = true;
    settings.hiddenLineRecordingEnabled = true;
    settings.maxBounces = 4;

    zg::PredictionEngine engine;
    const auto resultA = engine.compute(scanA.state, settings);
    const auto resultB = engine.compute(scanB.state, settings);

    if (!resultA.valid || !resultB.valid) {
        std::cerr << "engine did not produce valid results\n";
        return EXIT_FAILURE;
    }
    if (!allFinite(resultA) || !allFinite(resultB)) {
        dumpResult("A", scanA, resultA);
        dumpResult("B", scanB, resultB);
        std::cerr << "engine produced non-finite scanner-fed geometry\n";
        return EXIT_FAILURE;
    }
    if (!hasRole(resultA, zg::LineRole::CueGuide) ||
        !hasRole(resultA, zg::LineRole::ObjectPath) ||
        !hasRole(resultA, zg::LineRole::CaromPath) ||
        !hasRole(resultA, zg::LineRole::BouncePath)) {
        dumpResult("A", scanA, resultA);
        std::cerr << "result A is missing required line roles\n";
        return EXIT_FAILURE;
    }
    if (resultA.hiddenLines.size() < 6 || resultB.hiddenLines.size() < 6) {
        dumpResult("A", scanA, resultA);
        dumpResult("B", scanB, resultB);
        std::cerr << "hidden line recording is too small\n";
        return EXIT_FAILURE;
    }

    const zg::Line *cueA = firstRole(resultA, zg::LineRole::CueGuide);
    const zg::Line *cueB = firstRole(resultB, zg::LineRole::CueGuide);
    if (!cueA || !cueB || dist(cueA->end, cueB->end) < 45.0) {
        std::cerr << "cue prediction did not change with guide aim\n";
        return EXIT_FAILURE;
    }

    std::cout << "scanA=" << scanA.confidence
              << " scanB=" << scanB.confidence
              << " guideDelta=" << dist(scanA.state.guide.end, scanB.state.guide.end)
              << " visibleA=" << resultA.lines.size()
              << " hiddenA=" << resultA.hiddenLines.size()
              << "\n";
    return EXIT_SUCCESS;
}
