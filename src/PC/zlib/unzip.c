/* ASM dump from: unzip.c */
/* Original path: PC/zlib/unzip.c */

#include "common_types.h"
#include "imports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    UNZ_OK = 0,
    UNZ_ERRNO = -1,
    UNZ_END_OF_LIST_OF_FILE = -100,
    UNZ_PARAMERROR = -102,
    UNZ_BADZIPFILE = -103,
    UNZ_INTERNALERROR = -104,
    UNZ_BUFSIZE = 0x4000,
    UNZ_LOCAL_HEADER_SIZE = 0x1e,
    UNZ_SIZECENTRALDIRITEM = 0x2e
};

enum {
    Z_DEFLATED = 8,
    Z_SYNC_FLUSH = 2
};

extern FILE *FS_FileOpen(const char *filename, const char *mode);
extern size_t FS_FileRead(void *ptr, size_t size, size_t nitems, FILE *stream);
extern int FS_FileClose(FILE *stream);
extern int FS_FileSeek(FILE *file, long int offset, int whence);
extern int inflate(z_streamp strm, int flush);
extern int inflateEnd(z_streamp strm);
extern int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size);

#ifdef COD2_FEATURE_MODERN_LIBS
/* Hardened .iwd reader: bind inflate to whatever system zlib is actually linked
 * (modern, security-maintained: e.g. 1.3.x has the CVE-2022-37434 fix). The
 * bundled z_stream layout in common_types.h is ABI-identical to system zlib's,
 * so we only need the runtime version string for inflateInit2_'s compat check
 * instead of the hardcoded "1.1.4" the bundled 1998-era headers assumed. */
extern const char *zlibVersion(void);
#define UNZ_ZLIB_VERSION (zlibVersion())
#else
#define UNZ_ZLIB_VERSION "1.1.4"
#endif

unzFile unzReOpen(const char *path, unzFile file);
int unzGetGlobalInfo(unzFile file, unz_global_info *pglobal_info);
int unzGetCurrentFileInfoPosition(unzFile file, long unsigned int *pos);
int unzReadCurrentFile(unzFile file, voidp buf, unsigned int len);
long int unztell(unzFile file);
int unzCloseCurrentFile(unzFile file);
static int __attribute_regparm__(3) unzlocal_GetCurrentFileInfoInternal(
    unzFile file,
    unz_file_info *pfile_info,
    unz_file_info_internal *pfile_info_internal,
    char *szFileName,
    uLong fileNameBufferSize,
    void *extraField,
    uLong extraFieldBufferSize,
    char *szComment,
    uLong commentBufferSize);
int unzSetCurrentFileInfoPosition(unzFile file, long unsigned int pos);
int unzGoToNextFile(unzFile file);
int unzGoToFirstFile(unzFile file);
int unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *szFileName, uLong fileNameBufferSize, double *extraField, uLong extraFieldBufferSize, char *szComment, uLong commentBufferSize);
unzFile unzOpen(const char *path);
int unzClose(unzFile file);
int unzOpenCurrentFile(unzFile file);

static int unzlocal_ReadShort(FILE *file, uLong *value)
{
    unsigned short raw;

    if (FS_FileRead(&raw, sizeof(raw), 1, file) != 1)
        return UNZ_BADZIPFILE;

    *value = raw;
    return UNZ_OK;
}

static int unzlocal_ReadLong(FILE *file, uLong *value)
{
#if defined(__x86_64__)
    /* The ZIP central-directory "long" is a fixed 32-bit field on disk. uLong is
     * `unsigned long` = 8 bytes under LP64, so reading sizeof(uLong) would
     * consume 8 bytes and desync the parse. Read exactly 4 bytes. */
    unsigned int raw;
#else
    uLong raw;
#endif

    if (FS_FileRead(&raw, sizeof(raw), 1, file) != 1)
        return UNZ_BADZIPFILE;

    *value = raw;
    return UNZ_OK;
}

