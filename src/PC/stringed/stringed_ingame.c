#include "common_types.h"
#include "imports.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define STRINGED_NOINLINE __declspec(noinline)
#else
#define STRINGED_NOINLINE __attribute__((noinline))
#endif

#if defined(COD2_X64) && defined(__APPLE__)
#include "../../platform/macos_cpp_abi.h"

extern CStringEdPackage *TheStringPackage;
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern unsigned char *SE_LoadFileData(const char *name);
extern void SE_FreeFileDataAfterLoad(unsigned char *data);
extern int SE_BuildFileList(const char *directory, Cod2MacString *result);
extern const char *va(const char *format, ...);
extern int stricmp(const char *a, const char *b);
extern int strnicmp(const char *a, const char *b, unsigned long length);
extern char *strupr(char *text);

/* The package uses zone allocation; GCC tree nodes use operator new/delete.
 * These are native pointer-sized GCC ABI objects, never libc++ containers. */
typedef struct {
    Cod2MacTreeNode node;
    Cod2MacString key;
    Cod2MacString value;
} NativeStringEdEntry;

typedef struct {
    qboolean foundEndMarker;
    Cod2MacString reference;
    Cod2MacString english;
    Cod2MacString package;
    size_t treeAllocator;
    Cod2MacTreeNode tree;
    size_t count;
} NativeStringEdPackage;

_Static_assert(sizeof(NativeStringEdEntry) == 48, "native StringEd tree node");
_Static_assert(sizeof(NativeStringEdPackage) == 80, "native StringEd package");

static NativeStringEdPackage *SE_Package(const CStringEdPackage *package)
{
    return (NativeStringEdPackage *)(void *)package;
}

static void SE_Assign(Cod2MacString *string, const char *text)
{
    __ZNSs6assignEPKcm(string, text, strlen(text));
}

static NativeStringEdEntry *SE_FindEntry(NativeStringEdPackage *package, const char *key)
{
    Cod2MacTreeNode *node = package->tree.parent;
    while (node) {
        NativeStringEdEntry *entry = (NativeStringEdEntry *)node;
        int comparison = strcmp(key, entry->key.data);
        if (!comparison)
            return entry;
        node = comparison < 0 ? node->left : node->right;
    }
    return NULL;
}

static NativeStringEdEntry *SE_AddEntry(NativeStringEdPackage *package, const char *key)
{
    Cod2MacTreeNode *parent = &package->tree;
    Cod2MacTreeNode *node = parent->parent;
    NativeStringEdEntry *entry;
    int insertLeft = 1;
    while (node) {
        int comparison = strcmp(key, ((NativeStringEdEntry *)node)->key.data);
        if (!comparison)
            return (NativeStringEdEntry *)node;
        parent = node;
        insertLeft = comparison < 0;
        node = insertLeft ? node->left : node->right;
    }
    entry = __Znwm(sizeof(*entry));
    __ZNSsC1EPKcRKSaIcE(&entry->key, key, NULL);
    __ZNSsC1EPKcRKSaIcE(&entry->value, "", NULL);
    __ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(
        insertLeft, &entry->node, parent, &package->tree);
    ++package->count;
    return entry;
}

static void SE_EraseEntries(Cod2MacTreeNode *node)
{
    while (node) {
        NativeStringEdEntry *entry = (NativeStringEdEntry *)node;
        Cod2MacTreeNode *left = node->left;
        SE_EraseEntries(node->right);
        __ZNSsD1Ev(&entry->value);
        __ZNSsD1Ev(&entry->key);
        __ZdlPv(entry);
        node = left;
    }
}

qboolean CStringEdPackage_ReadLine(const CStringEdPackage *_this, const char **position, char *dest)
{
    const char *p = *position;
    const char *newline;
    char *scan;
    int quotes = 0;
    size_t length;
    (void)_this;
    if (!*p)
        return 0;
    newline = strchr(p, '\n');
    if (newline) {
        length = (size_t)(newline - p);
        strncpy(dest, p, length);
        dest[length] = '\0';
        *position += length;
        while (**position && strchr("\r\n", **position))
            ++*position;
    } else {
        strcpy(dest, p);
        *position += strlen(p);
    }
    length = strlen(dest);
    while (length && isspace((unsigned char)dest[length - 1]))
        dest[--length] = '\0';
    scan = dest;
    for (;;) {
        char *comment = strstr(scan, "//");
        char *quote;
        if (!comment)
            return 1;
        for (quote = scan; quote < comment; ++quote) {
            if (*quote == '"')
                ++quotes;
        }
        if (quotes & 1) {
            scan = comment + 1;
            continue;
        }
        *comment = '\0';
        length = strlen(scan);
        while (length && isspace((unsigned char)scan[length - 1]))
            scan[--length] = '\0';
        return 1;
    }
}

