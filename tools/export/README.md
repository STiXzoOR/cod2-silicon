# Exporting the engine source

`export.py` writes a copy of the engine source that has nothing of this project's
own app in it: no launcher, no packaging, no Metal renderer and none of its names.
The copy is for work that needs the bare engine, such as a plain reconstruction of
the game or a build for another platform. It is generated, so it cannot drift: change
the source here and export again. Never edit the copy.

```sh
tools/export/export.py /path/to/copy                    # the plain engine, no CoD2x layer
tools/export/export.py /path/to/copy --profile codx     # with the CoD2x layer
tools/export/export.py /path/to/copy --check            # exit 1 if the copy is out of date
tools/export/export.py /path/to/copy --prune            # also delete exported files that are gone here
```

The destination can also come from `COD2_EXPORT_DIR`. The tool writes only the files
it manages and lists them in `.exported-files` at the destination; anything else
there is left alone. If any exported line still names this project, the tool prints
those lines, writes nothing and exits with an error.

Before a release, run both profiles with `--check` against the two copies. A clean
check means the copies match the release.

## Keeping new code exportable

Most code needs nothing. The tool takes care of these cases:

| What you are adding | What to do |
| --- | --- |
| Engine code for every build | Nothing. |
| CoD2x code | Guard it with `COD2_CODX` as usual. A new symbol that is derived from it goes in `symbols.txt` under `[codx]`. |
| Code that only this project's app has (launcher hooks, its wording, its Metal paths) | `#if !defined(COD2_ENGINE_ONLY)`, with the plain form in `#else` if the engine needs one. Nothing defines the symbol; the export resolves it. |
| A file that only the app has | Add it to `[exclude]` in `paths.txt`. |
| A file of the CoD2x layer | Name it `*cod2x*`, or add it to `[codx only]` in `paths.txt`. |
| App or CoD2x lines in CMake, Python or shell | `# export-begin: app` (or `codx`) … `# export-end`. Lines after `# export-else`, written as `#\| …`, replace them in the copy. |
| This project's name in a message or a path | Add the neutral form to `rewrites.txt`. |

## Files

- `paths.txt`: which files are copied.
- `symbols.txt`: preprocessor symbols the export resolves.
- `rewrites.txt`: this project's names and their neutral forms.
- `forbidden.txt`: what the copy must not contain, and the few lines that are allowed.
- `conditionals.py`: removes the resolved `#if` blocks. It handles guards that run
  over several lines and rewrites a guard that is only partly resolved.

## Checking a change to the tool

Export both profiles to scratch folders and build them. Each copy should build the
Mac client and server (`cmake -DCOD2_X64=ON`, with `-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1`
for the CoD2x copy), the Linux server and the Windows server and client. In the plain
copy, test harnesses that offer a CoD2x option still name it; those variants do not
run there.
