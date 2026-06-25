#include "common_types.h"

const char *Material_TechniqueNames[] = {
    "\"depth prepass\"",
    "\"sky\"",
    "\"build floatz\"",
    "\"unlit no fog\"",
    "\"unlit linear fog\"",
    "\"unlit exp fog\"",
    "\"lightmap no fog\"",
    "\"lightmap linear fog\"",
    "\"lightmap exp fog\"",
    "\"lightprobe no fog\"",
    "\"lightprobe linear fog\"",
    "\"lightprobe exp fog\"",
    "\"lightprobe smodel no fog\"",
    "\"lightprobe smodel linear fog\"",
    "\"lightprobe smodel exp fog\"",
    "\"lightprobe smodel cached no fog\"",
    "\"lightprobe smodel cached linear fog\"",
    "\"lightprobe smodel cached exp fog\"",
    "\"pointlight no fog\"",
    "\"pointlight linear fog\"",
    "\"pointlight exp fog\"",
    "\"emissive no fog\"",
    "\"emissive linear fog\"",
    "\"emissive exp fog\"",
    "\"emissive feathered no fog\"",
    "\"emissive feathered linear fog\"",
    "\"emissive feathered exp fog\"",
    "\"fakelight normal\"",
    "\"fakelight view\"",
    "\"solid wireframe\"",
    "\"shaded wireframe\"",
    "\"shadowcookie caster\"",
    "\"shadowcookie receiver\"",
    "\"debug bumpmap\""
};

extern const char *const s_techniqueTypeNames[34] __attribute__((alias("Material_TechniqueNames")));

char material_tech_string[1024] __asm__("string") = { 0 };
