#ifndef COD2_MATERIAL_DISK_H
#define COD2_MATERIAL_DISK_H

#if defined(COD2_X64)
/* ===== 32-bit .material on-disk layout (kept binary-compatible) =====
 * The .material file is a 32-bit memory image: "pointers" are 4-byte
 * blob-relative offsets. On x64 we cannot relocate those in place (an 8-byte
 * pointer can't fit a 4-byte slot), so we MARSHAL the 32-bit image into an
 * x64-laid-out Material. The file bytes are never modified. */
typedef struct {
    unsigned int   name;            /* @0  blob offset */
    unsigned int   refImageName;    /* @4  blob offset */
    unsigned short hashIndex;       /* @8  */
    unsigned short sortedIndex;     /* @10 */
    unsigned char  gameFlags;       /* @12 */
    unsigned char  sortKey;         /* @13 */
    unsigned char  textureAtlasRowCount;    /* @14 */
    unsigned char  textureAtlasColumnCount; /* @15 */
    float          maxDeformMove;   /* @16 */
    unsigned char  deformFlags;     /* @20 */
    unsigned char  usage;           /* @21 */
    unsigned short toolFlags;       /* @22 */
    unsigned int   locale;          /* @24 */
    unsigned short autoTexScaleWidth;  /* @28 */
    unsigned short autoTexScaleHeight; /* @30 */
    float          tessSize;        /* @32 */
    int            surfaceFlags;    /* @36 */
    int            contents;        /* @40 */
} MaterialInfo32;   /* 44 bytes */

typedef struct {
    MaterialInfo32 info;            /* @0  */
    int            stateBits[2];    /* @44 */
    unsigned short textureCount;    /* @52 */
    unsigned short constantCount;   /* @54 */
    unsigned int   techniqueSet;    /* @56 blob offset (techset name) */
    unsigned int   textures;        /* @60 blob offset */
    unsigned int   constants;       /* @64 blob offset */
} Material32;   /* 68 bytes */

typedef struct {
    unsigned int   name;            /* @0 blob offset */
    unsigned char  samplerState;    /* @4 */
    unsigned char  semantic;        /* @5 */
    unsigned char  pad6, pad7;      /* @6,7 */
    unsigned int   image;           /* @8 blob offset */
} MaterialTextureDef32;   /* 12 bytes */

typedef struct {
    unsigned int   name;            /* @0 blob offset */
    float          literal[4];      /* @4 */
} MaterialConstantDef32;   /* 20 bytes */

typedef struct {
    int textureWidth;
    float horizontalWorldLength;
    float verticalWorldLength;
    float amplitude;
    float windSpeed;
    float windDirection[2];
    unsigned int map;
} MaterialWaterDef32;

typedef char Material32_size[(sizeof(Material32) == 68) ? 1 : -1];
typedef char MaterialTextureDef32_size[(sizeof(MaterialTextureDef32) == 12) ? 1 : -1];
typedef char MaterialConstantDef32_size[(sizeof(MaterialConstantDef32) == 20) ? 1 : -1];
typedef char MaterialWaterDef32_size[(sizeof(MaterialWaterDef32) == 32) ? 1 : -1];

static Bool Material_DiskRange(int size, unsigned int offset, size_t bytes)
{
    return size >= 0 && offset <= (unsigned int)size && bytes <= (size_t)size - offset;
}

static const char *Material_DiskString(const byte *blob, int size, unsigned int offset)
{
    if (!Material_DiskRange(size, offset, 1) || !memchr(blob + offset, 0, (size_t)size - offset))
        return NULL;
    return (const char *)blob + offset;
}

