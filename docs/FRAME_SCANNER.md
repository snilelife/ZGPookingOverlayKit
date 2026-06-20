# Frame Scanner

The scanner is a fallback path for host-fed frames.

It expects the host app to pass RGBA or BGRA pixels it already has access to:

```objc
BOOL ok = ZGPookingOverlayUpdateFromFrameBytes(bytes,
                                               width,
                                               height,
                                               bytesPerRow,
                                               ZGPookingPixelFormatBGRA8888);
```

Scanner behavior:

```text
1. Estimate the Pooking table rectangle from gray/blue/green table pixels.
2. Detect ball-like connected components inside the table area.
3. Pick the likely cue ball from bright/low-saturation components.
4. Probe rays from the cue ball to detect the visible Pooking guide line.
5. Feed the detected table/cue/balls/guide into the same prediction engine.
6. Show scan confidence in the menu status line.
```

Prediction behavior after scanning:

```text
If a guide line is detected or supplied by the host, the guide direction drives the contacted-ball solver.
The solver calculates the ghost-ball impact from the guide ray offset, so object paths and cue-after-hit paths react to thin hits versus full hits.
In Bank Shot mode, that live guide direction is extended through multiple rail contacts before drawing the pocket leg.
If no guide is available, the engine falls back to geometric best-shot selection.
Four-line mode outputs cue, object, carom, and bank/bounce paths.
Hidden recording stores alternate paths, including one-rail and two-rail bank routes, in Result.hiddenLines and exposes them through ZGPookingOverlayHiddenLineCount() and ZGPookingOverlayCopyHiddenLines().
```

Implementation notes:

```text
The scanner separates saturated colored-ball pixels from pale guide pixels so guide lines do not merge into ball components.
White cue-ball detection uses a stricter round-component pass, which keeps the cue ball while ignoring long guide strokes.
Guide detection scores bright/cyan/violet pixels along rays from the cue ball and updates state.guide automatically.
It also accepts lower-contrast light segments when they are brighter than the nearby cloth, which matches Pooking's alpha-textured AimLine shader.
Head-on cue-after-hit prediction uses a short finite stop marker instead of generating invalid bounce segments.
FrameStabilizer smooths scanner-fed table/cue/ball/guide updates to reduce jitter without delaying large guide movement.
```

Stabilizer behavior:

```text
Small frame-to-frame cue or guide movement is blended.
Large guide movement snaps immediately so the aim line still feels live.
Balls are matched to the previous frame by nearest position and cue/non-cue identity.
The UIKit menu exposes SCAN SMOOTH ON/OFF for testing.
Direct host geometry updates are not smoothed; only scanner-fed frames use the stabilizer.
```

Accuracy notes:

```text
Direct game geometry is still best.
Frame scanning depends on lighting, table skin, UI overlap, and frame scale.
Use BGRA8888 for typical iOS pixel buffers unless your source is explicitly RGBA.
Prediction is still OFF by default; the user must enable AIM ON or the host must set predictionEnabled = YES.
```
