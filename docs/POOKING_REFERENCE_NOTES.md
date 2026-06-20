# Pooking Reference Notes

Reference IPA inspected:

```text
com.pool.club.billiards.city-2.4.6-Decrypted.ipa
```

Observed metadata:

```text
CFBundleDisplayName: Pooking
CFBundleIdentifier: com.pool.club.billiards.city
CFBundleShortVersionString: 2.4.6
MinimumOSVersion: 13.0
DTSDKName: iphoneos18.2
```

Useful readable resources:

```text
AimLine.frag
AimLine.vert
AimLineOld.frag
AimLineOld.vert
BallShader.frag
BallShaderP.frag
BallShader.vert
TableShader.frag
TableShader.vert
FireCue.frag
FireCue.vert
FireCueProgress.frag
FireCueProgress.vert
```

Overlay design choices from that inspection:

```text
1. Prediction is OFF by default so no lines appear immediately at launch.
2. Pooking aim uses a ladder/segmented guide instead of only a plain solid line.
3. Shot mode has Auto, Long, Carom, and Bank paths.
4. Collision warnings mark blocking balls on the cue-to-ghost path.
5. Pocket heat draws pocket rings and emphasizes the selected manual pocket.
6. AimLine.frag samples a texture and multiplies by vertex alpha, so guide scanning must handle alpha-textured light segments over cloth.
7. Table resources include themed cloth/background names such as img/ui/table_bg.webp, img/ui/table_bg_11.jpg, and minigameTable7Cloth.png.
```

This package does not modify the IPA. It is a clean dynamic framework/dylib-ready source kit for a host app that intentionally links and starts it.

This kit is tuned for Pooking-style portrait tables with grey, blue, green, or themed table skins.

Current solver behavior:

```text
The frame scanner can detect the table, cue ball, saturated colored balls, dark balls, and bright/cyan/violet guide strokes.
Guide detection uses local contrast, matching Pooking's alpha-textured AimLine shader better than a pure solid-line detector.
The engine uses the guide angle first when available, so moving the in-game guide changes the cue guide, object path, carom path, and bank/bounce path.
Guide-driven ghost-ball geometry uses the aim ray offset against the object ball, so off-center hits create different object and cue-after-hit trajectories.
Head-on hits produce a short finite cue-ball stop marker instead of invalid bounce points.
In Bank Shot mode, guide-driven scans trace the current aim angle through multiple rail contacts before drawing the pocket leg.
Without a guide, Bank Shot mode calculates the ghost-ball point from the selected rail contact instead of from the final pocket.
Hidden bank recording keeps alternate one-rail and two-rail indirect routes.
The four-line pack is available in Auto, Long Shot, Carom Shot, and Bank Shot modes.
Hidden recording stores alternate cue/object/carom/bank/bounce paths for later retrieval.
The shot scorer penalizes blocked cue lanes, blocked object-to-pocket lanes, and blocked bank rail paths.
Scanner-fed updates use temporal smoothing so tiny frame noise does not make the guide flicker.
```

Regression coverage:

```text
tests/pooking_scanner_engine_test.cpp
  Verifies frame scanner guide detection and trajectory movement from two synthetic Pooking-like frames.
  Verifies lower-contrast textured guide detection inspired by AimLine.frag.
  Verifies scanner-fed prediction geometry remains finite.

tests/pooking_engine_modes_test.cpp
  Verifies all three visual styles across Auto, Long, Carom, and Bank modes.
  Verifies hidden line recording.
  Verifies cue, object, and cue-after-hit trajectories change when the guide changes.
  Verifies all visible and hidden geometry is finite.
  Verifies Bank Shot object paths target a rail before continuing to the pocket.
  Verifies angled Bank Shot output chains multiple rail legs.
  Verifies a blocked direct lane is avoided.

tests/pooking_stabilizer_test.cpp
  Verifies small scanner jitter is reduced.
  Verifies large guide movement still snaps immediately and changes the prediction.
```
