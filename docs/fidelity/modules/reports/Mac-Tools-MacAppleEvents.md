# Mac/Tools/MacAppleEvents reconciliation

Updated 2026-11-05.

**15/15 reconciled: 15 intentional.** The old ProcessSerialNumber/Carbon object-property transport has no external source caller or reference from the compiled native generated-data files. This conclusion uses the selected call graph, not the module name or the presence of stubs.

Read both `GetData` overloads at 0x2640/0x2736, including type coercion, Pascal length limit 255 and signed status handling. Read event/target destruction, null initialization, all C1/C2 target and object-specifier constructors, and `AESend` at 0x2cba. The property helpers at 0x2d72/0x2ef4/0x30c8 build core `setd`/`cnte`/`getd` requests with object/type/data parameters and reply extraction. Their dormant native stubs or missing overloads are explicitly not equality claims, and the legacy exception/runtime wrappers are not approved for live use.

Native engine startup and launcher lifecycle no longer create the old Carbon engine or target these objects. The separately reviewed pending Unix-runtime commit `b8facc48110ee340ba1d1311818604de7934fd55` opens the required application/URL through SDK CoreFoundation/LaunchServices rather than reviving this object scripting layer. That dependency was read, not cherry-picked or modified in this unit. General AppleEvent property automation is not an active native feature.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