void CStringEdPackage_SetupNewFileParse(const CStringEdPackage *_this, const char *filename)
{
    const char *base = filename;
    const char *p;
    const char *dot = strrchr(filename, '.');
    Cod2MacString name;
    size_t length;
    for (p = filename; *p; ++p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    length = dot && dot >= base ? (size_t)(dot - base) : strlen(base);
    __ZNSsC1EPKcRKSaIcE(&name, "", NULL);
    __ZNSs6assignEPKcm(&name, base, length);
    __ZNSs12_M_leak_hardEv(&name);
    strupr(name.data);
    __ZNSs6assignERKSs(&SE_Package(_this)->package, &name);
    __ZNSsD1Ev(&name);
}

Cod2MacString CStringEdPackage_InsideQuotes(const CStringEdPackage *_this, const char *line)
{
    Cod2MacString result;
    size_t length;
    (void)_this;
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '"')
        ++line;
    length = strlen(line);
    /* The original reads before its string when trimming an all-space value.
     * Keep native empty values defined instead of recreating that underread. */
    while (length && (line[length - 1] == ' ' || line[length - 1] == '\t'))
        --length;
    if (length && line[length - 1] == '"')
        --length;
    __ZNSsC1EPKcRKSaIcE(&result, "", NULL);
    __ZNSs6assignEPKcm(&result, line, length);
    return result;
}

Cod2MacString CStringEdPackage_ConvertCRLiterals_Read(const CStringEdPackage *_this, Cod2MacString *input)
{
    Cod2MacString result;
    size_t position;
    (void)_this;
    __ZNSsC1ERKSs(&result, input);
    while ((position = __ZNKSs4findEPKcmm(&result, "\\n", 0, 2)) != (size_t)-1)
        __ZNSs7replaceEmmPKcm(&result, position, 2, "\n", 1);
    return result;
}

void CStringEdPackage_AddEntry(const CStringEdPackage *_this, const char *reference)
{
    NativeStringEdPackage *package = SE_Package(_this);
    const char *key = va("%s_%s", package->package.data, reference);
    SE_AddEntry(package, key);
    SE_Assign(&package->reference, reference);
}

void CStringEdPackage_SetString(const CStringEdPackage *_this, const char *reference, const char *text, qboolean english)
{
    NativeStringEdPackage *package = SE_Package(_this);
    NativeStringEdEntry *entry = SE_FindEntry(package, va("%s_%s", package->package.data, reference));
    /* Engine calls add a reference before setting it. Keep the native behavior
     * defined for an otherwise missing entry rather than writing through end(). */
    if (!entry) {
        entry = SE_AddEntry(package, va("%s_%s", package->package.data, reference));
    }
    if (english) {
        SE_Assign(&entry->value, text);
        SE_Assign(&package->english, text);
    } else if (!stricmp(text, "#same")) {
        __ZNSs6assignERKSs(&entry->value, &package->english);
    } else {
        SE_Assign(&entry->value, text);
    }
}

static const char *SE_AfterKeyword(const char *line, size_t length)
{
    line += length;
    while (*line == ' ' || *line == '\t')
        ++line;
    return line;
}

static const char *SE_FormatError(const char *text)
{
    unsigned char used[9] = {0};
    const char *scan = strstr(text, "&&");
    while (scan) {
        unsigned int index = (unsigned char)scan[2] - '1';
        /* Mac accepts digit zero before indexing one byte before used[].
         * Preserve native's deterministic rejection of that invalid format. */
        if (index >= 9 || used[index])
            return va("Illegal string format \"%s\"\n", text);
        used[index] = 1;
        scan = strstr(scan + 3, "&&");
    }
    return NULL;
}