static int unzlocal_CheckCurrentFileCoherencyHeader(
    unz_s *s,
    uInt *sizeVar,
    uLong *offsetLocalExtrafield,
    uInt *sizeLocalExtrafield)
{
    uLong value;
    uLong flags;
    uLong sizeFilename;
    uLong sizeExtraField;
    int err;

    *sizeVar = 0;
    *offsetLocalExtrafield = 0;
    *sizeLocalExtrafield = 0;

    if (FS_FileSeek(
            s->file,
            s->cur_file_info_internal.offset_curfile + s->byte_before_the_zipfile,
            SEEK_SET) != 0) {
        return UNZ_BADZIPFILE;
    }

    err = unzlocal_ReadLong(s->file, &value);
    if (err != UNZ_OK || value != 0x04034b50)
        return UNZ_BADZIPFILE;

    if (unzlocal_ReadShort(s->file, &value) != UNZ_OK)
        return UNZ_BADZIPFILE;

    if (unzlocal_ReadShort(s->file, &flags) != UNZ_OK)
        return UNZ_BADZIPFILE;

    err = unzlocal_ReadShort(s->file, &value);
    if (err != UNZ_OK || value != s->cur_file_info.compression_method)
        return UNZ_BADZIPFILE;

    if (value != 0 && value != Z_DEFLATED)
        return UNZ_BADZIPFILE;

    if (unzlocal_ReadLong(s->file, &value) != UNZ_OK)
        return UNZ_BADZIPFILE;

    err = unzlocal_ReadLong(s->file, &value);
    if (err != UNZ_OK)
        return UNZ_BADZIPFILE;
    if (value != s->cur_file_info.crc && (flags & 8) == 0)
        return UNZ_BADZIPFILE;

    err = unzlocal_ReadLong(s->file, &value);
    if (err != UNZ_OK)
        return UNZ_BADZIPFILE;
    if (value != s->cur_file_info.compressed_size && (flags & 8) == 0)
        return UNZ_BADZIPFILE;

    err = unzlocal_ReadLong(s->file, &value);
    if (err != UNZ_OK)
        return UNZ_BADZIPFILE;
    if (value != s->cur_file_info.uncompressed_size && (flags & 8) == 0)
        return UNZ_BADZIPFILE;

    err = unzlocal_ReadShort(s->file, &sizeFilename);
    if (err != UNZ_OK || sizeFilename != s->cur_file_info.size_filename)
        return UNZ_BADZIPFILE;

    *sizeVar += (uInt)sizeFilename;

    if (unzlocal_ReadShort(s->file, &sizeExtraField) != UNZ_OK)
        return UNZ_BADZIPFILE;

    *offsetLocalExtrafield = s->cur_file_info_internal.offset_curfile
        + UNZ_LOCAL_HEADER_SIZE
        + sizeFilename;
    *sizeLocalExtrafield = (uInt)sizeExtraField;
    *sizeVar += (uInt)sizeExtraField;
    return UNZ_OK;
}

static uLong unzlocal_SearchCentralDir(FILE *file)
{
    unsigned char *buffer;
    uLong fileSize;
    uLong backRead;
    uLong maxBack;
    uLong posFound;

    if (FS_FileSeek(file, 0, SEEK_END) != 0)
        return 0;

    fileSize = (uLong)ftell(file);
    maxBack = 0xffff;
    if (maxBack > fileSize)
        maxBack = fileSize;

    buffer = (unsigned char *)malloc(0x404);
    if (buffer == NULL)
        return 0;

    posFound = 0;
    backRead = 4;
    while (backRead < maxBack) {
        uLong readPos;
        uLong readSize;
        int i;

        if (backRead + 0x400 > maxBack)
            backRead = maxBack;
        else
            backRead += 0x400;

        readPos = fileSize - backRead;
        readSize = 0x404;
        if (readSize > fileSize - readPos)
            readSize = fileSize - readPos;

        if (FS_FileSeek(file, readPos, SEEK_SET) != 0)
            break;

        if (FS_FileRead(buffer, 1, readSize, file) != readSize)
            break;

        for (i = (int)readSize - 4; i >= 0; --i) {
            if (buffer[i] == 0x50 && buffer[i + 1] == 0x4b &&
                buffer[i + 2] == 0x05 && buffer[i + 3] == 0x06) {
                posFound = readPos + (uLong)i;
                break;
            }
        }

        if (posFound != 0)
            break;
    }

    free(buffer);
    return posFound;
}

