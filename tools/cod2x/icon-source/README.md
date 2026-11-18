# CoD2 Unbound app icon

`CoD2 Unbound.icon` is an Icon Composer document: a flat gold star with a cream
block 2 cut out of it, on a plain olive plate. It is original art. It uses no
font, game data, copied logo or third-party artwork.

The document is written by `tools/brand/artwork.py`, together with the logo
files in `docs/brand` and the shapes the launcher draws its own mark from
(`launcher/BrandShapes.swift`). One set of numbers there is the icon, the mark
and the wordmark; change the geometry in that file and run it again rather
than editing what it writes.

## Document

The olive plate is the document `fill`, a linear gradient from `#6f7a44` to
`#323819`. Dark uses an olive-black gradient from `#262a1b` to `#0c0d08`.
There are two groups, listed front to back (the first group renders on top):

| Group | Layer | Shadow | Appearance variants |
| --- | --- | --- | --- |
| Characters | `characters.svg`: the 2, a block numeral with cut, softened corners | layer colour, 0.5 | Dark uses a deeper cream; tinted uses solid white |
| Star | `star.svg`: the five-point star with the 2's shape, a little larger, cut out of it | neutral, 0.5 | Dark uses a deeper gold; tinted uses solid white |

The layers are flat shapes and the document's `fill-specializations` colour
them. Icon Composer supplies the highlights, translucency and shadows, and
masks the canvas to the macOS shape. There is no bevel line and there are no
rivets on the plate.

The JSON schema is not a published API. Its keys were checked against the
strings in Icon Composer's own frameworks, and the installed `ictool` and
`actool` validated the files. A key and its `-specializations` form must not
both be present: if they are, the specializations are ignored. Icon Composer
drops a stroke painted with a `userSpaceOnUse` gradient under a mask, so the
layers carry no gradients of their own.

## Build and integration

```sh
scripts/compile-launcher-icon.sh output/icon-build
```

Xcode 27's `actool` compiles the package directly, with no Xcode project, for a
macOS 26.0 minimum. It writes:

- `Assets.car`, with layered `IconImageStack` and `IconGroup` renditions for
  Aqua, DarkAqua and tintable appearances, plus flattened renditions;
- `CoD2 Unbound.icns`, the static fallback;
- `icon-info.plist`, with `CFBundleIconName` and `CFBundleIconFile` set to
  `CoD2 Unbound`.

`tools/cod2x/make_macos_app.py` installs `Assets.car` and the ICNS into both the
outer launcher and the nested `CoD2 Game.app`, and merges the plist keys into
each `Info.plist` before signing. `actool` ships with Xcode, not the Command Line
Tools. Without it, the bundler draws the same shapes with
`tools/cod2x/app_icon.swift` and packages a static `CoD2 Unbound.icns`.

## Review renditions

```sh
tools/cod2x/icon-source/render.sh output/icon
```

This uses Xcode's Icon Composer `ictool`, which renders through the same system
pipeline, to export `Default`, `Dark`, `ClearLight`, `ClearDark`, `TintedLight`
and `TintedDark` at 1024 px. The purple in the tinted renditions is the tool's
default preview tint. On a real Mac, the tint follows the user's choice.

Sources consulted:

- [Apple HIG: App icons](https://developer.apple.com/design/human-interface-guidelines/app-icons)
- [Create icons with Icon Composer, WWDC25](https://developer.apple.com/videos/play/wwdc2025/361/)
- [Creating your app icon using Icon Composer](https://developer.apple.com/documentation/xcode/creating-your-app-icon-using-icon-composer)
