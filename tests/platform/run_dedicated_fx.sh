#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
case "$(uname -s)" in
  Darwin) ;;
  *) echo "This native dedicated FX check requires macOS" >&2; exit 1 ;;
esac
mkdir -p build-macos
cc -arch arm64 -std=gnu99 -g -O0 -fcommon -ffp-contract=off \
  -DCOD2_X64=1 -DDEDICATED -fsanitize=address -Wl,-dead_strip \
  -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration \
  -Wno-error=int-conversion -Wno-error=implicit-int \
  -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
  -Isrc -Isrc/headers -I. \
  tests/platform/macos_dedicated_fx.c \
  src/PC/EffectsCore/GenericParser2.c src/PC/EffectsCore/FxTemplate.c \
  src/PC/EffectsCore/FxScheduler.c src/PC/EffectsCore/FxScheduler_load_obj.c \
  src/PC/EffectsCore/FxChannel.c src/PC/EffectsCore/FxCurve.c \
  src/PC/EffectsCore/FxCurve_load_obj.c \
  -o build-macos/headless-fx-parser \
  >build-macos/headless-fx-parser-build.log 2>&1
build-macos/headless-fx-parser
