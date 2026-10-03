#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void *allocations[128];
static int allocationCount;
static GfxImage image;
static water_t water;
static MaterialTechniqueSet techniqueSet;
static dvar_t renderer;
static dvar_t *r_rendererInUse = &renderer;

static void *Material_Alloc(int size)
{
    void *p = calloc(1, size);
    assert(p && allocationCount < 128);
    allocations[allocationCount++] = p;
    return p;
}

static const char *Material_RegisterString(const char *name) { return name; }
static GfxImage *Image_Register(const char *name, int semantic, int track)
{
    assert(strcmp(name, "image") == 0 && semantic == 2 && track == 7);
    return &image;
}
static water_t *R_LoadWaterSetup(const water_t *setup)
{
    water = *setup;
    return &water;
}
static Bool Material_ResolveTechniqueSet(Material *material, const char *name, int track)
{
    assert(strcmp(name, "technique") == 0 && track == 7);
    material->techniqueSet = &techniqueSet;
    return 1;
}

#include "PC/gfx_d3d/r_material_disk.h"

int main(void)
{
    byte blob[384] = { 0 }, original[384];
    Material32 disk = { 0 };
    MaterialTextureDef32 textures[2] = { 0 };
    MaterialConstantDef32 constant = { 0 };
    MaterialWaterDef32 waterDef = { 64, 37.0f, 37.0f, 0.1f, 50.0f, { 1, 0 }, 0 };
    Material *material;
    int i;

    renderer.current.integer = 1;
    disk.info.name = 256;
    disk.techniqueSet = 265;
    disk.textureCount = 2;
    disk.constantCount = 1;
    disk.textures = 69; /* Deliberately unaligned disk records. */
    disk.constants = 95;
    textures[0].name = textures[1].name = 275;
    textures[0].semantic = 2;
    textures[0].image = 283;
    textures[1].semantic = 5;
    textures[1].image = 117;
    constant.name = 289;
    constant.literal[0] = 3.5f;
    memcpy(blob, &disk, sizeof(disk));
    memcpy(blob + 69, textures, sizeof(textures));
    memcpy(blob + 95, &constant, sizeof(constant));
    memcpy(blob + 117, &waterDef, sizeof(waterDef));
    strcpy((char *)blob + 256, "material");
    strcpy((char *)blob + 265, "technique");
    strcpy((char *)blob + 275, "texture");
    strcpy((char *)blob + 283, "image");
    strcpy((char *)blob + 289, "constant");
    memcpy(original, blob, sizeof(blob));

    material = Material_Marshal32To64(blob, sizeof(blob), 7);
    assert(material && (uintptr_t)material > UINT32_MAX);
    assert(material->textures[0].u.image == &image);
    assert(material->constants[0].literal[0] == 3.5f);
    assert(material->textures[1].u.water->map == &water);
    assert(water.M == 64 && water.N == 64 && water.windvel == 50);
    assert(water.amplitude == 0.1f && water.gravity == 800 && water.winddir[0] == 1);
    assert(memcmp(blob, original, sizeof(blob)) == 0);
    assert(!Material_Marshal32To64(blob, 67, 7));
    disk.textures = UINT32_MAX;
    memcpy(blob, &disk, sizeof(disk));
    assert(!Material_Marshal32To64(blob, sizeof(blob), 7));
    memcpy(blob, original, sizeof(blob));
    waterDef.textureWidth = 256;
    memcpy(blob + 117, &waterDef, sizeof(waterDef));
    assert(!Material_Marshal32To64(blob, sizeof(blob), 7));
    memcpy(blob, original, sizeof(blob));
    memset(blob + 256, 'x', sizeof(blob) - 256);
    assert(!Material_Marshal32To64(blob, sizeof(blob), 7));
    for (i = 0; i < allocationCount; ++i)
        free(allocations[i]);
    puts("renderer material disk offsets, native pointers, water and invalid bounds: passed");
    return 0;
}
