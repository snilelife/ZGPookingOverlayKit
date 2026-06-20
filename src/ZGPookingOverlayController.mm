#import "ZGPookingOverlayController.h"

#include "ZGPookingEngine.hpp"
#include "ZGPookingFrameScanner.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#import <QuartzCore/QuartzCore.h>

static NSString *ZGOnOff(BOOL value) {
    return value ? @"ON" : @"OFF";
}

static NSString *ZGRouteName(ZGScanRoute route) {
    switch (route) {
        case ZGScanRouteGuideLock: return @"GUIDE";
        case ZGScanRouteBallGeometry: return @"BALL";
        case ZGScanRouteCornerLock: return @"CORNER";
        case ZGScanRouteAutoHybrid:
        default: return @"AUTO";
    }
}

static NSString *ZGStyleName(ZGPookingStyle style) {
    switch (style) {
        case ZGPookingStyleSimple: return @"SIMPLE";
        case ZGPookingStyleProVideo: return @"VIDEO";
        case ZGPookingStyleAdvanced:
        default: return @"ADVANCED";
    }
}

static NSString *ZGShotModeName(ZGPookingShotMode mode) {
    switch (mode) {
        case ZGPookingShotModeLongShot: return @"LONG";
        case ZGPookingShotModeCaromShot: return @"CAROM";
        case ZGPookingShotModeBankShot: return @"BANK";
        case ZGPookingShotModeAuto:
        default: return @"AUTO";
    }
}

static NSString *ZGPocketName(NSInteger pocket, BOOL manual) {
    if (!manual) return @"AUTO";
    NSArray<NSString *> *names = @[@"TOP L", @"TOP M", @"TOP R", @"BOT L", @"BOT M", @"BOT R"];
    NSInteger index = MAX(0, MIN((NSInteger)names.count - 1, pocket));
    return names[index];
}

static ZGPookingLineRole ZGExportRole(zg::LineRole role) {
    switch (role) {
        case zg::LineRole::CueGuide: return ZGPookingLineRoleCueGuide;
        case zg::LineRole::ObjectPath: return ZGPookingLineRoleObjectPath;
        case zg::LineRole::GhostContact: return ZGPookingLineRoleGhostContact;
        case zg::LineRole::CaromPath: return ZGPookingLineRoleCaromPath;
        case zg::LineRole::BankPath: return ZGPookingLineRoleBankPath;
        case zg::LineRole::BouncePath: return ZGPookingLineRoleBouncePath;
        case zg::LineRole::CollisionWarning: return ZGPookingLineRoleCollisionWarning;
        case zg::LineRole::CenterLine: return ZGPookingLineRoleCenterLine;
        case zg::LineRole::Unknown:
        default: return ZGPookingLineRoleUnknown;
    }
}

static ZGOverlayLine ZGExportLine(const zg::Line &line) {
    ZGOverlayLine out;
    out.start = {line.start.x, line.start.y};
    out.end = {line.end.x, line.end.y};
    out.red = line.color.r;
    out.green = line.color.g;
    out.blue = line.color.b;
    out.alpha = line.color.a;
    out.width = line.width;
    out.role = ZGExportRole(line.role);
    return out;
}

static void ZGScaleGameState(zg::GameState &state, CGFloat scale) {
    if (scale <= 0.0 || std::fabs(scale - 1.0) < 0.001) return;
    state.table.x *= scale;
    state.table.y *= scale;
    state.table.width *= scale;
    state.table.height *= scale;
    state.cueBall.x *= scale;
    state.cueBall.y *= scale;
    state.guide.start.x *= scale;
    state.guide.start.y *= scale;
    state.guide.end.x *= scale;
    state.guide.end.y *= scale;
    for (auto &ball : state.balls) {
        ball.center.x *= scale;
        ball.center.y *= scale;
        ball.radius *= scale;
    }
}

@interface ZGPookingOverlaySettings ()
- (zg::Settings)zg_settings;
@end

@implementation ZGPookingOverlaySettings

+ (instancetype)defaults {
    ZGPookingOverlaySettings *settings = [[ZGPookingOverlaySettings alloc] init];
    settings.predictionEnabled = NO;
    settings.cuePredictionEnabled = NO;
    settings.pocketPredictionEnabled = NO;
    settings.bankPredictionEnabled = NO;
    settings.caromPredictionEnabled = NO;
    settings.ladderGuideEnabled = NO;
    settings.collisionWarningEnabled = NO;
    settings.pocketHeatEnabled = NO;
    settings.fourLinePredictionEnabled = NO;
    settings.hiddenLineRecordingEnabled = NO;
    settings.scanSmoothingEnabled = YES;
    settings.liveScanEnabled = NO;
    settings.manualPocket = NO;
    settings.showSideLines = NO;
    settings.showDetectedBalls = NO;
    settings.showGhostBall = NO;
    settings.lineLength = 1.10;
    settings.maxBounces = 4;
    settings.selectedPocket = 1;
    settings.scanRoute = ZGScanRouteAutoHybrid;
    settings.shotMode = ZGPookingShotModeAuto;
    settings.predictionStyle = ZGPookingStyleAdvanced;
    return settings;
}

- (id)copyWithZone:(NSZone *)zone {
    ZGPookingOverlaySettings *copy = [[[self class] allocWithZone:zone] init];
    copy.predictionEnabled = self.predictionEnabled;
    copy.cuePredictionEnabled = self.cuePredictionEnabled;
    copy.pocketPredictionEnabled = self.pocketPredictionEnabled;
    copy.bankPredictionEnabled = self.bankPredictionEnabled;
    copy.caromPredictionEnabled = self.caromPredictionEnabled;
    copy.ladderGuideEnabled = self.ladderGuideEnabled;
    copy.collisionWarningEnabled = self.collisionWarningEnabled;
    copy.pocketHeatEnabled = self.pocketHeatEnabled;
    copy.fourLinePredictionEnabled = self.fourLinePredictionEnabled;
    copy.hiddenLineRecordingEnabled = self.hiddenLineRecordingEnabled;
    copy.scanSmoothingEnabled = self.scanSmoothingEnabled;
    copy.liveScanEnabled = self.liveScanEnabled;
    copy.manualPocket = self.manualPocket;
    copy.showSideLines = self.showSideLines;
    copy.showDetectedBalls = self.showDetectedBalls;
    copy.showGhostBall = self.showGhostBall;
    copy.lineLength = self.lineLength;
    copy.maxBounces = self.maxBounces;
    copy.selectedPocket = self.selectedPocket;
    copy.scanRoute = self.scanRoute;
    copy.shotMode = self.shotMode;
    copy.predictionStyle = self.predictionStyle;
    return copy;
}

