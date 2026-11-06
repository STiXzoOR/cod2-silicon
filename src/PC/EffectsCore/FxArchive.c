#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <string.h>

#if defined(COD2_X64)
_Static_assert(sizeof(FxArchive) == 32, "FX native archive storage");
_Static_assert(sizeof(FxChannelInstance) == 24, "FX native channel instance storage");
_Static_assert(sizeof(FxBoltInfo) == 8, "FX bolt wire storage");
#endif

extern void MemFile_ReadData(MemoryFile *memFile, int bytes, void *data);
extern void MemFile_WriteData(MemoryFile *memFile, int bytes, const void *data);
extern EffectTemplate * FX_RegisterEffect(const char *fileName);
extern MaterialHandle FX_RegisterMaterial(const char *material);
extern struct XModel *FX_ModelRegister(const char *name);
extern const char *XModelGetName(const XModel *model);
extern const char *FxHelper_GetMaterialName(const FxHelper *_this, MaterialHandle handle);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern void *imp_theFxHelper;

void FxArchive_FxArchive(const FxArchive *_this);
void FxArchive_BeginReading(const FxArchive *_this, MemoryFile *memFile);
void FxArchive_ReadData(const FxArchive *_this, void *p, int byteCount);
void FxArchive_WriteData(const FxArchive *_this, const void *p, int byteCount);
void FxArchive_ArchiveEffect(const FxArchive *_this, const EffectTemplate **fx);
void FxArchive_ArchiveMaterial(const FxArchive *_this, MaterialHandle *ph);
void FxArchive_ArchiveModel(const FxArchive *_this, struct XModel **model);
void FxArchive_ArchiveChannelInstance(const FxArchive *_this, FxChannelInstance *channelInstance);
void FxArchive_ArchiveFxBoltInfo(const FxArchive *_this, FxBoltInfo *bolt);
void FxArchive_ArchiveFxGfxEntity(const FxArchive *_this, FxGfxEntity *entity);

void FxArchive_FxArchive(const FxArchive *_this)
{
    ((FxArchive *)_this)->memFile = 0;
    ((FxArchive *)_this)->isReading = 0;
    ((FxArchive *)_this)->unusedFlag = 0;
    ((FxArchive *)_this)->byteCounter = 0;
    ((FxArchive *)_this)->startPos = 0;
    ((FxArchive *)_this)->repeatCount = 0;
    ((FxArchive *)_this)->zeroCount = 0;
}

void FxArchive_BeginReading(const FxArchive *_this, MemoryFile *memFile)
{
    ((FxArchive *)_this)->memFile = memFile;
    ((FxArchive *)_this)->isReading = 1;
    ((FxArchive *)_this)->unusedFlag = 0;
    ((FxArchive *)_this)->byteCounter = 0;
    ((FxArchive *)_this)->startPos = memFile->bytesUsed;
    ((FxArchive *)_this)->repeatCount = 0;
    ((FxArchive *)_this)->zeroCount = 0;
}

#if defined(COD2_X64)
/* Fatal MemoryFile readers leave through Com_Error. A nonfatal reader leaves
 * memoryOverflow set and supplies zeroes for the unread remainder. */
static inline __attribute__((always_inline)) void FxArchive_ReadData_core(const FxArchive *_this, void *p, int byteCount)
{
    FxArchive *a = (FxArchive *)_this;
    byte *data = p;
    if (byteCount <= 0)
        return;
    a->byteCounter += byteCount;
    while (byteCount > 0) {
        unsigned char value = 0;
        if (a->repeatCount != 0) {
            --a->repeatCount;
            MemFile_ReadData(a->memFile, 1, &value);
            if (a->memFile->memoryOverflow) {
                memset(data, 0, byteCount);
                return;
            }
            *data++ = value;
            --byteCount;
        } else if (a->zeroCount != 0) {
            --a->zeroCount;
            *data++ = 0;
            --byteCount;
        } else {
            MemFile_ReadData(a->memFile, 1, &value);
            if (a->memFile->memoryOverflow) {
                memset(data, 0, byteCount);
                return;
            }
            switch (value & 0xc0) {
            case 0x00: a->repeatCount = 1; break;
            case 0x40: a->repeatCount = 2; break;
            case 0x80: a->repeatCount = 4; break;
            default:
                a->repeatCount = (value & 0x3f) + 1;
                a->zeroCount = 0;
                continue;
            }
            a->zeroCount = (value & 0x3f) + 1;
        }
    }
}
#else
static inline __attribute__((always_inline)) void FxArchive_ReadData_core(const FxArchive *_this, void *p, int byteCount)
{
    FxArchive *a = (FxArchive *)_this;
    byte *data;
    unsigned char value;
    int offset;
    int newRemaining;
    int bits;

    a->byteCounter += byteCount;
    data = (byte *)p;

    for (;;) {
        if (a->repeatCount != 0) {

            a->repeatCount -= 1;
            newRemaining = byteCount - 1;
            MemFile_ReadData(a->memFile, 1, &value);
            *data++ = value;
            if (newRemaining == 0)
                return;
            byteCount = newRemaining;
            continue;
        }

        offset = 0;
        for (;;) {
            newRemaining = byteCount - offset;
            if (a->zeroCount == 0)
                break;
            if (byteCount == offset)
                break;
            a->zeroCount -= 1;
            *data++ = 0;
            offset++;
            if (byteCount == offset)
                return;
        }

        MemFile_ReadData(a->memFile, 1, &value);
        bits = value & 0xC0;
        if (bits == 0x40) {
            a->repeatCount = 2;
            a->zeroCount = (value & 0x3f) + 1;
        } else if (bits == 0x80) {
            a->repeatCount = 4;
            a->zeroCount = (value & 0x3f) + 1;
        } else if (bits == 0) {
            a->repeatCount = 1;
            a->zeroCount = (value & 0x3f) + 1;
        } else {
            a->repeatCount = (value & 0x3f) + 1;
            a->zeroCount = 0;
        }
        byteCount = newRemaining;
    }
}
#endif

