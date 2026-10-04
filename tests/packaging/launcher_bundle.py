#!/usr/bin/env python3
"""Audit the production launcher/nested-engine bundle, without starting it."""
import pathlib, plistlib, re, subprocess, sys
app = pathlib.Path(sys.argv[1]).resolve()
info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert info['CFBundleExecutable'] == 'CoD2Launcher'
assert info['NSHighResolutionCapable'] is True
assert info['CFBundleURLTypes'][0]['CFBundleURLSchemes'] == ['cod2x']
game = app / 'Contents/Helpers/CoD2 Game.app'
engine = plistlib.loads((game / 'Contents/Info.plist').read_bytes())
assert engine['CFBundleExecutable'] == 'cod2_macos'
assert engine['CFBundleIdentifier'] == info['CFBundleIdentifier'] + '.game'
assert engine['LSSupportsGameMode'] is True
assert engine['CoD2AutomaticShaderSetup'] is False
assert 'CFBundleURLTypes' not in engine
assert 'CoD2GameDirectory' not in info
for contents, plist in [(app / 'Contents', info), (game / 'Contents', engine)]:
    assert plist['CFBundleIconFile'] == 'CoD2 Silicon', plist.get('CFBundleIconFile')
    assert (contents / 'Resources/CoD2 Silicon.icns').is_file()
    if 'CFBundleIconName' in plist:  # layered Icon Composer build (Xcode actool)
        assert plist['CFBundleIconName'] == 'CoD2 Silicon'
        assert (contents / 'Resources/Assets.car').is_file()
        assert plist['NSAccentColorName'] == 'AccentColor'
assert info['ATSApplicationFontsPath'] == 'Fonts'
fonts = {p.name for p in (app / 'Contents/Resources/Fonts').iterdir()}
assert fonts == {'BigShouldersStencilDisplay.ttf', 'CourierPrime-Regular.ttf', 'CourierPrime-Bold.ttf',
                 'OFL-BigShouldersStencilDisplay.txt', 'OFL-CourierPrime.txt'}, fonts
binaries = [app / 'Contents/MacOS/CoD2Launcher', game / 'Contents/MacOS/cod2_macos']
binaries += [p for p in (game / 'Contents/Frameworks').glob('*.dylib') if not p.is_symlink()]
assert len(binaries) == 4
for path in binaries:
    subprocess.run(['lipo', str(path), '-verify_arch', 'arm64'], check=True)
    build = subprocess.check_output(['vtool', '-show-build', str(path)], text=True)
    assert re.search(r'minos 13\.0(?:\.0)?\s', build), (path, build)
    links = subprocess.check_output(['otool', '-L', str(path)], text=True).splitlines()[1:]
    for line in links:
        link = line.strip().rsplit(' (', 1)[0]
        assert link.startswith(('/System/', '/usr/lib/', '@rpath/', '@executable_path/')), link
    subprocess.run(['codesign', '--verify', '--strict', str(path)], check=True)
for path in app.rglob('*'):
    assert path.suffix not in {'.iwd', '.py', '.dm_1', '.dm_2'}, path
    assert path.name not in {'preferences', 'manifest.json', 'data-path.txt'}, path
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
print('PASS: launcher + isolated Game Mode helper, one URL owner, four arm64/macOS13 Mach-Os, portable links, deep signature, no game content, icon G in both apps, OFL fonts with licences')
