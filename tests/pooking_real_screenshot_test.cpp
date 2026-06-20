#include "ZGPookingEngine.hpp"
#include "ZGPookingFrameScanner.hpp"

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct BmpImage {
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<std::uint8_t> rgba;
};

std::uint16_t u16(const std::vector<std::uint8_t> &data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(data[offset + 1] << 8);
}

std::uint32_t u32(const std::vector<std::uint8_t> &data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::int32_t i32(const std::vector<std::uint8_t> &data, std::size_t offset) {
    return static_cast<std::int32_t>(u32(data, offset));
}

bool loadBmp(const std::string &path, BmpImage &image) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() < 54 || data[0] != 'B' || data[1] != 'M') return false;

    const std::uint32_t pixelOffset = u32(data, 10);
    const std::int32_t width = i32(data, 18);
    const std::int32_t heightSigned = i32(data, 22);
    const std::uint16_t bpp = u16(data, 28);
    if (width <= 0 || heightSigned == 0 || bpp != 24) return false;

    const bool topDown = heightSigned < 0;
    const std::size_t height = static_cast<std::size_t>(heightSigned < 0 ? -heightSigned : heightSigned);
    const std::size_t rowBytes = ((static_cast<std::size_t>(width) * bpp + 31) / 32) * 4;
    image.width = static_cast<std::size_t>(width);
    image.height = height;
    image.rgba.assign(image.width * image.height * 4, 255);

    for (std::size_t y = 0; y < image.height; ++y) {
        const std::size_t srcY = topDown ? y : image.height - 1 - y;
        const std::size_t row = pixelOffset + srcY * rowBytes;
        if (row + image.width * 3 > data.size()) return false;
        for (std::size_t x = 0; x < image.width; ++x) {
            const std::size_t src = row + x * 3;
            const std::size_t dst = (y * image.width + x) * 4;
            image.rgba[dst + 0] = data[src + 2];
            image.rgba[dst + 1] = data[src + 1];
            image.rgba[dst + 2] = data[src + 0];
            image.rgba[dst + 3] = 255;
        }
    }
    return true;
}

bool finiteResult(const zg::Result &result) {
    for (const auto &line : result.lines) {
        if (!std::isfinite(line.start.x) || !std::isfinite(line.start.y) ||
            !std::isfinite(line.end.x) || !std::isfinite(line.end.y)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    const std::string base = "tests/fixtures/pooking_screens_bmp/";
    const char *positive[] = {
        "1-Photo-1.bmp",
        "2-Photo-2.bmp",
        "3-Photo-3.bmp",
        "4-Photo-4.bmp"
    };

    zg::FrameScanOptions scanOptions;
    scanOptions.pixelFormat = zg::PixelFormat::RGBA8888;
    scanOptions.sampleStep = 4;
    scanOptions.maxBalls = 16;

    zg::Settings settings;
    settings.predictionEnabled = true;
    settings.predictionStyle = zg::PredictionStyle::ProVideo;
    settings.fourLinePredictionEnabled = true;
    settings.hiddenLineRecordingEnabled = true;
    settings.bankPredictionEnabled = true;
    settings.caromPredictionEnabled = true;
    settings.pocketPredictionEnabled = true;
    settings.cuePredictionEnabled = true;

    zg::PredictionEngine engine;
    for (const char *name : positive) {
        BmpImage image;
        if (!loadBmp(base + name, image)) {
            std::cerr << "could not load fixture " << name << "\n";
            return EXIT_FAILURE;
        }
        const auto scan = zg::FrameScanner::scan(image.rgba.data(),
                                                 image.width,
                                                 image.height,
                                                 image.width * 4,
                                                 scanOptions);
        if (!scan.valid || !scan.state.hasCueBall || scan.state.balls.empty()) {
            std::cerr << "scanner failed fixture " << name
                      << " valid=" << scan.valid
                      << " cue=" << scan.state.hasCueBall
                      << " balls=" << scan.state.balls.size()
                      << " confidence=" << scan.confidence << "\n";
            return EXIT_FAILURE;
        }

        const auto result = engine.compute(scan.state, settings);
        if (!result.valid || result.lines.empty() || !finiteResult(result)) {
            std::cerr << "prediction failed fixture " << name
                      << " valid=" << result.valid
                      << " visible=" << result.lines.size() << "\n";
            return EXIT_FAILURE;
        }

        std::cout << name
                  << " confidence=" << scan.confidence
                  << " balls=" << scan.state.balls.size()
                  << " guide=" << scan.state.guide.valid
                  << " visible=" << result.lines.size()
                  << "\n";
    }

    BmpImage lobby;
    if (!loadBmp(base + "5-Photo-5.bmp", lobby)) {
        std::cerr << "could not load lobby fixture\n";
        return EXIT_FAILURE;
    }
    const auto lobbyScan = zg::FrameScanner::scan(lobby.rgba.data(),
                                                 lobby.width,
                                                 lobby.height,
                                                 lobby.width * 4,
                                                 scanOptions);
    if (lobbyScan.valid) {
        std::cerr << "lobby screen was incorrectly detected as a table\n";
        return EXIT_FAILURE;
    }
    std::cout << "lobby rejected\n";
    return EXIT_SUCCESS;
}