const char *CStringEdPackage_ParseLine(const CStringEdPackage *_this, const char *line, int forceEnglish)
{
    NativeStringEdPackage *package = SE_Package(_this);
    Cod2MacString text, sentence;
    const char *error;
    const char *language;
    size_t length;
    char token[1024] = {0};
    int english;
    if (!line)
        return NULL;
    if (!strnicmp("VERSION", line, 7)) {
        int version;
        text = CStringEdPackage_InsideQuotes(_this, SE_AfterKeyword(line, 7));
        version = atoi(text.data);
        error = version == 1 ? NULL : va("Unexpected version number %d, expecting %d!\n", version, 1);
        __ZNSsD1Ev(&text);
        return error;
    }
    if (!strnicmp("CONFIG", line, 6) || !strnicmp("FILENOTES", line, 9) ||
        !strnicmp("NOTES", line, 5) || !strnicmp("FLAGS", line, 5))
        return NULL;
    if (!strnicmp("REFERENCE", line, 9)) {
        text = CStringEdPackage_InsideQuotes(_this, SE_AfterKeyword(line, 9));
        CStringEdPackage_AddEntry(_this, text.data);
        __ZNSsD1Ev(&text);
        return NULL;
    }
    if (!strnicmp("ENDMARKER", line, 9)) {
        package->foundEndMarker = 1;
        return NULL;
    }
    if (strnicmp("LANG_", line, 5))
        return va("Unknown keyword at linestart: \"%s\"\n", line);
    if (!package->reference.data[0])
        return "Error parsing file: Unexpected \"LANG_\"\n";
    language = line + 5;
    for (length = 0; language[length] && language[length] != ' ' && language[length] != '\t'; ++length)
        ;
    if (length > sizeof(token) - 1)
        length = sizeof(token) - 1;
    strncpy(token, language, length);
    text = CStringEdPackage_InsideQuotes(_this, language + strlen(token));
    sentence = CStringEdPackage_ConvertCRLiterals_Read(_this, &text);
    __ZNSsD1Ev(&text);
    error = SE_FormatError(sentence.data);
    english = !stricmp(token, "english");
    if (!error && (english || !(unsigned char)forceEnglish))
        CStringEdPackage_SetString(_this, package->reference.data, sentence.data, english);
    __ZNSsD1Ev(&sentence);
    return error;
}

void CStringEdPackage_Clear(const CStringEdPackage *_this)
{
    NativeStringEdPackage *package = SE_Package(_this);
    SE_EraseEntries(package->tree.parent);
    package->tree.parent = NULL;
    package->tree.left = &package->tree;
    package->tree.right = &package->tree;
    package->count = 0;
    package->foundEndMarker = 0;
    SE_Assign(&package->reference, "");
    SE_Assign(&package->english, "");
}

const char *SE_Load(const char *filename, int forceEnglish)
{
    unsigned char *data = SE_LoadFileData(filename);
    const char *position = (const char *)data;
    const char *error = NULL;
    char line[0x4000];
    if (!data)
        return va("Unable to load \"%s\"!", filename);
    CStringEdPackage_SetupNewFileParse(TheStringPackage, filename);
    while (CStringEdPackage_ReadLine(TheStringPackage, &position, line)) {
        if (line[0]) {
            error = CStringEdPackage_ParseLine(TheStringPackage, line, (unsigned char)forceEnglish);
            if (error)
                break;
        }
    }
    SE_FreeFileDataAfterLoad(data);
    if (!error && !SE_Package(TheStringPackage)->foundEndMarker)
        error = va("Truncated file, failed to find \"%s\" at file end!", "ENDMARKER");
    return error;
}

STRINGED_NOINLINE const char *SE_GetString(const char *reference)
{
    NativeStringEdEntry *entry;
    if (!TheStringPackage)
        return NULL;
    entry = SE_FindEntry(SE_Package(TheStringPackage), reference);
    return entry ? entry->value.data : NULL;
}