void FxArchive_ReadData(const FxArchive *_this, void *p, int byteCount)
{
    FxArchive_ReadData_core(_this, p, byteCount);
}

#if defined(COD2_X64)
/* Mac 0xa9f0e: literal runs have a 0xc0 control; the other controls
 * encode one, two or four literals followed by up to 64 zeroes. */
void FxArchive_WriteData(const FxArchive *_this, const void *p, int byteCount)
{
    FxArchive *a = (FxArchive *)_this;
    MemoryFile *mf = a->memFile;
    const byte *data = p;
    byte control;
    a->byteCounter += byteCount;
    if (byteCount <= 0 || mf->memoryOverflow)
        return;
    if (a->startPos == mf->bytesUsed) {
        control = 0xc0;
        MemFile_WriteData(mf, 1, &control);
        MemFile_WriteData(mf, 1, data++);
        --byteCount;
    }
    for (int i = 0; i < byteCount && !mf->memoryOverflow; ++i) {
        byte *run = mf->buffer + a->startPos;
        int count = *run & 0x3f;
        if ((*run & 0xc0) != 0xc0) {
            if (data[i] == 0 && count != 0x3f) {
                ++*run;
                continue;
            }
        } else if (count == 0 || count == 1 || count == 3) {
            if (data[i] == 0) {
                *run = count == 0 ? 0 : count == 1 ? 0x40 : 0x80;
                continue;
            }
            ++*run;
            MemFile_WriteData(mf, 1, data + i);
            continue;
        } else if (count != 0x3f) {
            byte *end = mf->buffer + mf->bytesUsed;
            if (count > 5 && data[i] == 0 && end[-2] == 0 && end[-1] == 0) {
                *run -= 3;
                end[-2] = end[-3];
                end[-3] = 2;
                a->startPos = mf->bytesUsed - 3;
                --mf->bytesUsed;
                continue;
            }
            ++*run;
            MemFile_WriteData(mf, 1, data + i);
            continue;
        }
        a->startPos = mf->bytesUsed;
        control = 0xc0;
        MemFile_WriteData(mf, 1, &control);
        MemFile_WriteData(mf, 1, data + i);
    }
}
#else
void FxArchive_WriteData(const FxArchive *_this, const void *p, int byteCount)
{
    MemoryFile *memFile;
    const byte *data;
    int i;

    ((FxArchive *)_this)->byteCounter += byteCount;
    data = (const byte *)p;
    memFile = ((FxArchive *)_this)->memFile;

    if (((FxArchive *)_this)->startPos == memFile->bytesUsed) {

    }

    if (byteCount <= 0)
        return;

    for (i = 0; i < byteCount; i++) {

        byte *buf = memFile->buffer;
        int pos = ((FxArchive *)_this)->startPos;
        unsigned char ctrl = buf[pos];
        int bits = ctrl & 0xC0;

        if (bits == 0x00 || bits == 0x40 || bits == 0x80) {
            MemFile_WriteData(memFile, 1, &data[i]);
        } else {
            MemFile_WriteData(memFile, 1, &data[i]);
        }
    }
}
#endif

void FxArchive_ArchiveEffect(const FxArchive *_this, const EffectTemplate **fx)
{
    char isReading;
    unsigned char ch;
    const char *effectName;
    char chEffectName[64];
    char filename[64];
    int len;

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, &ch, 1);
        len = (unsigned char)(ch - 1);
        if ((unsigned char)len > 0x3e) {

            *fx = (const EffectTemplate *)0;
            return;
        }

        len = (unsigned char)ch;
        FxArchive_ReadData_core(_this, chEffectName, len);
        chEffectName[len] = '\0';
        Com_sprintf(filename, 0x40, "fx/%s", chEffectName);
        *fx = (const EffectTemplate *)FX_RegisterEffect(filename);
    } else {

        if (*fx != (void *)0) {
            effectName = *(const char **)(*fx);
        } else {
            effectName = "";
        }

        len = strlen(effectName);
        ch = (unsigned char)len;
        FxArchive_WriteData(_this, &ch, 1);
        if (len != 0) {
            FxArchive_WriteData(_this, effectName, len);
        }
    }
}