- (zg::Settings)zg_settings {
    zg::Settings out;
    out.predictionEnabled = self.predictionEnabled;
    out.cuePredictionEnabled = self.cuePredictionEnabled;
    out.pocketPredictionEnabled = self.pocketPredictionEnabled;
    out.bankPredictionEnabled = self.bankPredictionEnabled;
    out.caromPredictionEnabled = self.caromPredictionEnabled;
    out.ladderGuideEnabled = self.ladderGuideEnabled;
    out.collisionWarningEnabled = self.collisionWarningEnabled;
    out.pocketHeatEnabled = self.pocketHeatEnabled;
    out.fourLinePredictionEnabled = self.fourLinePredictionEnabled;
    out.hiddenLineRecordingEnabled = self.hiddenLineRecordingEnabled;
    out.manualPocket = self.manualPocket;
    out.showSideLines = self.showSideLines;
    out.showDetectedBalls = self.showDetectedBalls;
    out.showGhostBall = self.showGhostBall;
    out.lineLength = self.lineLength;
    out.maxBounces = (int)self.maxBounces;
    out.selectedPocket = (int)self.selectedPocket;
    out.scanRoute = static_cast<zg::ScanRoute>(self.scanRoute);
    out.shotMode = static_cast<zg::ShotMode>(self.shotMode);
    out.predictionStyle = static_cast<zg::PredictionStyle>(self.predictionStyle);
    return out;
}

@end

@interface ZGPookingCanvasView : UIView {
@private
    zg::Result _result;
}
- (void)setPredictionResult:(const zg::Result &)result;
@end

@implementation ZGPookingCanvasView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = UIColor.clearColor;
        self.userInteractionEnabled = NO;
        self.opaque = NO;
    }
    return self;
}

- (void)setPredictionResult:(const zg::Result &)result {
    _result = result;
    [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect {
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    if (!ctx || !_result.valid) return;

    CGContextSetLineCap(ctx, kCGLineCapRound);
    CGContextSetLineJoin(ctx, kCGLineJoinRound);

    for (const auto &line : _result.lines) {
        CGContextSetStrokeColorWithColor(ctx, [UIColor colorWithRed:line.color.r green:line.color.g blue:line.color.b alpha:line.color.a].CGColor);
        CGContextSetLineWidth(ctx, line.width);
        CGContextMoveToPoint(ctx, line.start.x, line.start.y);
        CGContextAddLineToPoint(ctx, line.end.x, line.end.y);
        CGContextStrokePath(ctx);
    }

    for (const auto &circle : _result.circles) {
        CGContextSetStrokeColorWithColor(ctx, [UIColor colorWithRed:circle.color.r green:circle.color.g blue:circle.color.b alpha:circle.color.a].CGColor);
        CGContextSetLineWidth(ctx, circle.width);
        CGRect oval = CGRectMake(circle.center.x - circle.radius, circle.center.y - circle.radius, circle.radius * 2.0, circle.radius * 2.0);
        CGContextStrokeEllipseInRect(ctx, oval);
    }
}

@end

@interface ZGPookingOverlayView : UIView {
@private
    zg::PredictionEngine _engine;
    zg::FrameStabilizer _stabilizer;
    zg::GameState _state;
    zg::Result _lastResult;
}
@property (nonatomic, strong) ZGPookingOverlaySettings *settings;
@property (nonatomic, strong) ZGPookingCanvasView *canvasView;
@property (nonatomic, strong) UIView *bubbleView;
@property (nonatomic, strong) UIButton *quickToggleButton;
@property (nonatomic, strong) UIView *menuView;
@property (nonatomic, strong) UILabel *statusLabel;
@property (nonatomic) CGFloat lastScanConfidence;
@property (nonatomic, strong) CADisplayLink *liveScanLink;
@property (nonatomic) CFTimeInterval lastLiveScanTimestamp;
@property (nonatomic) BOOL liveScanBusy;
@property (nonatomic, strong) UIButton *enabledButton;
@property (nonatomic, strong) UIButton *cueButton;
@property (nonatomic, strong) UIButton *pocketPathButton;
@property (nonatomic, strong) UIButton *bankPathButton;
@property (nonatomic, strong) UIButton *caromButton;
@property (nonatomic, strong) UIButton *ladderButton;
@property (nonatomic, strong) UIButton *collisionButton;
@property (nonatomic, strong) UIButton *pocketHeatButton;
@property (nonatomic, strong) UIButton *fourLineButton;
@property (nonatomic, strong) UIButton *hiddenRecordButton;
@property (nonatomic, strong) UIButton *liveScanButton;
@property (nonatomic, strong) UIButton *scanSmoothButton;
@property (nonatomic, strong) UIButton *shotModeButton;
@property (nonatomic, strong) UIButton *routeButton;
@property (nonatomic, strong) UIButton *styleButton;
@property (nonatomic, strong) UIButton *selectedPocketButton;
@property (nonatomic, strong) UIButton *bounceButton;
@property (nonatomic, strong) UIButton *lengthButton;
@property (nonatomic, strong) UIButton *ghostButton;
@property (nonatomic, strong) UIButton *ballsButton;
@property (nonatomic, strong) UIButton *sideLinesButton;
- (void)recompute;
- (void)refreshLiveScanState;
- (void)stopLiveScan;
@end

@implementation ZGPookingOverlayView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = UIColor.clearColor;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.settings = [ZGPookingOverlaySettings defaults];

        _canvasView = [[ZGPookingCanvasView alloc] initWithFrame:self.bounds];
        _canvasView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        [self addSubview:_canvasView];

        [self buildBubble];
        [self buildMenu];
        [self recompute];
    }
    return self;
}

- (void)dealloc {
    [self stopLiveScan];
}

- (void)didMoveToSuperview {
    [super didMoveToSuperview];
    [self refreshLiveScanState];
}

