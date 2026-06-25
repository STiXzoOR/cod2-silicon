#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <string.h>

extern void MemFile_ReadData(MemoryFile *memFile, int bytes, void *data);
extern void MemFile_WriteData(MemoryFile *memFile, int bytes, const void *data);
extern void *FX_RegisterEffect(const char *name);
extern MaterialHandle FX_RegisterMaterial(const char *material);
extern struct XModel *FX_ModelRegister(const char *name);
extern const char *XModelGetName(const void *model);
extern const char *FxHelper_GetMaterialName(const void *_this, MaterialHandle handle);
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

void FxArchive_ReadData(const FxArchive *_this, void *p, int byteCount)
{
    FxArchive_ReadData_core(_this, p, byteCount);
}

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
            materialName = FxHelper_GetMaterialName(fxHelper, *ph);
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

        *(int *)&channelInstance->curveIterator.master = 0;
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

void FxArchive_ArchiveFxGfxEntity(const FxArchive *_this, FxGfxEntity *entity)
{
    char isReading;
    byte temp[0x68] BM_ALIGNED(4);

    isReading = _this->isReading;

    if (isReading) {

        FxArchive_ReadData_core(_this, (void *)entity, 0x68);
    } else {

        memcpy(temp, entity, sizeof(FxGfxEntity));
        FxArchive_WriteData(_this, temp, 0x68);
    }
}
