#import <UIKit/UIKit.h>
#import "ZGPookingOverlayExports.h"

void ZGStartOverlayExample(UIWindow *window) {
    [ZGPookingOverlayController startInWindow:window];
}

void ZGEnableOverlayExample(void) {
    ZGPookingOverlaySettings *settings = [ZGPookingOverlayController settings];
    settings.predictionEnabled = YES;
    settings.cuePredictionEnabled = YES;
    settings.pocketPredictionEnabled = YES;
    settings.bankPredictionEnabled = YES;
    settings.caromPredictionEnabled = YES;
    settings.ladderGuideEnabled = YES;
    settings.collisionWarningEnabled = YES;
    settings.pocketHeatEnabled = YES;
    settings.fourLinePredictionEnabled = YES;
    settings.hiddenLineRecordingEnabled = YES;
    settings.scanSmoothingEnabled = YES;
    settings.shotMode = ZGPookingShotModeAuto;
    settings.predictionStyle = ZGPookingStyleProVideo;
    settings.scanRoute = ZGScanRouteAutoHybrid;
    settings.maxBounces = 4;
    [ZGPookingOverlayController applySettings:settings];
}

void ZGUpdateOverlayExample(CGRect viewBounds) {
    ZGOverlayRect table = {
        viewBounds.size.width * 0.10,
        viewBounds.size.height * 0.24,
        viewBounds.size.width * 0.80,
        viewBounds.size.height * 0.45
    };

    ZGOverlayPoint cue = {table.x + table.width * 0.25, table.y + table.height * 0.52};
    ZGOverlayBall balls[] = {
        {{table.x + table.width * 0.62, table.y + table.height * 0.42}, 12, 3, NO},
        {{table.x + table.width * 0.68, table.y + table.height * 0.48}, 12, 8, NO},
        {cue, 12, 0, YES}
    };
    ZGOverlayGuideLine guide = {YES, cue, {table.x + table.width * 0.86, table.y + table.height * 0.38}};

    [ZGPookingOverlayController updateTable:table
                                       cueBall:cue
                                    hasCueBall:YES
                                         balls:balls
                                         count:3
                                         guide:guide];
}

BOOL ZGUpdateOverlayFromFrameExample(const uint8_t *bytes,
                                     NSUInteger width,
                                     NSUInteger height,
                                     NSUInteger bytesPerRow) {
    return ZGPookingOverlayUpdateFromFrameBytes(bytes,
                                               width,
                                               height,
                                               bytesPerRow,
                                               ZGPookingPixelFormatBGRA8888);
}

NSUInteger ZGHiddenLineCountExample(void) {
    return ZGPookingOverlayHiddenLineCount();
}

NSUInteger ZGCopyHiddenLinesExample(ZGOverlayLine *lines, NSUInteger maxCount) {
    return ZGPookingOverlayCopyHiddenLines(lines, maxCount);
}