/* line 326 */
unzFile unzReOpen(const char *path, unzFile file)
{
    FILE *fin;
    unz_s *reopened;

    fin = FS_FileOpen(path, "rb");
    if (fin == NULL)
        return NULL;

    reopened = (unz_s *)malloc(sizeof(*reopened));
    if (reopened == NULL) {
        FS_FileClose(fin);
        return NULL;
    }

    memcpy(reopened, (const unz_s *)file, sizeof(*reopened));
    reopened->file = fin;
    reopened->pfile_in_zip_read = NULL;
    return (unzFile)reopened;
}

/* line 472 */
int unzGetGlobalInfo(unzFile file, unz_global_info *pglobal_info)
{
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    *pglobal_info = s->gi;
    return UNZ_OK;
}

/* line 757 */
int unzGetCurrentFileInfoPosition(unzFile file, long unsigned int *pos)
{
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    *pos = s->pos_in_central_dir;
    return UNZ_OK;
}

/* line 1043 */
int unzReadCurrentFile(unzFile file, voidp buf, unsigned int len)
{
    int err;
    unsigned int readCount;
    unz_s *s;
    file_in_zip_read_info_s *readInfo;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    readInfo = s->pfile_in_zip_read;
    if (readInfo == NULL)
        return UNZ_PARAMERROR;

    if (readInfo->read_buffer == NULL)
        return UNZ_END_OF_LIST_OF_FILE;

    if (len == 0)
        return 0;

    readInfo->stream.next_out = (Bytef *)buf;
    readInfo->stream.avail_out = len;
    if (len > readInfo->rest_read_uncompressed)
        readInfo->stream.avail_out = readInfo->rest_read_uncompressed;

    err = UNZ_OK;
    readCount = 0;

    while (readInfo->stream.avail_out > 0) {
        if (readInfo->stream.avail_in == 0 && readInfo->rest_read_compressed > 0) {
            unsigned int readThis = UNZ_BUFSIZE;

            if (readInfo->rest_read_compressed < readThis)
                readThis = readInfo->rest_read_compressed;

            if (FS_FileSeek(
                    readInfo->file,
                    readInfo->pos_in_zipfile + readInfo->byte_before_the_zipfile,
                    SEEK_SET) != 0) {
                return UNZ_ERRNO;
            }

            if (FS_FileRead(readInfo->read_buffer, readThis, 1, readInfo->file) != 1)
                return UNZ_ERRNO;

            readInfo->pos_in_zipfile += readThis;
            readInfo->rest_read_compressed -= readThis;
            readInfo->stream.next_in = (Bytef *)readInfo->read_buffer;
            readInfo->stream.avail_in = readThis;
        }

        if (readInfo->compression_method == 0) {
            unsigned int copyCount = readInfo->stream.avail_out;

            if (copyCount > readInfo->stream.avail_in)
                copyCount = readInfo->stream.avail_in;

            if (copyCount > 0)
                memcpy(readInfo->stream.next_out, readInfo->stream.next_in, copyCount);

            readInfo->rest_read_uncompressed -= copyCount;
            readInfo->stream.avail_in -= copyCount;
            readInfo->stream.avail_out -= copyCount;
            readInfo->stream.next_out += copyCount;
            readInfo->stream.next_in += copyCount;
            readInfo->stream.total_out += copyCount;
            readCount += copyCount;
            continue;
        }

        {
            uLong totalOutBefore = readInfo->stream.total_out;

            err = inflate(&readInfo->stream, Z_SYNC_FLUSH);
            readInfo->rest_read_uncompressed -= readInfo->stream.total_out - totalOutBefore;
            readCount += readInfo->stream.total_out - totalOutBefore;

            if (err == 1)
                return readCount;
            if (err == UNZ_OK)
                continue;
            return err;
        }
    }

    return err == UNZ_OK ? (int)readCount : err;
}