- (void)buildBubble {
    _bubbleView = [[UIView alloc] initWithFrame:CGRectMake(18, 80, 58, 58)];
    _bubbleView.backgroundColor = [UIColor colorWithRed:0.02 green:0.08 blue:0.12 alpha:0.92];
    _bubbleView.layer.cornerRadius = 29;
    _bubbleView.layer.borderColor = [UIColor colorWithRed:0.15 green:0.88 blue:1 alpha:0.74].CGColor;
    _bubbleView.layer.borderWidth = 1.5;
    _bubbleView.layer.shadowColor = UIColor.cyanColor.CGColor;
    _bubbleView.layer.shadowOpacity = 0.35;
    _bubbleView.layer.shadowRadius = 10;

    UILabel *label = [[UILabel alloc] initWithFrame:_bubbleView.bounds];
    label.text = @"ZG";
    label.textAlignment = NSTextAlignmentCenter;
    label.textColor = UIColor.whiteColor;
    label.font = [UIFont systemFontOfSize:20 weight:UIFontWeightBlack];
    [_bubbleView addSubview:label];

    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(toggleMenu)];
    UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panBubble:)];
    [_bubbleView addGestureRecognizer:tap];
    [_bubbleView addGestureRecognizer:pan];
    [self addSubview:_bubbleView];

    _quickToggleButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _quickToggleButton.frame = CGRectMake(82, 86, 88, 46);
    _quickToggleButton.layer.cornerRadius = 23;
    _quickToggleButton.layer.borderWidth = 1.0;
    _quickToggleButton.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightBlack];
    [_quickToggleButton setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [_quickToggleButton addTarget:self action:@selector(togglePredictions) forControlEvents:UIControlEventTouchUpInside];
    UIPanGestureRecognizer *quickPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panBubble:)];
    [_quickToggleButton addGestureRecognizer:quickPan];
    [self addSubview:_quickToggleButton];
    [self positionQuickToggleForBubble];
}

- (UIButton *)buttonWithTitle:(NSString *)title action:(SEL)action {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightBold];
    button.titleLabel.adjustsFontSizeToFitWidth = YES;
    button.titleLabel.minimumScaleFactor = 0.72;
    button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    button.backgroundColor = [UIColor colorWithWhite:0 alpha:0.14];
    button.layer.cornerRadius = 7;
    button.contentEdgeInsets = UIEdgeInsetsMake(0, 12, 0, 10);
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (UILabel *)menuLabelWithText:(NSString *)text
                          frame:(CGRect)frame
                           size:(CGFloat)size
                         weight:(UIFontWeight)weight
                          color:(UIColor *)color {
    UILabel *label = [[UILabel alloc] initWithFrame:frame];
    label.text = text;
    label.textColor = color;
    label.font = [UIFont systemFontOfSize:size weight:weight];
    label.adjustsFontSizeToFitWidth = YES;
    label.minimumScaleFactor = 0.70;
    return label;
}

