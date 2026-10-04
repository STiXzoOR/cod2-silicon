# CoD2 Silicon app icon (icon G)

`CoD2 Silicon.icon` is an Icon Composer document: a stenciled 2 over a raised
star on olive steel, from the approved launcher design. It is original art. It
uses no font, game data, copied logo or third-party artwork.

## Document

The olive background is the document `fill`, a linear gradient from `#6f7a44`
to `#323819`. Dark uses its own olive-black gradient from `#262a1b` to
`#0c0d08`. There are three groups, listed front to back (the first group
renders on top):

| Group | Layer | Shadow | Translucency | Appearance variants |
| --- | --- | --- | --- | --- |
| Stencil 2 | `stencil-two.svg`: cream paint with two stencil bridges and worn chips | layer colour, 0.5 | off | Dark uses deeper paint; mono uses a solid white fill |
| Raised star | `star.svg`: lit and shaded facets | neutral, 0.5 | off | Dark and mono use darker facets, so the numeral leads |
| Plate | `plate.svg`: inner bevel line and four rivets | none | 0.3 | Dark lowers the line and rivets |

The design had a third bridge on the 2's top-left hook. At Dock sizes (32–64 px)
it read as a separate tick mark, so it was removed and the crown stays whole.

The layers are flat colour. Icon Composer supplies the specular highlights,
refraction, translucency and shadows, and masks the canvas to the macOS shape.
`layers.py` writes every SVG. It rescales the design board's 100–924 tile to
the full 1024 canvas, and seeds the paint chips so its output is reproducible.

The JSON schema is not a published API. Its keys were checked against the
strings in Icon Composer's own frameworks, and the installed `ictool` and
`actool` validated the files. A key and its `-specializations` form must not
both be present: if they are, the specializations are ignored.

## Build and integration

```sh
scripts/compile-launcher-icon.sh output/ws28/icon-build
```

Xcode 27's `actool` compiles the package directly, with no Xcode project, for a
macOS 13.0 minimum. It writes:

- `Assets.car`, with layered `IconImageStack` and `IconGroup` renditions for
  Aqua, DarkAqua and tintable appearances, plus flattened renditions;
- `CoD2 Silicon.icns`, the static fallback;
- `icon-info.plist`, with `CFBundleIconName` and `CFBundleIconFile` set to
  `CoD2 Silicon`.

`tools/cod2x/make_macos_app.py` installs `Assets.car` and the ICNS into both the
outer launcher and the nested `CoD2 Game.app`, and merges the plist keys into
each `Info.plist` before signing. `actool` ships with Xcode, not the Command Line
Tools. Without it, the bundler draws the same composition with
`tools/cod2x/app_icon.swift` and packages a static `CoD2 Silicon.icns`.

## Review renditions

```sh
tools/cod2x/icon-source/render.sh output/ws28/icon
```

This uses Xcode's Icon Composer `ictool`, which renders through the same system
pipeline, to export `Default`, `Dark`, `ClearLight`, `ClearDark`, `TintedLight`
and `TintedDark` at 1024 px. The purple in the tinted renditions is the tool's
default preview tint. On a real Mac, the tint follows the user's choice.

Sources consulted:

- [Apple HIG: App icons](https://developer.apple.com/design/human-interface-guidelines/app-icons)
- [Create icons with Icon Composer, WWDC25](https://developer.apple.com/videos/play/wwdc2025/361/)
- [Creating your app icon using Icon Composer](https://developer.apple.com/documentation/xcode/creating-your-app-icon-using-icon-composer)