/* line 1174 */
long int unztell(unzFile file)
{
    unz_s *s;
    file_in_zip_read_info_s *readInfo;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    readInfo = s->pfile_in_zip_read;
    if (readInfo == NULL)
        return UNZ_PARAMERROR;

    return readInfo->stream.total_out;
}

/* line 1274 */
int unzCloseCurrentFile(unzFile file)
{
    unz_s *s;
    file_in_zip_read_info_s *readInfo;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    readInfo = s->pfile_in_zip_read;
    if (readInfo == NULL)
        return UNZ_PARAMERROR;

    free(readInfo->read_buffer);
    readInfo->read_buffer = NULL;

    if (readInfo->stream_initialised)
        inflateEnd(&readInfo->stream);

    readInfo->stream_initialised = 0;
    free(readInfo);
    s->pfile_in_zip_read = NULL;
    return UNZ_OK;
}

/* line 522 — replaced naked+regparm(3) asm with clean C (naked unsupported on x86 gcc 4.0.4) */
static int __attribute_regparm__(3)
unzlocal_GetCurrentFileInfoInternal(unzFile file, unz_file_info *pfile_info, unz_file_info_internal *pfile_info_internal, char *szFileName, uLong fileNameBufferSize, void *extraField, uLong extraFieldBufferSize, char *szComment, uLong commentBufferSize)
{
    unz_s *s;
    unz_file_info file_info;
    unz_file_info_internal file_info_internal;
    int err = UNZ_OK;
    uLong uMagic;
    long lSeek = 0;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;

    if (FS_FileSeek(s->file, s->pos_in_central_dir + s->byte_before_the_zipfile, 0) != 0)
        err = UNZ_ERRNO;

    /* check the magic */
    if (err == UNZ_OK) {
        if (unzlocal_ReadLong(s->file, &uMagic) != UNZ_OK)
            err = UNZ_ERRNO;
        else if (uMagic != 0x02014b50)
            err = UNZ_BADZIPFILE;
    }

    if (unzlocal_ReadShort(s->file, &file_info.version) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.version_needed) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.flag) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.compression_method) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadLong(s->file, &file_info.dosDate) != UNZ_OK)
        err = UNZ_ERRNO;

    {
        uLong uDate = file_info.dosDate >> 16;
        file_info.tmu_date.tm_mday = uDate & 0x1f;
        file_info.tmu_date.tm_mon = ((uDate & 0x1e0) >> 5) - 1;
        file_info.tmu_date.tm_year = (uDate >> 9) + 1980;
        file_info.tmu_date.tm_hour = (file_info.dosDate & 0xf800) >> 11;
        file_info.tmu_date.tm_min = (file_info.dosDate & 0x7e0) >> 5;
        file_info.tmu_date.tm_sec = (file_info.dosDate & 0x1f) * 2;
    }

    if (unzlocal_ReadLong(s->file, &file_info.crc) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadLong(s->file, &file_info.compressed_size) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadLong(s->file, &file_info.uncompressed_size) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.size_filename) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.size_file_extra) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.size_file_comment) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.disk_num_start) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadShort(s->file, &file_info.internal_fa) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadLong(s->file, &file_info.external_fa) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzlocal_ReadLong(s->file, &file_info_internal.offset_curfile) != UNZ_OK)
        err = UNZ_ERRNO;

    lSeek += file_info.size_filename;

    if ((err == UNZ_OK) && (szFileName != NULL)) {
        uLong uSizeRead;
        if (file_info.size_filename < fileNameBufferSize) {
            *(szFileName + file_info.size_filename) = '\0';
            uSizeRead = file_info.size_filename;
        } else {
            uSizeRead = fileNameBufferSize;
        }

        if ((file_info.size_filename > 0) && (fileNameBufferSize > 0))
            if (FS_FileRead(szFileName, uSizeRead, 1, s->file) != 1)
                err = UNZ_ERRNO;
        lSeek -= uSizeRead;
    }

    if ((err == UNZ_OK) && (extraField != NULL)) {
        uLong uSizeRead;
        if (file_info.size_file_extra < extraFieldBufferSize)
            uSizeRead = file_info.size_file_extra;
        else
            uSizeRead = extraFieldBufferSize;

        if (lSeek != 0) {
            if (FS_FileSeek(s->file, lSeek, 1) == 0)
                lSeek = 0;
            else
                err = UNZ_ERRNO;
        }
        if ((file_info.size_file_extra > 0) && (extraFieldBufferSize > 0))
            if (FS_FileRead(extraField, uSizeRead, 1, s->file) != 1)
                err = UNZ_ERRNO;
        lSeek += file_info.size_file_extra - uSizeRead;
    } else {
        lSeek += file_info.size_file_extra;
    }

    if ((err == UNZ_OK) && (szComment != NULL)) {
        uLong uSizeRead;
        if (file_info.size_file_comment < commentBufferSize) {
            *(szComment + file_info.size_file_comment) = '\0';
            uSizeRead = file_info.size_file_comment;
        } else {
            uSizeRead = commentBufferSize;
        }

        if (lSeek != 0) {
            if (FS_FileSeek(s->file, lSeek, 1) == 0)
                lSeek = 0;
            else
                err = UNZ_ERRNO;
        }
        if ((file_info.size_file_comment > 0) && (commentBufferSize > 0))
            if (FS_FileRead(szComment, uSizeRead, 1, s->file) != 1)
                err = UNZ_ERRNO;
        lSeek += file_info.size_file_comment - uSizeRead;
    } else {
        lSeek += file_info.size_file_comment;
    }

    if ((err == UNZ_OK) && (pfile_info != NULL))
        *pfile_info = file_info;

    if ((err == UNZ_OK) && (pfile_info_internal != NULL))
        *pfile_info_internal = file_info_internal;

    return err;
}