- (void)buildMenu {
    const BOOL landscape = self.bounds.size.width >= self.bounds.size.height;
    CGFloat menuWidth = landscape ? MIN(760.0, MAX(520.0, self.bounds.size.width - 150.0))
                                  : MIN(360.0, MAX(306.0, self.bounds.size.width - 32.0));
    CGFloat menuHeight = landscape ? MIN(382.0, MAX(302.0, self.bounds.size.height - 78.0))
                                   : MIN(476.0, MAX(336.0, self.bounds.size.height - 114.0));
    CGFloat menuX = landscape ? (self.bounds.size.width - menuWidth) * 0.50 : 16.0;
    CGFloat menuY = landscape ? (self.bounds.size.height - menuHeight) * 0.50
                              : MIN(146.0, MAX(66.0, self.bounds.size.height - menuHeight - 18.0));
    _menuView = [[UIView alloc] initWithFrame:CGRectMake(menuX, menuY, menuWidth, menuHeight)];
    _menuView.hidden = YES;
    _menuView.backgroundColor = [UIColor colorWithRed:0.004 green:0.008 blue:0.010 alpha:0.76];
    _menuView.layer.cornerRadius = 10;
    _menuView.layer.borderColor = [UIColor colorWithRed:0.05 green:0.72 blue:0.96 alpha:0.45].CGColor;
    _menuView.layer.borderWidth = 1.1;
    _menuView.layer.shadowColor = UIColor.cyanColor.CGColor;
    _menuView.layer.shadowOpacity = 0.20;
    _menuView.layer.shadowRadius = 14;

    UIPanGestureRecognizer *menuPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panMenu:)];
    [_menuView addGestureRecognizer:menuPan];

    const CGFloat sidebarWidth = landscape ? 142.0 : 106.0;
    UIView *sidebar = [[UIView alloc] initWithFrame:CGRectMake(0, 0, sidebarWidth, menuHeight)];
    sidebar.backgroundColor = [UIColor colorWithRed:0.00 green:0.03 blue:0.05 alpha:0.30];
    [_menuView addSubview:sidebar];

    UIView *divider = [[UIView alloc] initWithFrame:CGRectMake(sidebarWidth, 0, 1, menuHeight)];
    divider.backgroundColor = [UIColor colorWithRed:0.08 green:0.75 blue:1 alpha:0.48];
    [_menuView addSubview:divider];

    UIView *logo = [[UIView alloc] initWithFrame:CGRectMake((sidebarWidth - 58.0) * 0.5, 24, 58, 58)];
    logo.backgroundColor = [UIColor colorWithRed:0.01 green:0.10 blue:0.16 alpha:0.96];
    logo.layer.cornerRadius = 29;
    logo.layer.borderColor = [UIColor colorWithRed:0.12 green:0.86 blue:1 alpha:0.70].CGColor;
    logo.layer.borderWidth = 1.2;
    logo.layer.shadowColor = UIColor.cyanColor.CGColor;
    logo.layer.shadowOpacity = 0.22;
    logo.layer.shadowRadius = 8;
    [sidebar addSubview:logo];

    UILabel *logoText = [self menuLabelWithText:@"ZG"
                                          frame:logo.bounds
                                           size:21
                                         weight:UIFontWeightBlack
                                          color:UIColor.whiteColor];
    logoText.textAlignment = NSTextAlignmentCenter;
    [logo addSubview:logoText];

    UILabel *brand = [self menuLabelWithText:@"ZavIOS"
                                       frame:CGRectMake(12, 92, sidebarWidth - 24, 23)
                                        size:15
                                      weight:UIFontWeightBlack
                                       color:UIColor.whiteColor];
    brand.textAlignment = NSTextAlignmentCenter;
    [sidebar addSubview:brand];

    NSArray<NSString *> *nav = @[@"VISUALS", @"AIM MENU", @"SETTINGS"];
    NSArray<NSString *> *navMarks = @[@"EYE", @"AIM", @"GEAR"];
    for (NSUInteger i = 0; i < nav.count; ++i) {
        CGFloat y = 146.0 + (CGFloat)i * 46.0;
        UILabel *mark = [self menuLabelWithText:navMarks[i]
                                          frame:CGRectMake(12, y, 36, 24)
                                           size:10
                                         weight:UIFontWeightBlack
                                          color:[UIColor colorWithRed:0.22 green:0.88 blue:1 alpha:0.98]];
        [sidebar addSubview:mark];
        UILabel *item = [self menuLabelWithText:nav[i]
                                          frame:CGRectMake(48, y, sidebarWidth - 56, 24)
                                           size:12
                                         weight:UIFontWeightBold
                                          color:UIColor.whiteColor];
        [sidebar addSubview:item];
    }

    UILabel *version = [self menuLabelWithText:@"v3.0"
                                         frame:CGRectMake(14, menuHeight - 54, sidebarWidth - 28, 18)
                                          size:11
                                        weight:UIFontWeightBlack
                                         color:UIColor.whiteColor];
    version.textAlignment = NSTextAlignmentCenter;
    [sidebar addSubview:version];

    UILabel *credit = [self menuLabelWithText:@"created by zav G"
                                        frame:CGRectMake(8, menuHeight - 30, sidebarWidth - 16, 16)
                                         size:9
                                       weight:UIFontWeightBold
                                        color:[UIColor colorWithWhite:1 alpha:0.60]];
    credit.textAlignment = NSTextAlignmentCenter;
    [sidebar addSubview:credit];

    const CGFloat contentX = sidebarWidth + 14.0;
    const CGFloat contentWidth = menuWidth - contentX - 12.0;

    NSArray<NSString *> *tabs = @[@"Prediction", @"Bounds", @"Table Info"];
    CGFloat tabX = contentX;
    for (NSUInteger i = 0; i < tabs.count; ++i) {
        CGFloat tabWidth = i == 2 ? 74.0 : 74.0;
        UILabel *tab = [self menuLabelWithText:tabs[i]
                                         frame:CGRectMake(tabX, 10, tabWidth, 23)
                                          size:12
                                        weight:UIFontWeightBlack
                                         color:UIColor.whiteColor];
        tab.textAlignment = NSTextAlignmentCenter;
        tab.backgroundColor = i == 0 ? [UIColor colorWithRed:0.04 green:0.48 blue:0.74 alpha:0.78]
                                     : [UIColor colorWithRed:0.06 green:0.18 blue:0.25 alpha:0.54];
        tab.layer.cornerRadius = 4;
        tab.clipsToBounds = YES;
        [_menuView addSubview:tab];
        tabX += tabWidth + 6.0;
    }

    UIView *tabLine = [[UIView alloc] initWithFrame:CGRectMake(contentX, 36, contentWidth, 1)];
    tabLine.backgroundColor = [UIColor colorWithRed:0.08 green:0.66 blue:1 alpha:0.40];
    [_menuView addSubview:tabLine];

    _statusLabel = [[UILabel alloc] initWithFrame:CGRectMake(contentX, 40, contentWidth, 20)];
    _statusLabel.textColor = [UIColor colorWithRed:0.18 green:0.90 blue:1 alpha:1];
    _statusLabel.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightBold];
    [_menuView addSubview:_statusLabel];

    UIScrollView *scrollView = [[UIScrollView alloc] initWithFrame:CGRectMake(contentX, 66, contentWidth, menuHeight - 78)];
    scrollView.showsVerticalScrollIndicator = NO;
    scrollView.backgroundColor = UIColor.clearColor;
    [_menuView addSubview:scrollView];

    const CGFloat rowHeight = landscape ? 30.0 : 34.0;
    const CGFloat spacing = 5.0;
    const NSInteger rowCount = 21;
    CGFloat stackHeight = rowCount * rowHeight + (rowCount - 1) * spacing;
    UIStackView *stack = [[UIStackView alloc] initWithFrame:CGRectMake(0, 0, scrollView.bounds.size.width, stackHeight)];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = spacing;
    stack.distribution = UIStackViewDistributionFillEqually;
    [scrollView addSubview:stack];
    scrollView.contentSize = CGSizeMake(scrollView.bounds.size.width, stackHeight);

    self.enabledButton = [self buttonWithTitle:@"" action:@selector(togglePredictions)];
    self.cueButton = [self buttonWithTitle:@"" action:@selector(toggleCuePrediction)];
    self.pocketPathButton = [self buttonWithTitle:@"" action:@selector(togglePocketPrediction)];
    self.bankPathButton = [self buttonWithTitle:@"" action:@selector(toggleBankPrediction)];
    self.caromButton = [self buttonWithTitle:@"" action:@selector(toggleCaromPrediction)];
    self.ladderButton = [self buttonWithTitle:@"" action:@selector(toggleLadderGuide)];
    self.collisionButton = [self buttonWithTitle:@"" action:@selector(toggleCollisionWarnings)];
    self.pocketHeatButton = [self buttonWithTitle:@"" action:@selector(togglePocketHeat)];
    self.fourLineButton = [self buttonWithTitle:@"" action:@selector(toggleFourLines)];
    self.hiddenRecordButton = [self buttonWithTitle:@"" action:@selector(toggleHiddenRecording)];
    self.liveScanButton = [self buttonWithTitle:@"" action:@selector(toggleLiveScan)];
    self.scanSmoothButton = [self buttonWithTitle:@"" action:@selector(toggleScanSmoothing)];
    self.shotModeButton = [self buttonWithTitle:@"" action:@selector(cycleShotMode)];
    self.routeButton = [self buttonWithTitle:@"" action:@selector(cycleRoute)];
    self.styleButton = [self buttonWithTitle:@"" action:@selector(cycleStyle)];
    self.selectedPocketButton = [self buttonWithTitle:@"" action:@selector(nextPocket)];
    self.bounceButton = [self buttonWithTitle:@"" action:@selector(nextBounces)];
    self.lengthButton = [self buttonWithTitle:@"" action:@selector(nextLineLength)];
    self.ghostButton = [self buttonWithTitle:@"" action:@selector(toggleGhost)];
    self.ballsButton = [self buttonWithTitle:@"" action:@selector(toggleBalls)];
    self.sideLinesButton = [self buttonWithTitle:@"" action:@selector(toggleSideLines)];

    [stack addArrangedSubview:self.enabledButton];
    [stack addArrangedSubview:self.cueButton];
    [stack addArrangedSubview:self.pocketPathButton];
    [stack addArrangedSubview:self.bankPathButton];
    [stack addArrangedSubview:self.caromButton];
    [stack addArrangedSubview:self.ladderButton];
    [stack addArrangedSubview:self.collisionButton];
    [stack addArrangedSubview:self.pocketHeatButton];
    [stack addArrangedSubview:self.fourLineButton];
    [stack addArrangedSubview:self.hiddenRecordButton];
    [stack addArrangedSubview:self.liveScanButton];
    [stack addArrangedSubview:self.scanSmoothButton];
    [stack addArrangedSubview:self.shotModeButton];
    [stack addArrangedSubview:self.routeButton];
    [stack addArrangedSubview:self.styleButton];
    [stack addArrangedSubview:self.selectedPocketButton];
    [stack addArrangedSubview:self.bounceButton];
    [stack addArrangedSubview:self.lengthButton];
    [stack addArrangedSubview:self.ghostButton];
    [stack addArrangedSubview:self.ballsButton];
    [stack addArrangedSubview:self.sideLinesButton];

    [self addSubview:_menuView];
    [self updateMenuState];
}