static const char *SE_GetFoundFile(Cod2MacString *files)
{
    static char filename[64];
    char *delimiter;
    size_t removed;
    if (!files->data[0])
        return NULL;
    strncpy(filename, files->data, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    delimiter = strchr(filename, ';');
    if (delimiter) {
        *delimiter = '\0';
        removed = (size_t)(delimiter - filename) + 1;
    } else {
        removed = ((Cod2MacStringRep *)files->data - 1)->length;
    }
    __ZNSs9_M_mutateEmmm(files, 0, removed, 0);
    return filename;
}

void SE_Init(void)
{
    NativeStringEdPackage *package;
    if (TheStringPackage) {
        CStringEdPackage_Clear(TheStringPackage);
        return;
    }
    package = Z_MallocInternal(sizeof(*package));
    memset(package, 0, sizeof(*package));
    __ZNSsC1EPKcRKSaIcE(&package->reference, "", NULL);
    __ZNSsC1EPKcRKSaIcE(&package->english, "", NULL);
    __ZNSsC1EPKcRKSaIcE(&package->package, "", NULL);
    TheStringPackage = (CStringEdPackage *)(void *)package;
    CStringEdPackage_Clear(TheStringPackage);
}

const char *SE_LoadLanguage(int forceEnglish)
{
    Cod2MacString files;
    const char *filename;
    const char *error = NULL;
    if (!TheStringPackage)
        SE_Init();
    __ZNSsC1EPKcRKSaIcE(&files, "", NULL);
    CStringEdPackage_Clear(TheStringPackage);
    SE_BuildFileList("localizedstrings", &files);
    while ((filename = SE_GetFoundFile(&files)) != NULL) {
        if (error)
            break;
        error = SE_Load(filename, (unsigned char)forceEnglish);
    }
    __ZNSsD1Ev(&files);
    return error;
}

void SE_ShutDown(void)
{
    NativeStringEdPackage *package;
    if (!TheStringPackage)
        return;
    package = SE_Package(TheStringPackage);
    CStringEdPackage_Clear(TheStringPackage);
    __ZNSsD1Ev(&package->package);
    __ZNSsD1Ev(&package->english);
    __ZNSsD1Ev(&package->reference);
    Z_FreeInternal(package);
    TheStringPackage = NULL;
}
#else
extern CStringEdPackage *TheStringPackage;
static char sString[64];
static char sString_00482f80[64];
static char sTemp[64];

qboolean CStringEdPackage_ReadLine(const CStringEdPackage *_this, const char **psParsePos, char *psDest);
void CStringEdPackage_SetupNewFileParse(const CStringEdPackage *_this, const char *psFileName);
static const char *SE_GetFoundFile(LocalizeString *strResult);
LocalizeString CStringEdPackage_ConvertCRLiterals_Read(const CStringEdPackage *_this, LocalizeString *string);
LocalizeString CStringEdPackage_InsideQuotes(const CStringEdPackage *_this, const char *psLine);
void CStringEdPackage_AddEntry(const CStringEdPackage *_this, const char *psLocalReference);
void CStringEdPackage_SetString(const CStringEdPackage *_this, const char *psLocalReference, const char *psNewString, qboolean bSentenceIsEnglish);
const char *CStringEdPackage_ParseLine(const CStringEdPackage *_this, const char *psLine, int forceEnglish);
const char *SE_Load(const char *psFileName, int forceEnglish);
STRINGED_NOINLINE const char *SE_GetString(const char *psPackageAndStringReference);
void CStringEdPackage_Clear(const CStringEdPackage *_this);
const char *SE_LoadLanguage(int forceEnglish);
void SE_Init(void);
void SE_ShutDown(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE11lower_boundERS1_(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE4findERS1_(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE9_M_insertEPSt18_Rb_tree_node_baseSA_RKS2_(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE13insert_uniqueERKS2_(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE13insert_uniqueESt17_Rb_tree_iteratorIS2_ERKS2_(void);
void ZNSt8_Rb_treeISsSt4pairIKSsSsESt10_Select1stIS2_ESt4lessISsESaIS2_EE8_M_eraseEPSt13_Rb_tree_nodeIS2_E(void);

qboolean CStringEdPackage_ReadLine(const CStringEdPackage *_this, const char **psParsePos, char *psDest)
{
    const char *p = *psParsePos;
    const char *nl;
    int len, i;

    if (!*p)
        return 0;

    nl = strchr(p, '\n');
    if (nl) {
        int charsToCopy = nl - p;
        strncpy(psDest, p, charsToCopy);
        psDest[charsToCopy] = '\0';
        *psParsePos = p + charsToCopy;

        while (**psParsePos && strchr((const char *)"\r\n", **psParsePos)) {
            (*psParsePos)++;
        }
    } else {
        strcpy(psDest, p);
        *psParsePos = p + strlen(p);
    }

    if (!psDest[0])
        return 1;

    len = strlen(psDest);
    for (i = len - 1; i >= 0; i--) {
        if (!isspace((unsigned char)psDest[i]))
            break;
        psDest[i] = '\0';
    }

    {
        char *s = psDest;
        for (;;) {
            char *comment = strstr(s, (const char *)"//");
            int quoteCount = 0;
            if (!comment)
                return 1;

            for (i = 0; i < (int)(comment - s); i++) {
                if (s[i] == '"')
                    quoteCount++;
            }

            if (quoteCount & 1) {

                s = comment + 1;
                continue;
            }

            *comment = '\0';
            if (!*s)
                return 1;

            len = strlen(s);
            for (i = len - 1; i >= 0; i--) {
                if (!isspace((unsigned char)s[i]))
                    break;
                s[i] = '\0';
            }
            return 1;
        }
    }
}

extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern const char **FS_ListFiles(const char *path, const char *extension, int behavior, int *numfiles, int allocTrackType);
extern void FS_FreeFileList(const char **list, int allocTrackType);
extern unsigned char *SE_LoadFileData(const char *psFileName);
extern void SE_FreeFileDataAfterLoad(unsigned char *psLoadedFile);
extern int SEH_GetCurrentLanguage(void);
extern const char *SEH_GetLanguageName(const int iLanguage);

typedef struct WebStringEdEntry {
    char *key;
    char *value;
    struct WebStringEdEntry *next;
} WebStringEdEntry;

static WebStringEdEntry *webStringEntries;
static char webStringPackageName[64];
static char webStringLocalReference[1024];
static char webStringEnglishValue[4096];
static char webStringError[1024];
static qboolean webStringFoundEndMarker;

static char *WebSE_CopyString(const char *text)
{
    char *copy;
    unsigned int len;

    if (!text) {
        text = "";
    }

    len = strlen(text) + 1;
    copy = (char *)Z_MallocInternal(len);
    memcpy(copy, text, len);
    return copy;
}

static void WebSE_FreeEntry(WebStringEdEntry *entry)
{
    if (entry->key) {
        Z_FreeInternal(entry->key);
    }
    if (entry->value) {
        Z_FreeInternal(entry->value);
    }
    Z_FreeInternal(entry);
}

static void WebSE_ClearEntries(void)
{
    WebStringEdEntry *entry;

    entry = webStringEntries;
    while (entry) {
        WebStringEdEntry *next = entry->next;
        WebSE_FreeEntry(entry);
        entry = next;
    }

    webStringEntries = NULL;
}

static int WebSE_Strnicmp(const char *a, const char *b, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        ca = (unsigned char)tolower(ca);
        cb = (unsigned char)tolower(cb);
        if (ca != cb || ca == '\0' || cb == '\0') {
            return (int)ca - (int)cb;
        }
    }

    return 0;
}

static int WebSE_Stricmp(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;

    do {
        ca = (unsigned char)tolower(*a++);
        cb = (unsigned char)tolower(*b++);
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
    } while (ca && cb);

    return 0;
}

static const char *WebSE_SkipWhitespace(const char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static const char *WebSE_Error1(const char *fmt, const char *arg)
{
    snprintf(webStringError, sizeof(webStringError), fmt, arg);
    return webStringError;
}

static const char *WebSE_VersionError(int got)
{
    snprintf(webStringError, sizeof(webStringError), "Unexpected version number %d, expecting %d!\n", got, 1);
    return webStringError;
}

static void WebSE_InsideQuotes(const char *line, char *out, unsigned int outSize)
{
    unsigned int len;

    line = WebSE_SkipWhitespace(line);
    if (*line == '"') {
        line++;
    }

    if (outSize == 0) {
        return;
    }

    strncpy(out, line, outSize - 1);
    out[outSize - 1] = '\0';

    len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t')) {
        out[--len] = '\0';
    }
    if (len > 0 && out[len - 1] == '"') {
        out[--len] = '\0';
    }
}

static void WebSE_ConvertCRLiterals(char *text)
{
    char *read;
    char *write;

    read = text;
    write = text;
    while (*read) {
        if (read[0] == '\\' && read[1] == 'n') {
            *write++ = '\n';
            read += 2;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static const char *WebSE_ValidateFormatArgs(const char *text)
{
    qboolean usedArgs[9];
    const char *scan;
    int i;

    for (i = 0; i < 9; i++) {
        usedArgs[i] = 0;
    }

    scan = strstr(text, "&&");
    while (scan) {
        int argIndex;

        if (scan[2] < '1' || scan[2] > '9') {
            return WebSE_Error1("Illegal string format \"%s\"\n", text);
        }

        argIndex = scan[2] - '1';
        if (usedArgs[argIndex]) {
            return WebSE_Error1("Illegal string format \"%s\"\n", text);
        }

        usedArgs[argIndex] = 1;
        scan = strstr(scan + 3, "&&");
    }

    return NULL;
}

static void WebSE_BuildFullReference(const char *localReference, char *out, unsigned int outSize)
{
    if (outSize == 0) {
        return;
    }

    snprintf(out, outSize, "%s_%s", webStringPackageName, localReference ? localReference : "");
    out[outSize - 1] = '\0';
}

static WebStringEdEntry *WebSE_FindEntry(const char *key)
{
    WebStringEdEntry *entry;

    for (entry = webStringEntries; entry; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }

    return NULL;
}

static WebStringEdEntry *WebSE_AddOrFindEntry(const char *key)
{
    WebStringEdEntry *entry;

    entry = WebSE_FindEntry(key);
    if (entry) {
        return entry;
    }

    entry = (WebStringEdEntry *)Z_MallocInternal(sizeof(*entry));
    entry->key = WebSE_CopyString(key);
    entry->value = WebSE_CopyString("");
    entry->next = webStringEntries;
    webStringEntries = entry;
    return entry;
}

static void WebSE_SetEntryValue(WebStringEdEntry *entry, const char *value)
{
    if (entry->value) {
        Z_FreeInternal(entry->value);
    }
    entry->value = WebSE_CopyString(value);
}

static qboolean WebSE_JoinPath(const char *dir, const char *name, char *out, unsigned int outSize)
{
    int written;

    written = snprintf(out, outSize, "%s/%s", dir, name);
    if (written < 0 || (unsigned int)written >= outSize) {
        return 0;
    }
    return 1;
}

void CStringEdPackage_SetupNewFileParse(const CStringEdPackage *_this, const char *psFileName)
{
    const char *base;
    const char *slash;
    const char *backslash;
    const char *dot;
    unsigned int len;
    unsigned int i;

    (void)_this;

    base = psFileName;
    slash = strrchr(psFileName, '/');
    backslash = strrchr(psFileName, '\\');
    if (slash && slash + 1 > base) {
        base = slash + 1;
    }
    if (backslash && backslash + 1 > base) {
        base = backslash + 1;
    }

    dot = strrchr(base, '.');
    len = dot ? (unsigned int)(dot - base) : strlen(base);
    if (len >= sizeof(webStringPackageName)) {
        len = sizeof(webStringPackageName) - 1;
    }

    for (i = 0; i < len; i++) {
        webStringPackageName[i] = (char)toupper(base[i]);
    }
    webStringPackageName[len] = '\0';
    webStringLocalReference[0] = '\0';
    webStringEnglishValue[0] = '\0';
    webStringFoundEndMarker = 0;
}

void CStringEdPackage_AddEntry(const CStringEdPackage *_this, const char *psLocalReference)
{
    char fullReference[1152];

    (void)_this;

    WebSE_BuildFullReference(psLocalReference, fullReference, sizeof(fullReference));
    WebSE_AddOrFindEntry(fullReference);

    strncpy(webStringLocalReference, psLocalReference, sizeof(webStringLocalReference) - 1);
    webStringLocalReference[sizeof(webStringLocalReference) - 1] = '\0';
}

void CStringEdPackage_SetString(const CStringEdPackage *_this, const char *psLocalReference, const char *psNewString, qboolean bSentenceIsEnglish)
{
    WebStringEdEntry *entry;
    char fullReference[1152];
    const char *value;

    (void)_this;

    WebSE_BuildFullReference(psLocalReference, fullReference, sizeof(fullReference));
    entry = WebSE_AddOrFindEntry(fullReference);

    value = psNewString;
    if (bSentenceIsEnglish) {
        strncpy(webStringEnglishValue, psNewString, sizeof(webStringEnglishValue) - 1);
        webStringEnglishValue[sizeof(webStringEnglishValue) - 1] = '\0';
    } else if (WebSE_Stricmp(psNewString, "#same") == 0) {
        value = webStringEnglishValue;
    }

    WebSE_SetEntryValue(entry, value);
}

const char *CStringEdPackage_ParseLine(const CStringEdPackage *_this, const char *psLine, int forceEnglish)
{
    char token[1024];
    char sentence[4096];
    const char *afterKeyword;
    const char *error;
    int isEnglish;
    int isCurrentLanguage;

    if (WebSE_Strnicmp(psLine, "VERSION", 7) == 0) {
        WebSE_InsideQuotes(psLine + 7, token, sizeof(token));
        if (atoi(token) != 1) {
            return WebSE_VersionError(atoi(token));
        }
        return NULL;
    }

    if (WebSE_Strnicmp(psLine, "CONFIG", 6) == 0 ||
        WebSE_Strnicmp(psLine, "FILENOTES", 9) == 0 ||
        WebSE_Strnicmp(psLine, "NOTES", 5) == 0 ||
        WebSE_Strnicmp(psLine, "FLAGS", 5) == 0) {
        return NULL;
    }

    if (WebSE_Strnicmp(psLine, "REFERENCE", 9) == 0) {
        WebSE_InsideQuotes(psLine + 9, token, sizeof(token));
        CStringEdPackage_AddEntry(_this, token);
        return NULL;
    }

    if (WebSE_Strnicmp(psLine, "ENDMARKER", 9) == 0) {
        webStringFoundEndMarker = 1;
        return NULL;
    }

    if (WebSE_Strnicmp(psLine, "LANG_", 5) != 0) {
        return WebSE_Error1("Unknown keyword at linestart: \"%s\"\n", psLine);
    }

    if (webStringLocalReference[0] == '\0') {
        return "Error parsing file: Unexpected \"LANG_\"\n";
    }

    afterKeyword = psLine + 5;
    {
        unsigned int len = 0;
        while (afterKeyword[len] &&
               afterKeyword[len] != ' ' &&
               afterKeyword[len] != '\t' &&
               len < sizeof(token) - 1) {
            token[len] = afterKeyword[len];
            len++;
        }
        token[len] = '\0';
        afterKeyword += len;
    }

    WebSE_InsideQuotes(afterKeyword, sentence, sizeof(sentence));
    WebSE_ConvertCRLiterals(sentence);

    error = WebSE_ValidateFormatArgs(sentence);
    isEnglish = (WebSE_Stricmp(token, "english") == 0);
    isCurrentLanguage = (!forceEnglish &&
                         WebSE_Stricmp(token, SEH_GetLanguageName(SEH_GetCurrentLanguage())) == 0);
    if (!error && (isEnglish || isCurrentLanguage)) {
        CStringEdPackage_SetString(_this, webStringLocalReference, sentence, isEnglish);
    }

    return error;
}

void CStringEdPackage_Clear(const CStringEdPackage *_this)
{
    (void)_this;

    WebSE_ClearEntries();
    webStringPackageName[0] = '\0';
    webStringLocalReference[0] = '\0';
    webStringEnglishValue[0] = '\0';
    webStringFoundEndMarker = 0;
}

const char *SE_Load(const char *psFileName, int forceEnglish)
{
    unsigned char *psLoadedData;
    const char *psParsePos;
    const char *psErrorMessage;
    char sLineBuffer[0x4000];

    psLoadedData = SE_LoadFileData(psFileName);
    if (!psLoadedData) {
        return WebSE_Error1("Unable to load \"%s\"!", psFileName);
    }

    CStringEdPackage_SetupNewFileParse(TheStringPackage, psFileName);
    psParsePos = (const char *)psLoadedData;
    psErrorMessage = NULL;

    while (CStringEdPackage_ReadLine(TheStringPackage, &psParsePos, sLineBuffer)) {
        if (sLineBuffer[0] == '\0') {
            continue;
        }

        psErrorMessage = CStringEdPackage_ParseLine(TheStringPackage, sLineBuffer, forceEnglish);
        if (psErrorMessage) {
            break;
        }
    }

    SE_FreeFileDataAfterLoad(psLoadedData);

    if (!psErrorMessage && !webStringFoundEndMarker) {
        psErrorMessage = WebSE_Error1("Truncated file, failed to find \"%s\" at file end!", "ENDMARKER");
    }

    return psErrorMessage;
}

STRINGED_NOINLINE const char *SE_GetString(const char *psPackageAndStringReference)
{
    WebStringEdEntry *entry;

    if (!TheStringPackage) {
        return NULL;
    }

    entry = WebSE_FindEntry(psPackageAndStringReference);
    if (!entry) {
        return NULL;
    }

    return entry->value;
}

static void WebSE_LoadDirectory(const char *dir, int forceEnglish, const char **firstError, int *fileCount, int depth)
{
    const char **dirFiles;
    const char **sysFiles;
    int numDirs;
    int numFiles;
    int i;

    if (*firstError || depth > 16) {
        return;
    }

    dirFiles = FS_ListFiles(dir, "/", 0, &numDirs, 10);
    if (dirFiles) {
        for (i = 0; i < numDirs && !*firstError; i++) {
            char path[256];

            if (!dirFiles[i] || dirFiles[i][0] == '\0' || dirFiles[i][0] == '.') {
                continue;
            }
            if (!WebSE_JoinPath(dir, dirFiles[i], path, sizeof(path))) {
                *firstError = WebSE_Error1("Unable to load \"%s\"!", dirFiles[i]);
                break;
            }
            WebSE_LoadDirectory(path, forceEnglish, firstError, fileCount, depth + 1);
        }
        FS_FreeFileList(dirFiles, 10);
    }

    if (*firstError) {
        return;
    }

    sysFiles = FS_ListFiles(dir, "str", 0, &numFiles, 10);
    if (sysFiles) {
        for (i = 0; i < numFiles && !*firstError; i++) {
            char path[256];
            const char *error;

            if (!sysFiles[i] || sysFiles[i][0] == '\0') {
                continue;
            }
            if (!WebSE_JoinPath(dir, sysFiles[i], path, sizeof(path))) {
                *firstError = WebSE_Error1("Unable to load \"%s\"!", sysFiles[i]);
                break;
            }

            (*fileCount)++;
            error = SE_Load(path, forceEnglish);
            if (error) {
                *firstError = error;
                break;
            }
        }
        FS_FreeFileList(sysFiles, 10);
    }
}

const char *SE_LoadLanguage(int forceEnglish)
{
    const char *psErrorMessage;
    int fileCount;

    if (!TheStringPackage) {
        SE_Init();
    }

    CStringEdPackage_Clear(TheStringPackage);

    psErrorMessage = NULL;
    fileCount = 0;
    WebSE_LoadDirectory("localizedstrings", forceEnglish, &psErrorMessage, &fileCount, 0);
    return psErrorMessage;
}

void SE_Init(void)
{
    if (TheStringPackage) {
        CStringEdPackage_Clear(TheStringPackage);
        return;
    }

    TheStringPackage = (CStringEdPackage *)Z_MallocInternal(0x28);
    memset(TheStringPackage, 0, 0x28);
    CStringEdPackage_Clear(TheStringPackage);
}

void SE_ShutDown(void)
{
    if (!TheStringPackage) {
        return;
    }

    CStringEdPackage_Clear(TheStringPackage);
    Z_FreeInternal(TheStringPackage);
    TheStringPackage = NULL;
}

const unsigned char __ZZN16CStringEdPackage9ParseLineEPKchE5C_208[1120] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x04, 0x05, 0x00, 0x4e, 0x04, 0x05, 0x00, 0xea, 0x04, 0x05, 0x00, 0xea, 0x04, 0x05, 0x00,
    0xea, 0x04, 0x05, 0x00, 0xa1, 0x04, 0x05, 0x00, 0xd6, 0x04, 0x05, 0x00, 0xea, 0x04, 0x05, 0x00,
    0xea, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif
