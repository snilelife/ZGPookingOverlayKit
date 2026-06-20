#pragma once

// Optional source-integration adapter.
// This file is not included in the framework build by default because different
// Cocos2d-x projects use different include paths and renderer versions.

#include "ZGPookingEngine.hpp"

#if __has_include("cocos2d.h")
#include "cocos2d.h"

class ZGCocosPredictionOverlayLayer final : public cocos2d::Layer {
public:
    CREATE_FUNC(ZGCocosPredictionOverlayLayer);

    bool init() override;
    void setSettings(const zg::Settings &settings);
    void setGameState(const zg::GameState &state);
    void refresh();

private:
    zg::Settings settings_;
    zg::GameState state_;
    zg::PredictionEngine engine_;
    cocos2d::DrawNode *drawNode_ = nullptr;
};

#endif