- (void)toggleMenu {
    _menuView.hidden = !_menuView.hidden;
}

- (void)panBubble:(UIPanGestureRecognizer *)recognizer {
    CGPoint translation = [recognizer translationInView:self];
    CGPoint center = _bubbleView.center;
    center.x += translation.x;
    center.y += translation.y;
    center.x = MAX(34, MIN(self.bounds.size.width - 34, center.x));
    center.y = MAX(34, MIN(self.bounds.size.height - 34, center.y));
    _bubbleView.center = center;
    [self positionQuickToggleForBubble];
    [recognizer setTranslation:CGPointZero inView:self];
}

- (void)positionQuickToggleForBubble {
    if (!_quickToggleButton || !_bubbleView) return;
    const CGFloat offset = _bubbleView.center.x < self.bounds.size.width - 130.0 ? 91.0 : -91.0;
    CGPoint center = CGPointMake(_bubbleView.center.x + offset, _bubbleView.center.y);
    const CGFloat halfW = _quickToggleButton.bounds.size.width * 0.5;
    const CGFloat halfH = _quickToggleButton.bounds.size.height * 0.5;
    center.x = MAX(halfW + 8, MIN(self.bounds.size.width - halfW - 8, center.x));
    center.y = MAX(halfH + 8, MIN(self.bounds.size.height - halfH - 8, center.y));
    _quickToggleButton.center = center;
}

- (void)panMenu:(UIPanGestureRecognizer *)recognizer {
    CGPoint translation = [recognizer translationInView:self];
    CGPoint center = _menuView.center;
    center.x += translation.x;
    center.y += translation.y;
    const CGFloat halfW = _menuView.bounds.size.width * 0.5;
    const CGFloat halfH = _menuView.bounds.size.height * 0.5;
    center.x = MAX(halfW + 8, MIN(self.bounds.size.width - halfW - 8, center.x));
    center.y = MAX(halfH + 8, MIN(self.bounds.size.height - halfH - 8, center.y));
    _menuView.center = center;
    [recognizer setTranslation:CGPointZero inView:self];
}

- (void)setPredictionFeatureBundleEnabled:(BOOL)enabled {
    self.settings.predictionEnabled = enabled;
    self.settings.liveScanEnabled = enabled;
    self.settings.cuePredictionEnabled = enabled;
    self.settings.pocketPredictionEnabled = enabled;
    self.settings.bankPredictionEnabled = enabled;
    self.settings.caromPredictionEnabled = enabled;
    self.settings.ladderGuideEnabled = enabled;
    self.settings.collisionWarningEnabled = enabled;
    self.settings.pocketHeatEnabled = enabled;
    self.settings.fourLinePredictionEnabled = enabled;
    self.settings.hiddenLineRecordingEnabled = enabled;
    self.settings.showGhostBall = enabled;
    self.settings.showDetectedBalls = enabled;
    self.settings.showSideLines = enabled;
}

- (void)togglePredictions {
    const BOOL enable = !self.settings.predictionEnabled;
    [self setPredictionFeatureBundleEnabled:enable];
    if (!enable) {
        _lastResult = zg::Result();
        self.lastScanConfidence = 0.0;
        [self.canvasView setPredictionResult:_lastResult];
        [self stopLiveScan];
    }
    [self recompute];
}

- (void)toggleCuePrediction { self.settings.cuePredictionEnabled = !self.settings.cuePredictionEnabled; [self recompute]; }
- (void)togglePocketPrediction { self.settings.pocketPredictionEnabled = !self.settings.pocketPredictionEnabled; [self recompute]; }
- (void)toggleBankPrediction { self.settings.bankPredictionEnabled = !self.settings.bankPredictionEnabled; [self recompute]; }
- (void)toggleCaromPrediction { self.settings.caromPredictionEnabled = !self.settings.caromPredictionEnabled; [self recompute]; }
- (void)toggleLadderGuide { self.settings.ladderGuideEnabled = !self.settings.ladderGuideEnabled; [self recompute]; }
- (void)toggleCollisionWarnings { self.settings.collisionWarningEnabled = !self.settings.collisionWarningEnabled; [self recompute]; }
- (void)togglePocketHeat { self.settings.pocketHeatEnabled = !self.settings.pocketHeatEnabled; [self recompute]; }
- (void)toggleFourLines { self.settings.fourLinePredictionEnabled = !self.settings.fourLinePredictionEnabled; [self recompute]; }
- (void)toggleHiddenRecording { self.settings.hiddenLineRecordingEnabled = !self.settings.hiddenLineRecordingEnabled; [self recompute]; }
- (void)toggleLiveScan { self.settings.liveScanEnabled = !self.settings.liveScanEnabled; [self refreshLiveScanState]; [self recompute]; }
- (void)toggleScanSmoothing { self.settings.scanSmoothingEnabled = !self.settings.scanSmoothingEnabled; _stabilizer.reset(); [self recompute]; }

- (void)cycleShotMode {
    self.settings.shotMode = (ZGPookingShotMode)((self.settings.shotMode + 1) % 4);
    [self recompute];
}

