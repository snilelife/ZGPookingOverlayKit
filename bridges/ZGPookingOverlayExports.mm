#import <UIKit/UIKit.h>

#import "ZGPookingOverlayExports.h"

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayStartInWindow(UIWindow *window) {
    [ZGPookingOverlayController startInWindow:window];
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayStartInView(UIView *view) {
    [ZGPookingOverlayController startInView:view];
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayStop(void) {
    [ZGPookingOverlayController stop];
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlaySetVisible(BOOL visible) {
    [ZGPookingOverlayController setVisible:visible];
}

extern "C" __attribute__((visibility("default")))
NSUInteger ZGPookingOverlayHiddenLineCount(void) {
    return [ZGPookingOverlayController hiddenLineCount];
}

extern "C" __attribute__((visibility("default")))
NSUInteger ZGPookingOverlayCopyHiddenLines(ZGOverlayLine *outLines, NSUInteger maxCount) {
    return [ZGPookingOverlayController copyHiddenLines:outLines maxCount:maxCount];
}

extern "C" __attribute__((visibility("default")))
ZGPookingOverlaySettings *ZGPookingOverlayCopySettings(void) {
    return [ZGPookingOverlayController settings];
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayApplySettings(ZGPookingOverlaySettings *settings) {
    [ZGPookingOverlayController applySettings:settings];
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayUpdateTable(ZGOverlayRect table,
                                    ZGOverlayPoint cueBall,
                                    BOOL hasCueBall,
                                    const ZGOverlayBall *balls,
                                    NSUInteger count,
                                    ZGOverlayGuideLine guide) {
    [ZGPookingOverlayController updateTable:table
                                       cueBall:cueBall
                                    hasCueBall:hasCueBall
                                         balls:balls
                                         count:count
                                         guide:guide];
}

extern "C" __attribute__((visibility("default")))
BOOL ZGPookingOverlayUpdateFromFrameBytes(const uint8_t *bytes,
                                          NSUInteger width,
                                          NSUInteger height,
                                          NSUInteger bytesPerRow,
                                          ZGPookingPixelFormat pixelFormat) {
    return [ZGPookingOverlayController updateFromFrameBytes:bytes
                                                     width:width
                                                    height:height
                                               bytesPerRow:bytesPerRow
                                                pixelFormat:pixelFormat];
}
