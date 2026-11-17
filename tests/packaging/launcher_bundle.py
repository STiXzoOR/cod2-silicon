#!/usr/bin/env python3
"""Audit the production launcher/nested-engine bundle, without starting it."""
import pathlib, plistlib, re, subprocess, sys
app = pathlib.Path(sys.argv[1]).resolve()
info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert info['CFBundleExecutable'] == 'CoD2Launcher'
assert info['NSHighResolutionCapable'] is True
assert info['CFBundleURLTypes'][0]['CFBundleURLSchemes'] == ['cod2x', 'cod2']
documents = {item['CFBundleTypeName']: item for item in info['CFBundleDocumentTypes']}
assert documents['Call of Duty 2 demo']['LSHandlerRank'] == 'Default'
assert documents['Call of Duty 2 config']['LSHandlerRank'] == 'Alternate'
types = {item['UTTypeIdentifier']: item['UTTypeTagSpecification']['public.filename-extension'] for item in info['UTImportedTypeDeclarations']}
assert types == {'io.github.stixzoor.cod2silicon.demo': ['dm_1'], 'io.github.stixzoor.cod2silicon.config': ['cfg']}, types
game = app / 'Contents/Helpers/CoD2 Game.app'
engine = plistlib.loads((game / 'Contents/Info.plist').read_bytes())
assert engine['CFBundleExecutable'] == 'cod2_macos'
assert engine['CFBundleIdentifier'] == info['CFBundleIdentifier'] + '.game'
assert engine['LSSupportsGameMode'] is True
assert engine['CoD2AutomaticShaderSetup'] is False
assert not {'CFBundleURLTypes', 'CFBundleDocumentTypes', 'UTImportedTypeDeclarations'} & set(engine)
assert 'CoD2GameDirectory' not in info
for contents, plist in [(app / 'Contents', info), (game / 'Contents', engine)]:
    assert plist['LSMinimumSystemVersion'] == '26.0', plist['LSMinimumSystemVersion']
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
binaries = [app / 'Contents/MacOS/CoD2Launcher', *sorted((game / 'Contents/MacOS').iterdir()),
            game / 'Contents/Helpers/cod2-shadergen']
binaries += [p for p in (game / 'Contents/Frameworks').glob('*.dylib') if not p.is_symlink()]
# Both engines: CoD2x (the helper's executable) and the original 1.3 beside it, chosen in Settings,
# each built twice: drawing with OpenGL, and with Metal through libd3d9metal.
# And each one's dedicated server, which Servers > Hosting starts.
assert sorted(p.name for p in binaries) == ['CoD2Launcher', 'cod2-shadergen', 'cod2_macos', 'cod2_macos_ded', 'cod2_macos_ded_stock',
                                            'cod2_macos_metal', 'cod2_macos_stock', 'cod2_macos_stock_metal',
                                            'libSDL2-2.0.0.dylib', 'libSDL3.0.dylib', 'libd3d9metal.dylib', 'libvkd3d-shader.1.dylib'], binaries
symbols = {p.name: subprocess.check_output(['nm', '-g', str(p)], text=True) for p in (game / 'Contents/MacOS').iterdir()}
clients = ('cod2_macos', 'cod2_macos_stock', 'cod2_macos_metal', 'cod2_macos_stock_metal')
assert all(('_Cod2x_Init' in symbols[name]) == ('stock' not in name) for name in clients)
assert all('_Cod2xNativeApp_Arguments' in symbols[name] for name in clients)
for name in clients:
    links = subprocess.check_output(['otool', '-L', str(game / 'Contents/MacOS' / name)], text=True)
    assert ('@rpath/libd3d9metal.dylib' in links) == name.endswith('_metal'), (name, links)
licences = {p.name for p in (app / 'Contents/Resources/licenses').iterdir()}
assert {'mtld3d-LICENSE.txt', 'mtld3d-crates.txt'} <= licences, licences
assert '_SV_Cod2xMasterHeartbeat' in symbols['cod2_macos_ded'] and '_SV_Cod2xMasterHeartbeat' not in symbols['cod2_macos_ded_stock']
for name, identifier in (('cod2_macos_ded', '.server'), ('cod2_macos_ded_stock', '.server.stock')):
    server = game / 'Contents/MacOS' / name
    # A server has no window, sound or renderer: system libraries only, and no entitlements.
    for line in subprocess.check_output(['otool', '-L', str(server)], text=True).splitlines()[1:]:
        assert line.strip().startswith(('/System/', '/usr/lib/')), (name, line)
    held = subprocess.run(['codesign', '-d', '--entitlements', '-', '--xml', str(server)], capture_output=True, check=True).stdout
    assert not held or plistlib.loads(held) == {}, (name, held)
    signed = subprocess.run(['codesign', '-dv', str(server)], capture_output=True, text=True, check=True).stderr
    assert 'Identifier=' + info['CFBundleIdentifier'] + identifier + '\n' in signed, signed
for name, identifier in (('cod2_macos_stock', '.game.stock'), ('cod2_macos_metal', '.game.metal'),
                         ('cod2_macos_stock_metal', '.game.stock.metal')):
    held = subprocess.run(['codesign', '-d', '--entitlements', '-', '--xml', str(game / 'Contents/MacOS' / name)],
                          capture_output=True, check=True)
    assert plistlib.loads(held.stdout) == {'com.apple.security.device.audio-input': True}, (name, held.stdout)
    signature = subprocess.run(['codesign', '-dv', str(game / 'Contents/MacOS' / name)], capture_output=True, text=True, check=True).stderr
    assert 'Identifier=' + info['CFBundleIdentifier'] + identifier + '\n' in signature, signature
for path in binaries:
    subprocess.run(['lipo', str(path), '-verify_arch', 'arm64'], check=True)
    build = subprocess.check_output(['vtool', '-show-build', str(path)], text=True)
    assert re.search(r'minos 26\.0(?:\.0)?\s', build), (path, build)
    links = subprocess.check_output(['otool', '-L', str(path)], text=True).splitlines()[1:]
    for line in links:
        link = line.strip().rsplit(' (', 1)[0]
        assert link.startswith(('/System/', '/usr/lib/', '@rpath/', '@executable_path/')), link
    subprocess.run(['codesign', '--verify', '--strict', str(path)], check=True)
for path in app.rglob('*'):
    assert path.suffix not in {'.iwd', '.py', '.dm_1', '.dm_2'}, path
    assert path.name not in {'preferences', 'manifest.json', 'data-path.txt'}, path
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
print('PASS: launcher + isolated Game Mode helper with CoD2x and original 1.3 engines, one URL owner, each also drawing with Metal, both dedicated servers, twelve arm64/macOS26 Mach-Os, portable links, deep signature, no game content, icon G in both apps, OFL fonts with licences')