- (void)cycleRoute {
    self.settings.scanRoute = (ZGScanRoute)((self.settings.scanRoute + 1) % 4);
    [self recompute];
}

- (void)cycleStyle {
    self.settings.predictionStyle = (ZGPookingStyle)((self.settings.predictionStyle + 1) % 3);
    [self recompute];
}

- (void)nextPocket {
    if (!self.settings.manualPocket) {
        self.settings.manualPocket = YES;
        self.settings.selectedPocket = 0;
    } else {
        self.settings.selectedPocket += 1;
        if (self.settings.selectedPocket >= 6) {
            self.settings.selectedPocket = 0;
            self.settings.manualPocket = NO;
        }
    }
    [self recompute];
}

- (void)nextBounces {
    self.settings.maxBounces = (self.settings.maxBounces + 1) % 7;
    [self recompute];
}

- (void)nextLineLength {
    self.settings.lineLength += 0.10;
    if (self.settings.lineLength > 1.35) self.settings.lineLength = 0.45;
    [self recompute];
}

- (void)toggleGhost { self.settings.showGhostBall = !self.settings.showGhostBall; [self recompute]; }
- (void)toggleBalls { self.settings.showDetectedBalls = !self.settings.showDetectedBalls; [self recompute]; }
- (void)toggleSideLines { self.settings.showSideLines = !self.settings.showSideLines; [self recompute]; }

- (BOOL)shouldRunLiveScan {
    return self.settings.predictionEnabled &&
           self.settings.liveScanEnabled &&
           self.superview != nil &&
           !self.hidden &&
           self.alpha > 0.01;
}

- (void)refreshLiveScanState {
    if ([self shouldRunLiveScan]) {
        [self startLiveScanIfNeeded];
    } else {
        [self stopLiveScan];
    }
}

- (void)startLiveScanIfNeeded {
    if (self.liveScanLink) return;
    self.lastLiveScanTimestamp = 0.0;
    self.liveScanLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(liveScanTick:)];
    if (@available(iOS 15.0, *)) {
        self.liveScanLink.preferredFrameRateRange = CAFrameRateRangeMake(8, 12, 12);
    } else {
        self.liveScanLink.preferredFramesPerSecond = 10;
    }
    [self.liveScanLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)stopLiveScan {
    [self.liveScanLink invalidate];
    self.liveScanLink = nil;
    self.liveScanBusy = NO;
}

- (void)liveScanTick:(CADisplayLink *)link {
    if (self.liveScanBusy || ![self shouldRunLiveScan]) return;
    const CFTimeInterval now = CACurrentMediaTime();
    if (self.lastLiveScanTimestamp > 0.0 && now - self.lastLiveScanTimestamp < 0.10) return;
    self.lastLiveScanTimestamp = now;
    self.liveScanBusy = YES;
    [self captureAndScanHostView];
    self.liveScanBusy = NO;
}

- (BOOL)captureAndScanHostView {
    UIView *host = self.superview ?: self;
    const CGSize hostSize = host.bounds.size;
    if (hostSize.width < 64.0 || hostSize.height < 64.0) return NO;

    const CGFloat maxDimension = 1280.0;
    const CGFloat longest = MAX(hostSize.width, hostSize.height);
    const CGFloat captureScale = MIN(1.0, maxDimension / MAX(1.0, longest));
    const NSUInteger pixelWidth = MAX((NSUInteger)1, (NSUInteger)std::llround(hostSize.width * captureScale));
    const NSUInteger pixelHeight = MAX((NSUInteger)1, (NSUInteger)std::llround(hostSize.height * captureScale));
    const NSUInteger bytesPerRow = pixelWidth * 4;
    std::vector<uint8_t> pixels(bytesPerRow * pixelHeight, 0);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels.data(),
                                                 pixelWidth,
                                                 pixelHeight,
                                                 8,
                                                 bytesPerRow,
                                                 colorSpace,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(colorSpace);
    if (!context) return NO;

    UIGraphicsPushContext(context);
    CGContextClearRect(context, CGRectMake(0, 0, pixelWidth, pixelHeight));
    CGContextScaleCTM(context, captureScale, captureScale);

    BOOL drewSibling = NO;
    for (UIView *subview in host.subviews) {
        if (subview == self) break;
        if (subview.hidden || subview.alpha <= 0.01) continue;
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, subview.frame.origin.x, subview.frame.origin.y);
        BOOL drew = [subview drawViewHierarchyInRect:subview.bounds afterScreenUpdates:NO];
        if (!drew) {
            [subview.layer renderInContext:context];
        }
        CGContextRestoreGState(context);
        drewSibling = YES;
    }

    if (!drewSibling) {
        const BOOL wasHidden = self.hidden;
        self.hidden = YES;
        BOOL drew = [host drawViewHierarchyInRect:host.bounds afterScreenUpdates:NO];
        if (!drew) {
            [host.layer renderInContext:context];
        }
        self.hidden = wasHidden;
    }

    UIGraphicsPopContext();
    CGContextRelease(context);

    const CGFloat coordinateScale = captureScale <= 0.0 ? 1.0 : (1.0 / captureScale);
    return [self updateFromFrameBytes:pixels.data()
                                width:pixelWidth
                               height:pixelHeight
                          bytesPerRow:bytesPerRow
                           pixelFormat:ZGPookingPixelFormatBGRA8888
                      coordinateScale:coordinateScale];
}

- (void)recompute {
    _lastResult = _engine.compute(_state, [self.settings zg_settings]);
    [self.canvasView setPredictionResult:_lastResult];
    [self refreshLiveScanState];
    [self updateMenuState];
}

- (void)updateButton:(UIButton *)button title:(NSString *)title active:(BOOL)active {
    if (!button) return;
    [button setTitle:title forState:UIControlStateNormal];
    if (active) {
        button.backgroundColor = [UIColor colorWithRed:0.02 green:0.42 blue:0.58 alpha:0.60];
        button.layer.borderColor = [UIColor colorWithRed:0.18 green:0.92 blue:1.0 alpha:0.58].CGColor;
        [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    } else {
        button.backgroundColor = [UIColor colorWithRed:0.02 green:0.03 blue:0.04 alpha:0.26];
        button.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.12].CGColor;
        [button setTitleColor:[UIColor colorWithWhite:1 alpha:0.86] forState:UIControlStateNormal];
    }
    button.layer.borderWidth = 0.8;
}

