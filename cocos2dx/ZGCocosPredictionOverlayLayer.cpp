#include "ZGCocosPredictionOverlayLayer.hpp"

#if __has_include("cocos2d.h")

bool ZGCocosPredictionOverlayLayer::init() {
    if (!cocos2d::Layer::init()) {
        return false;
    }
    drawNode_ = cocos2d::DrawNode::create();
    addChild(drawNode_);
    refresh();
    return true;
}

void ZGCocosPredictionOverlayLayer::setSettings(const zg::Settings &settings) {
    settings_ = settings;
    refresh();
}

void ZGCocosPredictionOverlayLayer::setGameState(const zg::GameState &state) {
    state_ = state;
    refresh();
}

void ZGCocosPredictionOverlayLayer::refresh() {
    if (!drawNode_) {
        return;
    }
    drawNode_->clear();

    const zg::Result result = engine_.compute(state_, settings_);
    if (!result.valid) {
        return;
    }

    for (const auto &line : result.lines) {
        drawNode_->drawSegment(
            cocos2d::Vec2(line.start.x, line.start.y),
            cocos2d::Vec2(line.end.x, line.end.y),
            line.width * 0.5f,
            cocos2d::Color4F(line.color.r, line.color.g, line.color.b, line.color.a)
        );
    }

    for (const auto &circle : result.circles) {
        drawNode_->drawCircle(
            cocos2d::Vec2(circle.center.x, circle.center.y),
            circle.radius,
            0.0f,
            96,
            false,
            cocos2d::Color4F(circle.color.r, circle.color.g, circle.color.b, circle.color.a)
        );
    }
}

#endif
