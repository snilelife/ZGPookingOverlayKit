#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ZGPookingOverlayController.h"

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default")))
void ZGPookingOverlayStartInWindow(UIWindow *window);

__attribute__((visibility("default")))
void ZGPookingOverlayStartInView(UIView *view);

__attribute__((visibility("default")))
void ZGPookingOverlayStop(void);

__attribute__((visibility("default")))
void ZGPookingOverlaySetVisible(BOOL visible);

__attribute__((visibility("default")))
NSUInteger ZGPookingOverlayHiddenLineCount(void);

__attribute__((visibility("default")))
NSUInteger ZGPookingOverlayCopyHiddenLines(ZGOverlayLine *_Nullable outLines, NSUInteger maxCount);

__attribute__((visibility("default")))
ZGPookingOverlaySettings *ZGPookingOverlayCopySettings(void);

__attribute__((visibility("default")))
void ZGPookingOverlayApplySettings(ZGPookingOverlaySettings *settings);

__attribute__((visibility("default")))
void ZGPookingOverlayUpdateTable(ZGOverlayRect table,
                                    ZGOverlayPoint cueBall,
                                    BOOL hasCueBall,
                                    const ZGOverlayBall *_Nullable balls,
                                    NSUInteger count,
                                    ZGOverlayGuideLine guide);

__attribute__((visibility("default")))
BOOL ZGPookingOverlayUpdateFromFrameBytes(const uint8_t *bytes,
                                          NSUInteger width,
                                          NSUInteger height,
                                          NSUInteger bytesPerRow,
                                          ZGPookingPixelFormat pixelFormat);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