- (void)updateQuickToggle {
    NSString *title = self.settings.predictionEnabled ? @"AIM ON" : @"AIM OFF";
    [_quickToggleButton setTitle:title forState:UIControlStateNormal];
    if (self.settings.predictionEnabled) {
        _quickToggleButton.backgroundColor = [UIColor colorWithRed:0.00 green:0.58 blue:0.76 alpha:0.96];
        _quickToggleButton.layer.borderColor = [UIColor colorWithRed:0.34 green:0.95 blue:1.0 alpha:0.72].CGColor;
        _quickToggleButton.layer.shadowColor = UIColor.cyanColor.CGColor;
        _quickToggleButton.layer.shadowOpacity = 0.34;
        _quickToggleButton.layer.shadowRadius = 10;
    } else {
        _quickToggleButton.backgroundColor = [UIColor colorWithRed:0.10 green:0.11 blue:0.14 alpha:0.96];
        _quickToggleButton.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.18].CGColor;
        _quickToggleButton.layer.shadowOpacity = 0.0;
    }
}

- (void)updateMenuState {
    [self updateQuickToggle];
    [self updateButton:self.enabledButton
                 title:[NSString stringWithFormat:@"Enable Prediction     %@", ZGOnOff(self.settings.predictionEnabled)]
                active:self.settings.predictionEnabled];
    [self updateButton:self.cueButton
                 title:[NSString stringWithFormat:@"Cue Prediction        %@", ZGOnOff(self.settings.cuePredictionEnabled)]
                active:self.settings.cuePredictionEnabled];
    [self updateButton:self.pocketPathButton
                 title:[NSString stringWithFormat:@"Pocket Path           %@", ZGOnOff(self.settings.pocketPredictionEnabled)]
                active:self.settings.pocketPredictionEnabled];
    [self updateButton:self.bankPathButton
                 title:[NSString stringWithFormat:@"Bank Paths            %@", ZGOnOff(self.settings.bankPredictionEnabled)]
                active:self.settings.bankPredictionEnabled];
    [self updateButton:self.caromButton
                 title:[NSString stringWithFormat:@"Carom Path            %@", ZGOnOff(self.settings.caromPredictionEnabled)]
                active:self.settings.caromPredictionEnabled];
    [self updateButton:self.ladderButton
                 title:[NSString stringWithFormat:@"Pooking Ladder        %@", ZGOnOff(self.settings.ladderGuideEnabled)]
                active:self.settings.ladderGuideEnabled];
    [self updateButton:self.collisionButton
                 title:[NSString stringWithFormat:@"Block Warning         %@", ZGOnOff(self.settings.collisionWarningEnabled)]
                active:self.settings.collisionWarningEnabled];
    [self updateButton:self.pocketHeatButton
                 title:[NSString stringWithFormat:@"Pocket Heat           %@", ZGOnOff(self.settings.pocketHeatEnabled)]
                active:self.settings.pocketHeatEnabled];
    [self updateButton:self.fourLineButton
                 title:[NSString stringWithFormat:@"Four Line Style       %@", ZGOnOff(self.settings.fourLinePredictionEnabled)]
                active:self.settings.fourLinePredictionEnabled];
    [self updateButton:self.hiddenRecordButton
                 title:[NSString stringWithFormat:@"Hidden Line Buffer    %@", ZGOnOff(self.settings.hiddenLineRecordingEnabled)]
                active:self.settings.hiddenLineRecordingEnabled];
    [self updateButton:self.liveScanButton
                 title:[NSString stringWithFormat:@"Live Scanner          %@", ZGOnOff(self.settings.liveScanEnabled)]
                active:self.settings.liveScanEnabled];
    [self updateButton:self.scanSmoothButton
                 title:[NSString stringWithFormat:@"Scanner Smoothing     %@", ZGOnOff(self.settings.scanSmoothingEnabled)]
                active:self.settings.scanSmoothingEnabled];
    [self updateButton:self.shotModeButton
                 title:[NSString stringWithFormat:@"Shot Mode             %@", ZGShotModeName(self.settings.shotMode)]
                active:self.settings.shotMode != ZGPookingShotModeAuto];
    [self updateButton:self.routeButton
                 title:[NSString stringWithFormat:@"Scan Route            %@", ZGRouteName(self.settings.scanRoute)]
                active:YES];
    [self updateButton:self.styleButton
                 title:[NSString stringWithFormat:@"Prediction Style      %@", ZGStyleName(self.settings.predictionStyle)]
                active:YES];
    [self updateButton:self.selectedPocketButton
                 title:[NSString stringWithFormat:@"Pocket Target         %@", ZGPocketName(self.settings.selectedPocket, self.settings.manualPocket)]
                active:self.settings.manualPocket];
    [self updateButton:self.bounceButton
                 title:[NSString stringWithFormat:@"Bounds / Bounces      %ld", (long)self.settings.maxBounces]
                active:self.settings.maxBounces > 0];
    [self updateButton:self.lengthButton
                 title:[NSString stringWithFormat:@"Line Reach            %.0f%%", self.settings.lineLength * 100.0]
                active:YES];
    [self updateButton:self.ghostButton
                 title:[NSString stringWithFormat:@"Ghost Ball            %@", ZGOnOff(self.settings.showGhostBall)]
                active:self.settings.showGhostBall];
    [self updateButton:self.ballsButton
                 title:[NSString stringWithFormat:@"Ball Markers          %@", ZGOnOff(self.settings.showDetectedBalls)]
                active:self.settings.showDetectedBalls];
    [self updateButton:self.sideLinesButton
                 title:[NSString stringWithFormat:@"Table Centerlines     %@", ZGOnOff(self.settings.showSideLines)]
                active:self.settings.showSideLines];

    NSString *scanner = self.liveScanLink ? @"LIVE" : @"IDLE";
    self.statusLabel.text = [NSString stringWithFormat:@"ZavIOS | %@ | %@ | %@ | H%lu | %.0f%%",
                             ZGOnOff(self.settings.predictionEnabled),
                             scanner,
                             ZGShotModeName(self.settings.shotMode),
                             (unsigned long)_lastResult.hiddenLines.size(),
                             self.lastScanConfidence * 100.0];
}

