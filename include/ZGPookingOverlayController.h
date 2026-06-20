#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ZGScanRoute) {
    ZGScanRouteAutoHybrid = 0,
    ZGScanRouteGuideLock = 1,
    ZGScanRouteBallGeometry = 2,
    ZGScanRouteCornerLock = 3
};

typedef NS_ENUM(NSInteger, ZGPookingStyle) {
    ZGPookingStyleSimple = 0,
    ZGPookingStyleAdvanced = 1,
    ZGPookingStyleProVideo = 2
};

typedef NS_ENUM(NSInteger, ZGPookingShotMode) {
    ZGPookingShotModeAuto = 0,
    ZGPookingShotModeLongShot = 1,
    ZGPookingShotModeCaromShot = 2,
    ZGPookingShotModeBankShot = 3
};

typedef NS_ENUM(NSInteger, ZGPookingPixelFormat) {
    ZGPookingPixelFormatRGBA8888 = 0,
    ZGPookingPixelFormatBGRA8888 = 1
};

typedef NS_ENUM(NSInteger, ZGPookingLineRole) {
    ZGPookingLineRoleUnknown = 0,
    ZGPookingLineRoleCueGuide = 1,
    ZGPookingLineRoleObjectPath = 2,
    ZGPookingLineRoleGhostContact = 3,
    ZGPookingLineRoleCaromPath = 4,
    ZGPookingLineRoleBankPath = 5,
    ZGPookingLineRoleBouncePath = 6,
    ZGPookingLineRoleCollisionWarning = 7,
    ZGPookingLineRoleCenterLine = 8
};

typedef struct {
    CGFloat x;
    CGFloat y;
} ZGOverlayPoint;

typedef struct {
    CGFloat x;
    CGFloat y;
    CGFloat width;
    CGFloat height;
} ZGOverlayRect;

typedef struct {
    ZGOverlayPoint center;
    CGFloat radius;
    NSInteger number;
    BOOL cue;
} ZGOverlayBall;

typedef struct {
    BOOL valid;
    ZGOverlayPoint start;
    ZGOverlayPoint end;
} ZGOverlayGuideLine;

typedef struct {
    ZGOverlayPoint start;
    ZGOverlayPoint end;
    CGFloat red;
    CGFloat green;
    CGFloat blue;
    CGFloat alpha;
    CGFloat width;
    ZGPookingLineRole role;
} ZGOverlayLine;

@interface ZGPookingOverlaySettings : NSObject <NSCopying>
@property (nonatomic) BOOL predictionEnabled;
@property (nonatomic) BOOL cuePredictionEnabled;
@property (nonatomic) BOOL pocketPredictionEnabled;
@property (nonatomic) BOOL bankPredictionEnabled;
@property (nonatomic) BOOL caromPredictionEnabled;
@property (nonatomic) BOOL ladderGuideEnabled;
@property (nonatomic) BOOL collisionWarningEnabled;
@property (nonatomic) BOOL pocketHeatEnabled;
@property (nonatomic) BOOL fourLinePredictionEnabled;
@property (nonatomic) BOOL hiddenLineRecordingEnabled;
@property (nonatomic) BOOL scanSmoothingEnabled;
@property (nonatomic) BOOL manualPocket;
@property (nonatomic) BOOL showSideLines;
@property (nonatomic) BOOL showDetectedBalls;
@property (nonatomic) BOOL showGhostBall;
@property (nonatomic) CGFloat lineLength;
@property (nonatomic) NSInteger maxBounces;
@property (nonatomic) NSInteger selectedPocket;
@property (nonatomic) ZGScanRoute scanRoute;
@property (nonatomic) ZGPookingShotMode shotMode;
@property (nonatomic) ZGPookingStyle predictionStyle;
+ (instancetype)defaults;
@end

@interface ZGPookingOverlayController : NSObject

+ (void)startInWindow:(UIWindow *)window;
+ (void)startInView:(UIView *)view;
+ (void)stop;
+ (void)setVisible:(BOOL)visible;
+ (NSUInteger)hiddenLineCount;
+ (NSUInteger)copyHiddenLines:(ZGOverlayLine *_Nullable)outLines maxCount:(NSUInteger)maxCount;
+ (ZGPookingOverlaySettings *)settings;
+ (void)applySettings:(ZGPookingOverlaySettings *)settings;

+ (void)updateTable:(ZGOverlayRect)table
            cueBall:(ZGOverlayPoint)cueBall
        hasCueBall:(BOOL)hasCueBall
             balls:(const ZGOverlayBall *_Nullable)balls
             count:(NSUInteger)count
             guide:(ZGOverlayGuideLine)guide;

+ (BOOL)updateFromFrameBytes:(const uint8_t *)bytes
                       width:(NSUInteger)width
                      height:(NSUInteger)height
                 bytesPerRow:(NSUInteger)bytesPerRow
                  pixelFormat:(ZGPookingPixelFormat)pixelFormat;

@end

NS_ASSUME_NONNULL_END