/* line 773 */
int unzSetCurrentFileInfoPosition(unzFile file, long unsigned int pos)
{
    int err;
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    s->pos_in_central_dir = pos;
    err = unzlocal_GetCurrentFileInfoInternal(
        file,
        &s->cur_file_info,
        &s->cur_file_info_internal,
        NULL,
        0,
        NULL,
        0,
        NULL,
        0);
    s->current_file_ok = (err == UNZ_OK);
    return UNZ_OK;
}

/* line 729 */
int unzGoToNextFile(unzFile file)
{
    int err;
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;

    if (s->num_file + 1 == s->gi.number_entry)
        return UNZ_END_OF_LIST_OF_FILE;

    s->pos_in_central_dir += UNZ_SIZECENTRALDIRITEM
        + s->cur_file_info.size_filename
        + s->cur_file_info.size_file_extra
        + s->cur_file_info.size_file_comment;
    s->num_file++;

    err = unzlocal_GetCurrentFileInfoInternal(
        file,
        &s->cur_file_info,
        &s->cur_file_info_internal,
        NULL,
        0,
        NULL,
        0,
        NULL,
        0);
    s->current_file_ok = (err == UNZ_OK);
    return err;
}

/* line 706 */
int unzGoToFirstFile(unzFile file)
{
    int err;
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    s->pos_in_central_dir = s->offset_central_dir;
    s->num_file = 0;
    err = unzlocal_GetCurrentFileInfoInternal(
        file,
        &s->cur_file_info,
        &s->cur_file_info_internal,
        NULL,
        0,
        NULL,
        0,
        NULL,
        0);
    s->current_file_ok = (err == UNZ_OK);
    return err;
}

/* line 686 */
int unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *szFileName, uLong fileNameBufferSize, double *extraField, uLong extraFieldBufferSize, char *szComment, uLong commentBufferSize)
{
    return unzlocal_GetCurrentFileInfoInternal(
        file,
        pfile_info,
        NULL,
        szFileName,
        fileNameBufferSize,
        extraField,
        extraFieldBufferSize,
        szComment,
        commentBufferSize);
}

