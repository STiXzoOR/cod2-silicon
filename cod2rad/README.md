Source-only cod2rad64_our bundle.

Contents:
  src\                 C/C++/ASM source only
  build_source.bat     relative clean-build script

No EXE, DLL, OBJ, PDB, logs, map files, or research artifacts are included.

Build requirements:
  Visual Studio x64 C/C++ build tools
  Embree 4 x64 Windows SDK

Before building, set EMBREEDIR to the Embree SDK folder, for example:

  set EMBREEDIR=C:\path\to\embree-4.4.1.x64.windows
  build_source.bat

The built EXE will be written to:

  bin\cod2rad64_our.exe

At runtime, place these Embree DLLs next to the EXE:

  embree4.dll
  tbb12.dll
  tbbmalloc.dll
