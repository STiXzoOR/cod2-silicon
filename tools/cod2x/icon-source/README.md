# Original launcher icons

The default `CoD2Silicon.icon` uses a squared stencil 2 cut through an olive plate,
a khaki inlay and four small rivets. `Helmet.icon` is an original frontal helmet
silhouette over khaki. `DogTag.icon` is a punched steel tag with the same squared
stencil stamp. None uses a font, game data, copied logos or third-party artwork.
The SVGs contain flat colors and hard-edged vector paths; the material, masking,
lighting, shadows and appearance adaptation come from Icon Composer.

The default stencil outline is a 1024-unit path:

```text
M304 242 H620 Q720 242 720 342 V426 Q720 474 680 506
L394 716 H720 V794 H304 V696 L624 456 V342
Q624 326 608 326 H304 Z
```

The three bridge shapes mask the outline; masking clips bridges to the glyph
and avoids projecting extra shapes outside it:

```text
M448 230 H472 V340 H448 Z
M432 444 L456 426 L646 674 L622 692 Z
M560 700 H584 V810 H560 Z
```

This direction can be reused for the launcher's original wordmark and sidebar
mark. It deliberately does not resemble the official Call of Duty logotype.

## Build and integration

```sh
scripts/compile-launcher-icon.sh output/ws25/icon-build
```

Verified with Xcode 27.0 `actool`: this compiles the `.icon` package directly,
without an Xcode project or GUI, targeting macOS 13.0. It emits:

- `Assets.car`, including layered `IconImageStack`, `IconGroup` and SVG vector
  renditions, plus flattened compatibility renditions;
- `CoD2Silicon.icns`, the static fallback;
- `icon-info.plist`, containing `CFBundleIconName = CoD2Silicon` and
  `CFBundleIconFile = CoD2Silicon`.

Copy `Assets.car` and the ICNS into each app's `Contents/Resources`, and merge the
partial plist keys into that app's `Info.plist` before signing. Do not package
the intermediate plist. The helper accepts an optional second argument selecting
`Helmet.icon` or `DogTag.icon` and emits their corresponding icon names.

The JSON schema isn't a published stable API. The background/package structure
was checked against Xcode's installed macOS Icon Composer template; the
specialization and glass annotation structure was checked against Apple's
Landmarks sample. `actool`, `ictool` and `assetutil` validated the actual files.
Older SDKs that don't support `.icon` can still run the existing static fallback:

```sh
swift tools/cod2x/app_icon.swift output/ws25/Native.iconset
iconutil -c icns output/ws25/Native.iconset -o output/ws25/Native.icns
```

No Icon Composer tool or Homebrew dependency is needed at runtime. This generator
omits tiny rivets below 128px for a cleaner small-scale fallback.

## Review renditions

```sh
tools/cod2x/icon-source/render.sh output/ws25/screens-v2/icons
```

Uses the installed Xcode Icon Composer `ictool` to render the genuine macOS 26
design at 1024px. Each concept has `Default`, `Dark`, `ClearLight`, `ClearDark`,
`TintedLight` and `TintedDark` PNGs named `<concept>-<appearance>.png`. The purple
tint is the tool's default preview tint, not a fixed app color. Icon tint remains
the user's system choice. White mono annotations preserve the primary silhouette
in clear/tinted appearances.

All 18 renditions were reviewed in three iterations. The first review exposed
reversed layer ordering and an obscured glyph. The second fixed the layer order
and the stencil bridges, then exposed weak mono contrast and excessive material
on the dog tag stamp. The final version clips each bridge to the glyph, uses
white mono annotations, keeps stamped ink flat, and reduces the group's shadow
and translucency. Icon Composer applies the outer canvas mask itself.

Sources consulted directly:

- [Apple HIG: App icons](https://developer.apple.com/design/human-interface-guidelines/app-icons)
  — vector layers, clearly defined edges, balanced foreground and background,
  system-applied masking, and review of every appearance.
- [Say hello to the new look of app icons, WWDC25](https://developer.apple.com/videos/play/wwdc2025/220/)
  — frontal silhouettes, generous breathing room, bold details and soft gradients.
- [Create icons with Icon Composer, WWDC25](https://developer.apple.com/videos/play/wwdc2025/361/)
  — flat source artwork, outlined type, background/foreground separation and
  dynamic materials supplied by the system.
- [Creating your app icon using Icon Composer](https://developer.apple.com/documentation/xcode/creating-your-app-icon-using-icon-composer)
  — compiler-generated compatibility icons for older deployment targets.
- [Landmarks: Building an app with Liquid Glass](https://developer.apple.com/documentation/swiftui/landmarks-building-an-app-with-liquid-glass)
  — annotation/schema reference only; no sample artwork was copied.