/* line 352 */
unzFile unzOpen(const char *path)
{
    int err;
    uLong centralPos;
    uLong ignored;
    uLong numberDisk;
    uLong numberDiskWithCd;
    uLong numberEntryCd;
    unz_s us;
    unz_s *s;

    memset(&us, 0, sizeof(us));
    us.file = FS_FileOpen(path, "rb");
    if (us.file == NULL)
        return NULL;

    err = UNZ_OK;
    centralPos = unzlocal_SearchCentralDir(us.file);
    if (centralPos == 0)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && FS_FileSeek(us.file, centralPos, SEEK_SET) != 0)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadLong(us.file, &ignored) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadShort(us.file, &numberDisk) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadShort(us.file, &numberDiskWithCd) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadShort(us.file, &us.gi.number_entry) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadShort(us.file, &numberEntryCd) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK &&
        (numberEntryCd != us.gi.number_entry || numberDiskWithCd != 0 || numberDisk != 0)) {
        err = UNZ_BADZIPFILE;
    }

    if (err == UNZ_OK && unzlocal_ReadLong(us.file, &us.size_central_dir) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadLong(us.file, &us.offset_central_dir) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && unzlocal_ReadShort(us.file, &us.gi.size_comment) != UNZ_OK)
        err = UNZ_ERRNO;

    if (err == UNZ_OK && centralPos < us.offset_central_dir + us.size_central_dir)
        err = UNZ_BADZIPFILE;

    if (err != UNZ_OK) {
        FS_FileClose(us.file);
        return NULL;
    }

    us.byte_before_the_zipfile = centralPos - (us.offset_central_dir + us.size_central_dir);
    us.central_pos = centralPos;
    us.pfile_in_zip_read = NULL;

    s = (unz_s *)malloc(sizeof(*s));
    if (s == NULL) {
        FS_FileClose(us.file);
        return NULL;
    }

    *s = us;
    unzGoToFirstFile((unzFile)s);
    return (unzFile)s;
}

/* line 451 */
int unzClose(unzFile file)
{
    unz_s *s;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    if (s->pfile_in_zip_read != NULL)
        unzCloseCurrentFile(file);

    FS_FileClose(s->file);
    free(s);
    return UNZ_OK;
}

/* line 941 */
int unzOpenCurrentFile(unzFile file)
{
    int err;
    uInt sizeVar;
    uLong offsetLocalExtrafield;
    uInt sizeLocalExtrafield;
    unz_s *s;
    file_in_zip_read_info_s *readInfo;

    if (file == NULL)
        return UNZ_PARAMERROR;

    s = (unz_s *)file;
    if (!s->current_file_ok)
        return UNZ_PARAMERROR;

    if (s->pfile_in_zip_read != NULL)
        unzCloseCurrentFile(file);

    err = unzlocal_CheckCurrentFileCoherencyHeader(
        s,
        &sizeVar,
        &offsetLocalExtrafield,
        &sizeLocalExtrafield);
    if (err != UNZ_OK)
        return err;

    readInfo = (file_in_zip_read_info_s *)malloc(sizeof(*readInfo));
    if (readInfo == NULL)
        return UNZ_INTERNALERROR;

    memset(readInfo, 0, sizeof(*readInfo));
    readInfo->read_buffer = (char *)malloc(UNZ_BUFSIZE);
    readInfo->offset_local_extrafield = offsetLocalExtrafield;
    readInfo->size_local_extrafield = sizeLocalExtrafield;

    if (readInfo->read_buffer == NULL) {
        free(readInfo);
        return UNZ_INTERNALERROR;
    }

    readInfo->compression_method = s->cur_file_info.compression_method;
    readInfo->file = s->file;
    readInfo->byte_before_the_zipfile = s->byte_before_the_zipfile;
    readInfo->stream.total_out = 0;

    if (readInfo->compression_method != 0) {
        if (inflateInit2_(&readInfo->stream, -15, UNZ_ZLIB_VERSION, sizeof(readInfo->stream)) == UNZ_OK)
            readInfo->stream_initialised = 1;
    }

    readInfo->rest_read_compressed = s->cur_file_info.compressed_size;
    readInfo->rest_read_uncompressed = s->cur_file_info.uncompressed_size;
    readInfo->pos_in_zipfile =
        s->cur_file_info_internal.offset_curfile + UNZ_LOCAL_HEADER_SIZE + sizeVar;
    readInfo->stream.avail_in = 0;

    s->pfile_in_zip_read = readInfo;
    return UNZ_OK;
}