- (void)updateTable:(ZGOverlayRect)table
            cueBall:(ZGOverlayPoint)cueBall
         hasCueBall:(BOOL)hasCueBall
              balls:(const ZGOverlayBall *)balls
              count:(NSUInteger)count
              guide:(ZGOverlayGuideLine)guide {
    _state.table = {table.x, table.y, table.width, table.height};
    _state.cueBall = {cueBall.x, cueBall.y};
    _state.hasCueBall = hasCueBall;
    _state.guide.valid = guide.valid;
    _state.guide.start = {guide.start.x, guide.start.y};
    _state.guide.end = {guide.end.x, guide.end.y};
    _state.balls.clear();
    if (!balls && count > 0) {
        count = 0;
    }
    for (NSUInteger i = 0; i < count; ++i) {
        zg::Ball ball;
        ball.center = {balls[i].center.x, balls[i].center.y};
        ball.radius = balls[i].radius;
        ball.number = (int)balls[i].number;
        ball.cue = balls[i].cue;
        _state.balls.push_back(ball);
    }
    _stabilizer.reset();
    [self recompute];
}

- (BOOL)updateFromFrameBytes:(const uint8_t *)bytes
                       width:(NSUInteger)width
                      height:(NSUInteger)height
                 bytesPerRow:(NSUInteger)bytesPerRow
                  pixelFormat:(ZGPookingPixelFormat)pixelFormat
              coordinateScale:(CGFloat)coordinateScale {
    if (!bytes || width == 0 || height == 0 || bytesPerRow < width * 4) {
        self.lastScanConfidence = 0.0;
        [self updateMenuState];
        return NO;
    }
    zg::FrameScanOptions options;
    options.pixelFormat = pixelFormat == ZGPookingPixelFormatBGRA8888 ? zg::PixelFormat::BGRA8888 : zg::PixelFormat::RGBA8888;
    options.maxBalls = 16;
    options.sampleStep = 2;
    const zg::FrameScanResult scan = zg::FrameScanner::scan(bytes, width, height, bytesPerRow, options);
    if (!scan.valid) {
        self.lastScanConfidence = 0.0;
        [self updateMenuState];
        return NO;
    }
    zg::FrameStabilizerOptions smoothing;
    smoothing.enabled = self.settings.scanSmoothingEnabled;
    zg::GameState scaled = scan.state;
    ZGScaleGameState(scaled, coordinateScale);
    _state = _stabilizer.update(scaled, scan.confidence, smoothing);
    self.lastScanConfidence = scan.confidence;
    [self recompute];
    return YES;
}

- (BOOL)updateFromFrameBytes:(const uint8_t *)bytes
                       width:(NSUInteger)width
                      height:(NSUInteger)height
                 bytesPerRow:(NSUInteger)bytesPerRow
                  pixelFormat:(ZGPookingPixelFormat)pixelFormat {
    return [self updateFromFrameBytes:bytes
                                width:width
                               height:height
                          bytesPerRow:bytesPerRow
                           pixelFormat:pixelFormat
                       coordinateScale:1.0];
}

- (NSUInteger)hiddenLineCount {
    return _lastResult.hiddenLines.size();
}

- (NSUInteger)copyHiddenLines:(ZGOverlayLine *)outLines maxCount:(NSUInteger)maxCount {
    const NSUInteger count = _lastResult.hiddenLines.size();
    if (!outLines || maxCount == 0) {
        return count;
    }
    const NSUInteger copied = MIN(count, maxCount);
    for (NSUInteger i = 0; i < copied; ++i) {
        outLines[i] = ZGExportLine(_lastResult.hiddenLines[i]);
    }
    return copied;
}

@end

static ZGPookingOverlayView *ZGSharedOverlayView = nil;
static ZGPookingOverlaySettings *ZGSharedSettings = nil;

@implementation ZGPookingOverlayController

+ (void)startInWindow:(UIWindow *)window {
    if (!window) return;
    [self startInView:window];
}

+ (void)startInView:(UIView *)view {
    if (!view) return;
    if (ZGSharedOverlayView) {
        [ZGSharedOverlayView removeFromSuperview];
        ZGSharedOverlayView = nil;
    }
    ZGSharedOverlayView = [[ZGPookingOverlayView alloc] initWithFrame:view.bounds];
    if (ZGSharedSettings) {
        ZGSharedOverlayView.settings = [ZGSharedSettings copy];
        [ZGSharedOverlayView recompute];
    }
    [view addSubview:ZGSharedOverlayView];
}

+ (void)stop {
    [ZGSharedOverlayView removeFromSuperview];
    ZGSharedOverlayView = nil;
}

+ (void)setVisible:(BOOL)visible {
    ZGSharedOverlayView.hidden = !visible;
    [ZGSharedOverlayView refreshLiveScanState];
}

+ (NSUInteger)hiddenLineCount {
    return ZGSharedOverlayView ? [ZGSharedOverlayView hiddenLineCount] : 0;
}

+ (NSUInteger)copyHiddenLines:(ZGOverlayLine *)outLines maxCount:(NSUInteger)maxCount {
    return ZGSharedOverlayView ? [ZGSharedOverlayView copyHiddenLines:outLines maxCount:maxCount] : 0;
}

+ (ZGPookingOverlaySettings *)settings {
    if (ZGSharedOverlayView) return [ZGSharedOverlayView.settings copy];
    if (!ZGSharedSettings) ZGSharedSettings = [ZGPookingOverlaySettings defaults];
    return [ZGSharedSettings copy];
}

+ (void)applySettings:(ZGPookingOverlaySettings *)settings {
    ZGSharedSettings = [settings copy];
    if (ZGSharedOverlayView) {
        ZGSharedOverlayView.settings = [settings copy];
        [ZGSharedOverlayView recompute];
    }
}

+ (void)updateTable:(ZGOverlayRect)table
            cueBall:(ZGOverlayPoint)cueBall
         hasCueBall:(BOOL)hasCueBall
              balls:(const ZGOverlayBall *)balls
              count:(NSUInteger)count
              guide:(ZGOverlayGuideLine)guide {
    [ZGSharedOverlayView updateTable:table cueBall:cueBall hasCueBall:hasCueBall balls:balls count:count guide:guide];
}

+ (BOOL)updateFromFrameBytes:(const uint8_t *)bytes
                       width:(NSUInteger)width
                      height:(NSUInteger)height
                 bytesPerRow:(NSUInteger)bytesPerRow
                  pixelFormat:(ZGPookingPixelFormat)pixelFormat {
    return [ZGSharedOverlayView updateFromFrameBytes:bytes
                                              width:width
                                             height:height
                                        bytesPerRow:bytesPerRow
                                         pixelFormat:pixelFormat];
}

@end