void FxArchive_ArchiveMaterial(const FxArchive *_this, MaterialHandle *ph)
{
    char isReading;
    unsigned char ch;
    const char *materialName;
    char chMaterialName[64];
    int len;

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, &ch, 1);
        len = (unsigned char)(ch - 1);
        if ((unsigned char)len > 0x3e) {

            *ph = (MaterialHandle)0;
            return;
        }

        len = (unsigned char)ch;
        FxArchive_ReadData_core(_this, chMaterialName, len);
        chMaterialName[len] = '\0';
        *ph = FX_RegisterMaterial(chMaterialName);
    } else {

        if (*(MaterialHandle *)ph != (MaterialHandle)0) {
            void *fxHelper = *(void **)imp_theFxHelper;
            materialName = FxHelper_GetMaterialName((const FxHelper *)fxHelper, *ph);
        } else {
            materialName = "";
        }

        len = strlen(materialName);
        ch = (unsigned char)len;
        FxArchive_WriteData(_this, &ch, 1);
        if (len != 0) {
            FxArchive_WriteData(_this, materialName, len);
        }
    }
}

void FxArchive_ArchiveModel(const FxArchive *_this, struct XModel **model)
{
    char isReading;
    unsigned char ch;
    const char *pszModelName;
    char chModelName[64];
    int len;

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, &ch, 1);
        len = (unsigned char)(ch - 1);
        if ((unsigned char)len > 0x3e) {

            *model = (struct XModel *)0;
            return;
        }

        len = (unsigned char)ch;
        FxArchive_ReadData_core(_this, chModelName, len);
        chModelName[len] = '\0';
        *model = FX_ModelRegister(chModelName);
    } else {

        if (*model != (void *)0) {
            pszModelName = XModelGetName(*model);
        } else {
            pszModelName = "";
        }

        len = strlen(pszModelName);
        ch = (unsigned char)len;
        FxArchive_WriteData(_this, &ch, 1);
        if (len != 0) {
            FxArchive_WriteData(_this, pszModelName, len);
        }
    }
}

void FxArchive_ArchiveChannelInstance(const FxArchive *_this, FxChannelInstance *channelInstance)
{
    char isReading;
    int f;

    isReading = _this->isReading;

    if (isReading) {

        channelInstance->curveIterator.currentKeyIndex = 0;

        FxArchive_ReadData_core(_this, &f, 4);

        *(int *)&channelInstance->scale = f;

#if defined(COD2_X64)
        channelInstance->curveIterator.master = NULL;
#else
        *(int *)&channelInstance->curveIterator.master = 0;
#endif
    } else {

        f = *(int *)&channelInstance->scale;
        FxArchive_WriteData(_this, &f, 4);
    }
}

void FxArchive_ArchiveFxBoltInfo(const FxArchive *_this, FxBoltInfo *bolt)
{
    char isReading;
    FxBoltInfo temp;

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, (void *)bolt, 8);
    } else {

        temp = *bolt;
        FxArchive_WriteData(_this, &temp, 8);
    }
}

#if defined(COD2_X64)
void FxArchive_ArchiveFxGfxEntity(const FxArchive *_this, FxGfxEntity *entity)
{
    /* The material token is process-local and is replaced by ArchiveMaterial
     * later in Effect_Archive. Keep its retail 32-bit slot. */
    BM_ALIGNED(4) byte wire[0x68];
    uint32_t material;
    _Static_assert(offsetof(FxGfxEntity, endpos) - offsetof(FxGfxEntity, rotation) + sizeof(vec3_t) == 100,
                   "FxGfxEntity archive scalar span");
    if (_this->isReading) {
        FxArchive_ReadData_core(_this, wire, sizeof(wire));
        memcpy(&material, wire, sizeof(material));
        entity->customMaterial = (MaterialHandle)(uintptr_t)material;
        memcpy(&entity->rotation, wire + 4, 100);
    } else {
        material = (uint32_t)(uintptr_t)entity->customMaterial;
        memcpy(wire, &material, sizeof(material));
        memcpy(wire + 4, &entity->rotation, 100);
        FxArchive_WriteData(_this, wire, sizeof(wire));
    }
}
#else
void FxArchive_ArchiveFxGfxEntity(const FxArchive *_this, FxGfxEntity *entity)
{
    char isReading;
    BM_ALIGNED(4) byte temp[0x68];   /* leading: valid for both gcc and MSVC __declspec(align) */

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, (void *)entity, 0x68);
    } else {

        memcpy(temp, entity, sizeof(FxGfxEntity));
        FxArchive_WriteData(_this, temp, 0x68);
    }
}
#endif
