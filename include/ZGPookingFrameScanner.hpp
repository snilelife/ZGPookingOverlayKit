#pragma once

#include <cstddef>
#include <cstdint>

#include "ZGOverlayTypes.hpp"

namespace zg {

enum class PixelFormat : std::uint8_t {
    RGBA8888 = 0,
    BGRA8888 = 1
};

struct FrameScanOptions {
    PixelFormat pixelFormat = PixelFormat::RGBA8888;
    int maxBalls = 16;
    int sampleStep = 3;
};

struct FrameScanResult {
    bool valid = false;
    double confidence = 0.0;
    GameState state;
};

struct FrameStabilizerOptions {
    bool enabled = true;
    double baseSmoothing = 0.32;
    double snapDistanceRatio = 0.105;
    double maxBallMatchDistanceRatio = 0.070;
};

class FrameScanner {
public:
    static FrameScanResult scan(const std::uint8_t *bytes,
                                std::size_t width,
                                std::size_t height,
                                std::size_t bytesPerRow,
                                const FrameScanOptions &options);
};

class FrameStabilizer {
public:
    void reset();
    GameState update(const GameState &raw, double confidence, const FrameStabilizerOptions &options = {});
    bool hasState() const { return hasState_; }
    const GameState &lastState() const { return state_; }

private:
    bool hasState_ = false;
    GameState state_;
};

} // namespace zg