static Material *Material_Marshal32To64(const byte *blob, int blobSize, int imageTrack)
{
    Material32 source;
    const Material32 *src = &source;
    Material *m;
    const char *name, *refImageName, *techniqueName;
    int i;

    if (!Material_DiskRange(blobSize, 0, sizeof(*src)))
        return NULL;
    memcpy(&source, blob, sizeof(source));
    name = Material_DiskString(blob, blobSize, src->info.name);
    refImageName = src->info.refImageName ? Material_DiskString(blob, blobSize, src->info.refImageName) : NULL;
    techniqueName = Material_DiskString(blob, blobSize, src->techniqueSet);
    if (!name || (src->info.refImageName && !refImageName) || !techniqueName ||
        !Material_DiskRange(blobSize, src->textures, src->textureCount * sizeof(MaterialTextureDef32)) ||
        !Material_DiskRange(blobSize, src->constants, src->constantCount * sizeof(MaterialConstantDef32)))
        return NULL;
    m = (Material *)Material_Alloc((int)sizeof(Material));

    memset(m, 0, sizeof(*m));

    m->info.name                    = name;
    m->info.refImageName            = refImageName;
    m->info.hashIndex               = src->info.hashIndex;
    m->info.sortedIndex             = src->info.sortedIndex;
    m->info.gameFlags               = src->info.gameFlags;
    m->info.sortKey                 = src->info.sortKey;
    m->info.textureAtlasRowCount    = src->info.textureAtlasRowCount;
    m->info.textureAtlasColumnCount = src->info.textureAtlasColumnCount;
    m->info.maxDeformMove           = src->info.maxDeformMove;
    m->info.deformFlags             = src->info.deformFlags;
    m->info.usage                   = src->info.usage;
    m->info.toolFlags               = src->info.toolFlags;
    m->info.locale                  = src->info.locale;
    m->info.autoTexScaleWidth       = src->info.autoTexScaleWidth;
    m->info.autoTexScaleHeight      = src->info.autoTexScaleHeight;
    m->info.tessSize                = src->info.tessSize;
    m->info.surfaceFlags            = src->info.surfaceFlags;
    m->info.contents                = src->info.contents;

    m->stateBits[0]  = src->stateBits[0];
    m->stateBits[1]  = src->stateBits[1];
    m->textureCount  = src->textureCount;
    m->constantCount = src->constantCount;

    if (m->textureCount) {
        MaterialTextureDef32 texture;
        const MaterialTextureDef32 *st = &texture;
        MaterialTextureDef *dt = (MaterialTextureDef *)Material_Alloc((int)(m->textureCount * sizeof(MaterialTextureDef)));
        m->textures = dt;
        for (i = 0; i < m->textureCount; i++) {
            memcpy(&texture, blob + src->textures + i * sizeof(texture), sizeof(texture));
            unsigned char sem = st->semantic;
            const char *textureName = Material_DiskString(blob, blobSize, st->name);
            if (!textureName)
                return NULL;
            dt[i].name         = Material_RegisterString(textureName);
            dt[i].samplerState = st->samplerState;
            dt[i].semantic     = sem;
            dt[i].unused_0     = 0;
            dt[i].unused_1     = 0;
            dt[i].u.image      = (GfxImage *)0;
            if (sem == 5) {
                MaterialWaterDef32 water;
                const MaterialWaterDef32 *sw = &water;
                MaterialWaterDef *dw;
                water_t setup;
                if (!Material_DiskRange(blobSize, st->image, sizeof(*sw)))
                    return NULL;
                memcpy(&water, blob + st->image, sizeof(water));
                /* Square power-of-two FFTs must fit WaterGlob.H (16384 elements). */
                if (sw->textureWidth < 2 || sw->textureWidth > 128 ||
                    (sw->textureWidth & (sw->textureWidth - 1)))
                    return NULL;
                dw = (MaterialWaterDef *)Material_Alloc(sizeof(*dw));
                dw->textureWidth = sw->textureWidth;
                dw->horizontalWorldLength = sw->horizontalWorldLength;
                dw->verticalWorldLength = sw->verticalWorldLength;
                dw->amplitude = sw->amplitude;
                dw->windSpeed = sw->windSpeed;
                dw->windDirection[0] = sw->windDirection[0];
                dw->windDirection[1] = sw->windDirection[1];
                memset(&setup, 0, sizeof(setup));
                setup.M = setup.N = dw->textureWidth;
                setup.Lx = dw->horizontalWorldLength;
                setup.Lz = dw->verticalWorldLength;
                setup.gravity = 800.0f; /* Original loader's 0x44480000. */
                setup.windvel = dw->windSpeed;
                setup.winddir[0] = dw->windDirection[0];
                setup.winddir[1] = dw->windDirection[1];
                setup.amplitude = dw->amplitude;
                dw->map = R_LoadWaterSetup(&setup);
                if (!dw->map)
                    return NULL;
                dt[i].u.water = dw;
            } else {
                int isDx7 = (r_rendererInUse->current.integer == 2);
                if (isDx7 && (unsigned)(sem - 3) <= 1) {
                    dt[i].u.image = (GfxImage *)0;
                } else {
                    const char *imageName = Material_DiskString(blob, blobSize, st->image);
                    GfxImage *img;
                    if (!imageName)
                        return NULL;
                    img = Image_Register(imageName, sem, imageTrack);
                    dt[i].u.image = img;
                    if (!img)
                        return (Material *)0;
                }
            }
        }
    }

    if (m->constantCount) {
        MaterialConstantDef32 constant;
        const MaterialConstantDef32 *sc = &constant;
        MaterialConstantDef *dc = (MaterialConstantDef *)Material_Alloc((int)(m->constantCount * sizeof(MaterialConstantDef)));
        m->constants = dc;
        for (i = 0; i < m->constantCount; i++) {
            memcpy(&constant, blob + src->constants + i * sizeof(constant), sizeof(constant));
            const char *constantName = Material_DiskString(blob, blobSize, sc->name);
            if (!constantName)
                return NULL;
            dc[i].name = Material_RegisterString(constantName);
            if (!dc[i].name)
                return (Material *)0;
            dc[i].literal[0] = sc->literal[0];
            dc[i].literal[1] = sc->literal[1];
            dc[i].literal[2] = sc->literal[2];
            dc[i].literal[3] = sc->literal[3];
        }
    }

    {
        const char *tsName = techniqueName;
        if (!Material_ResolveTechniqueSet(m, tsName, imageTrack))
            return (Material *)0;
    }
    return m;
}
#endif /* COD2_X64 */

#endif
