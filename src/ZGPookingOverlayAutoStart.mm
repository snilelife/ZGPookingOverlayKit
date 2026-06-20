#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ZGPookingOverlayExports.h"

static BOOL ZGPookingOverlayAutoStartEnabled = YES;
static BOOL ZGPookingOverlayAutoStartInstalled = NO;

static BOOL ZGPookingWindowLooksUsable(UIWindow *window) {
    if (!window) return NO;
    if (window.hidden) return NO;
    if (window.alpha <= 0.01) return NO;
    if (CGRectGetWidth(window.bounds) < 20.0 || CGRectGetHeight(window.bounds) < 20.0) return NO;
    return YES;
}

static UIWindow *ZGPookingFindBestWindow(void) {
    UIApplication *app = UIApplication.sharedApplication;

    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in app.connectedScenes) {
            if (![scene isKindOfClass:UIWindowScene.class]) continue;
            UIWindowScene *windowScene = (UIWindowScene *)scene;
            if (windowScene.activationState != UISceneActivationStateForegroundActive &&
                windowScene.activationState != UISceneActivationStateForegroundInactive) {
                continue;
            }
            for (UIWindow *window in windowScene.windows) {
                if (window.isKeyWindow && ZGPookingWindowLooksUsable(window)) return window;
            }
            for (UIWindow *window in windowScene.windows) {
                if (ZGPookingWindowLooksUsable(window)) return window;
            }
        }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    UIWindow *keyWindow = app.keyWindow;
    if (ZGPookingWindowLooksUsable(keyWindow)) return keyWindow;

    for (UIWindow *window in app.windows) {
        if (window.isKeyWindow && ZGPookingWindowLooksUsable(window)) return window;
    }
    for (UIWindow *window in app.windows) {
        if (ZGPookingWindowLooksUsable(window)) return window;
    }
#pragma clang diagnostic pop

    return nil;
}

static void ZGPookingOverlayAutoAttachOnMain(void) {
    if (!ZGPookingOverlayAutoStartEnabled) return;
    UIWindow *window = ZGPookingFindBestWindow();
    if (!window) return;
    ZGPookingOverlayStartInWindow(window);
    ZGPookingOverlaySetVisible(YES);
}

static void ZGPookingOverlayAutoAttachAfterDelay(NSTimeInterval delay) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * (double)NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        ZGPookingOverlayAutoAttachOnMain();
    });
}

static void ZGPookingOverlayInstallAutoStartObservers(void) {
    if (ZGPookingOverlayAutoStartInstalled) return;
    ZGPookingOverlayAutoStartInstalled = YES;
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    void (^attachBlock)(NSNotification *) = ^(__unused NSNotification *note) {
        ZGPookingOverlayAutoAttachAfterDelay(0.05);
        ZGPookingOverlayAutoAttachAfterDelay(0.35);
        ZGPookingOverlayAutoAttachAfterDelay(1.20);
    };
    [center addObserverForName:UIApplicationDidFinishLaunchingNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:attachBlock];
    [center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:attachBlock];
    [center addObserverForName:UIApplicationWillEnterForegroundNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:attachBlock];
    [center addObserverForName:UIWindowDidBecomeKeyNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:attachBlock];
    if (@available(iOS 13.0, *)) {
        [center addObserverForName:UISceneDidActivateNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:attachBlock];
    }
    ZGPookingOverlayAutoAttachAfterDelay(0.10);
    ZGPookingOverlayAutoAttachAfterDelay(0.75);
    ZGPookingOverlayAutoAttachAfterDelay(2.00);
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayAutoAttachNow(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        ZGPookingOverlayAutoAttachOnMain();
    });
}

extern "C" __attribute__((visibility("default")))
void ZGPookingOverlayAutoStartSetEnabled(BOOL enabled) {
    ZGPookingOverlayAutoStartEnabled = enabled;
    if (enabled) ZGPookingOverlayAutoAttachNow();
}

__attribute__((constructor))
static void ZGPookingOverlayAutoStartConstructor(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        ZGPookingOverlayInstallAutoStartObservers();
    });
}
