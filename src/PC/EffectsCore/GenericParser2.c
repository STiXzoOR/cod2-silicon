#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <string.h>

extern void Z_FreeInternal(void *ptr);
extern void *Z_MallocInternal(int size);
extern void I_strncpyz(char *dest, const char *src, int destsize);
extern int strcmpi(const char *str1, const char *str2);

static char token[1024];

const char *GPValue_GetTopValue(const GPValue *_this);
Bool GPValue_IsList(const GPValue *_this);
void GPGroup_SortObject(const GPGroup *_this, GPObject *object, GPObject **unsortedList, GPObject **sortedList, GPObject **lastObject);
void GPGroup_Clean(const GPGroup *_this);
void GenericParser2_GenericParser2(const GenericParser2 *_this);
char *TextPool_AllocText(const TextPool *_this, char *text, unsigned char addNULL, TextPool **poolPtr);
GPGroup *GPGroup_AddGroup(const GPGroup *_this, const char *name, TextPool **textPool);
void GPValue_AddValue(const GPValue *_this, const char *newValue, TextPool **textPool);
GPValue *GPGroup_AddPair(const GPGroup *_this, const char *name, const char *value, TextPool **textPool);
void ZN14GenericParser2D2Ev(GenericParser2 *_this);
void ZN14GenericParser2D1Ev(GenericParser2 *_this);
Bool GPGroup_Parse(const GPGroup *_this, char **dataPtr, TextPool **textPool);
Bool GenericParser2_Parse(const GenericParser2 *_this, char **dataPtr, int cleanFirst, int writeable);

static char *__attribute_regparm__(3) GetToken_impl(char **text, char allowLineBreaks, char readUntilEOL)
{
    char *pointer;
    int length;
    int foundNewline;

    pointer = *text;
    token[0] = '\0';

    if (!pointer) {
        return token;
    }

    while (1) {
        char ch = *pointer;

        foundNewline = 0;

        while (ch && ch <= 0x20) {
            if (ch == '\n') {
                foundNewline = 1;
            }
            pointer++;
            ch = *pointer;
            if (ch > 0x20) {

                if (foundNewline && !allowLineBreaks) {
                    *text = pointer;
                    return token;
                }
                goto check_comment;
            }
            if (!ch) {
                break;
            }
        }

        if (!ch) {
            *text = 0;
            return token;
        }

    check_comment:
        foundNewline = 0;
        if (ch == '/') {
            char next = pointer[1];
            if (next == '/') {

                pointer += 2;
                while (1) {
                    ch = *pointer;
                    if (!ch)
                        break;
                    if (ch == '\n')
                        break;
                    pointer++;
                }
                continue;
            } else if (next == '*') {

                pointer += 2;
                while (1) {
                    ch = *pointer;
                    if (!ch)
                        break;
                    if (ch == '*' && pointer[1] == '/') {
                        pointer += 2;
                        break;
                    }
                    pointer++;
                }
                continue;
            }

        }

        break;
    }

    {
        char ch = *pointer;

        if (ch == '"' && !readUntilEOL) {

            pointer++;
            length = 0;
            while (1) {
                ch = *pointer;
                pointer++;
                if (ch == '"')
                    break;
                if (!ch)
                    break;
                if (length <= 0x3ff) {
                    token[length] = ch;
                    length++;
                }
            }
        } else if (readUntilEOL) {

            length = 0;
            while (1) {

                if (ch == '/') {
                    char next = pointer[1];
                    if (next == '/' || next == '*') {
                        break;
                    }
                    pointer++;
                } else {
                    pointer++;
                }
                if (length <= 0x3ff) {
                    token[length] = ch;
                    length++;
                }
                ch = *pointer;
                if (ch == '\n' || ch == '\r')
                    break;
            }

            if (length > 0) {
                int i = length - 1;
                while (token[i] <= 0x1f) {
                    length = i;
                    if (i == 0) {
                        break;
                    }
                    i--;
                }
            }
        } else {

            if (ch == '/') {
                ch = '/';
            }
            length = 0;

            token[length] = ch;
            length++;
            if (length > 0x3ff) {

                pointer++;
                while (*pointer > 0x20) {
                    pointer++;
                }
            } else {

                pointer++;
                ch = *pointer;
                while (ch > 0x20) {
                    token[length] = ch;
                    length++;
                    if (length > 0x3ff) {
                        pointer++;
                        while (*pointer > 0x20) {
                            pointer++;
                        }
                        goto finish_token;
                    }
                    pointer++;
                    ch = *pointer;
                }
            }
        }

    finish_token:

        if (token[0] == '"') {
            int origLen = length;
            memmove(token, token + 1, origLen - 1);
            length = origLen - 1;
            if (length != 0 && token[origLen - 2] == '"') {
                length = origLen - 2;
            }
        }

        if (length >= 0x400) {
            length = 0;
        }
        token[length] = '\0';
        *text = pointer;
    }

    return token;
}

const char *GPValue_GetTopValue(const GPValue *_this)
{
    GPValue *list = ((GPValue *)_this)->valueList;
    if (list)
        return list->name;
    return NULL;
}

Bool GPValue_IsList(const GPValue *_this)
{
    GPValue *list = ((GPValue *)_this)->valueList;
    if (list) {
        GPValue *nx;
        nx = (GPValue *)list->next;
        if (nx) {
            return 1;
        }
    }
    return 0;
}

void GPGroup_SortObject(const GPGroup *_this, GPObject *object, GPObject **unsortedList, GPObject **sortedList, GPObject **lastObject)
{
    GPObject *test;
    GPObject *last;

    if (*unsortedList == NULL) {
        *sortedList = object;
        *unsortedList = object;
        *lastObject = object;
        return;
    }

    (*lastObject)->next = object;

    test = *sortedList;
    if (!test) {

        *sortedList = object;
        *lastObject = object;
        return;
    }

    last = NULL;
    while (1) {

        int cmp = strcmpi(object->name, test->name);
        if (cmp < 0) {

            test->sortedPrev = object;
            object->sortedNext = test;
            if (last) {

                last->sortedNext = object;
                object->sortedPrev = last;
                *lastObject = object;
            } else {

                *sortedList = object;
                *lastObject = object;
            }
            return;
        }

        {
            GPObject *nextSorted = test->sortedNext;
            if (!nextSorted) {

                last = test;
                break;
            }
            last = test;
            test = nextSorted;
        }
    }

    last->sortedNext = object;
    object->sortedPrev = last;
    *lastObject = object;
}

void GPGroup_Clean(const GPGroup *_this)
{
    GPGroup *grp = (GPGroup *)_this;
    GPValue *pair;
    GPObject *nextPair;
    GPGroup *subGroup;
    GPObject *nextSubGroup;

    pair = grp->pairList;
    while (pair) {
        nextPair = ((GPObject *)pair)->next;
        grp->pairLast = (GPValue *)nextPair;

        {
            GPValue *valNode = pair->valueList;
            while (valNode) {
                GPObject *nextVal = ((GPObject *)valNode)->next;
                Z_FreeInternal(valNode);
                pair->valueList = (GPValue *)nextVal;
                valNode = (GPValue *)nextVal;
            }
        }

        Z_FreeInternal(pair);

        pair = grp->pairLast;
        grp->pairList = pair;
    }

    subGroup = grp->subGroupList;
    while (subGroup) {
        nextSubGroup = ((GPObject *)subGroup)->next;
        grp->subGroupLast = (GPGroup *)nextSubGroup;

        GPGroup_Clean((const GPGroup *)subGroup);

        Z_FreeInternal(subGroup);

        subGroup = grp->subGroupLast;
        grp->subGroupList = subGroup;
    }

    grp->pairLast = NULL;
    grp->pairSorted = NULL;
    grp->pairList = NULL;
    grp->subGroupLast = NULL;
    grp->subGroupSorted = NULL;
    grp->subGroupList = NULL;
    grp->parent = NULL;
    grp->cleanFlag = 0;
}

void GenericParser2_GenericParser2(const GenericParser2 *_this)
{

    GPGroup *grp = (GPGroup *)_this;
    grp->name = "Top Level";
    grp->nextUnsorted = NULL;
    grp->sortedNext = NULL;
    grp->sortedPrev = NULL;
    grp->pairList = NULL;
    grp->pairSorted = NULL;
    grp->pairLast = NULL;
    grp->subGroupList = NULL;
    grp->subGroupSorted = NULL;
    grp->subGroupLast = NULL;
    grp->parent = NULL;
    grp->cleanFlag = 0;

    GenericParser2 *gp = (GenericParser2 *)_this;
    gp->textPoolList = NULL;
    gp->writeable = 0;
}

char *TextPool_AllocText(const TextPool *_this, char *text, unsigned char addNULL, TextPool **poolPtr)
{
    TextPool *pool = (TextPool *)_this;
    int length;
    int extra;
    int used;
    char *dest;

    extra = addNULL ? 1 : 0;
    length = strlen(text) + extra;

    used = pool->used;
    if (used + length + 1 <= pool->capacity) {

        dest = pool->data + used;
        strcpy(dest, text);
        used += length;
        pool->used = used;

        pool->data[used] = '\0';

        return pool->data + used - length;
    }

    if (!poolPtr) {
        return NULL;
    }

    {
        TextPool *newPool;
        int initSize;
        TextPool *prev;
        TextPool *newNode;

        initSize = pool->capacity;

        newPool = (TextPool *)Z_MallocInternal(0x10);

        newPool->next = NULL;
        newPool->capacity = initSize;
        newPool->used = 0;
        newPool->data = (char *)Z_MallocInternal(initSize);

        prev = *poolPtr;
        prev->next = newPool;

        newNode = (*poolPtr)->next;
        *poolPtr = newNode;

        return TextPool_AllocText((const TextPool *)newNode, text, addNULL, NULL);
    }
}

GPGroup *GPGroup_AddGroup(const GPGroup *_this, const char *name, TextPool **textPool)
{
    const char *allocName = name;
    GPGroup *newGroup;

    if (textPool) {
        allocName = TextPool_AllocText(*textPool, (char *)name, 1, textPool);
    }

    newGroup = (GPGroup *)Z_MallocInternal(0x30);

    newGroup->name = allocName;
    newGroup->nextUnsorted = NULL;
    newGroup->sortedNext = NULL;
    newGroup->sortedPrev = NULL;
    newGroup->pairList = NULL;
    newGroup->pairSorted = NULL;
    newGroup->pairLast = NULL;
    newGroup->subGroupList = NULL;
    newGroup->subGroupSorted = NULL;
    newGroup->subGroupLast = NULL;
    newGroup->parent = NULL;
    newGroup->cleanFlag = 0;

    {
        GPGroup *grp = (GPGroup *)_this;
        GPGroup_SortObject(_this, (GPObject *)newGroup,
                           (GPObject **)&grp->subGroupList,
                           (GPObject **)&grp->subGroupSorted,
                           (GPObject **)&grp->subGroupLast);
    }

    return newGroup;
}

void GPValue_AddValue(const GPValue *_this, const char *newValue, TextPool **textPool)
{
    const char *allocValue = newValue;
    GPValue *gpv = (GPValue *)_this;
    GPValue *newNode;

    if (textPool) {
        allocValue = TextPool_AllocText(*textPool, (char *)newValue, 1, textPool);
    }

    if (gpv->valueList) {

        newNode = (GPValue *)Z_MallocInternal(0x10);
        newNode->name = allocValue;
        ((GPObject *)newNode)->next = NULL;
        ((GPObject *)newNode)->sortedNext = NULL;
        ((GPObject *)newNode)->sortedPrev = NULL;

        ((GPObject *)gpv->valueList)->sortedNext->next = (GPObject *)newNode;

        {
            GPObject *head = (GPObject *)gpv->valueList;
            head->sortedNext = head->sortedNext->next;
        }
    } else {

        newNode = (GPValue *)Z_MallocInternal(0x10);
        newNode->name = allocValue;
        ((GPObject *)newNode)->next = NULL;
        ((GPObject *)newNode)->sortedPrev = NULL;
        gpv->valueList = newNode;

        ((GPObject *)newNode)->sortedNext = (GPObject *)newNode;
    }
}

GPValue *GPGroup_AddPair(const GPGroup *_this, const char *name, const char *value, TextPool **textPool)
{
    const char *allocName = name;
    const char *allocValue = value;
    GPValue *newPair;

    if (textPool) {
        allocName = TextPool_AllocText(*textPool, (char *)name, 1, textPool);
        if (value) {
            allocValue = TextPool_AllocText(*textPool, (char *)value, 1, textPool);
        }
    }

    newPair = (GPValue *)Z_MallocInternal(0x14);
    newPair->name = allocName;
    ((GPObject *)newPair)->next = NULL;
    ((GPObject *)newPair)->sortedNext = NULL;
    ((GPObject *)newPair)->sortedPrev = NULL;
    newPair->valueList = NULL;

    if (allocValue) {
        GPValue_AddValue((const GPValue *)newPair, allocValue, NULL);
    }

    {
        GPGroup *grp = (GPGroup *)_this;
        GPGroup_SortObject(_this, (GPObject *)newPair,
                           (GPObject **)&grp->pairList,
                           (GPObject **)&grp->pairSorted,
                           (GPObject **)&grp->pairLast);
    }

    return newPair;
}

static inline __attribute__((always_inline)) void GenericParser2_Destroy(byte *self)
{
    GenericParser2 *gp = (GenericParser2 *)self;
    TextPool *pool;
    TextPool *next;

    GPGroup_Clean((const GPGroup *)self);

    pool = gp->textPoolList;
    while (pool) {
        next = pool->next;

        Z_FreeInternal(pool->data);

        Z_FreeInternal(pool);
        pool = next;
    }

    gp->textPoolList = NULL;

    GPGroup_Clean((const GPGroup *)self);
}

void ZN14GenericParser2D2Ev(GenericParser2 *_this)
{
    GenericParser2_Destroy((byte *)_this);
}

void ZN14GenericParser2D1Ev(GenericParser2 *_this)
{
    GenericParser2_Destroy((byte *)_this);
}

Bool GPGroup_Parse(const GPGroup *_this, char **dataPtr, TextPool **textPool)
{
    byte *self = (byte *)_this;
    char *tok;
    char lastToken[0x400];

    while (1) {

        tok = GetToken_impl(dataPtr, 1, 0);

        if (tok[0] == '\0') {

            if (((GPGroup *)_this)->parent != NULL) {
                return 0;
            }
            return 1;
        }

        if (strcmpi(tok, "}") == 0) {
            return 1;
        }

        I_strncpyz(lastToken, tok, 0x400);

        tok = GetToken_impl(dataPtr, 1, 1);

        if (strcmpi(tok, "{") == 0) {

            GPGroup *newGroup;
            newGroup = GPGroup_AddGroup(_this, lastToken, textPool);

            ((GPGroup *)newGroup)->cleanFlag = ((GPGroup *)_this)->cleanFlag;

            if (!GPGroup_Parse((const GPGroup *)newGroup, dataPtr, textPool)) {
                return 0;
            }
            continue;
        }

        if (strcmpi(tok, "[") == 0) {

            GPValue *newPair;
            newPair = GPGroup_AddPair(_this, lastToken, NULL, textPool);

            while (1) {
                tok = GetToken_impl(dataPtr, 1, 1);
                if (tok[0] == '\0') {
                    return 0;
                }
                if (strcmpi(tok, "]") == 0) {

                    break;
                }

                {
                    char *allocValue = TextPool_AllocText(*textPool, tok, 1, textPool);
                    GPValue_AddValue(newPair, allocValue, NULL);
                }
            }
            continue;
        }

        GPGroup_AddPair(_this, lastToken, tok, textPool);
    }
}

Bool GenericParser2_Parse(const GenericParser2 *_this, char **dataPtr, int cleanFirst, int writeable)
{
    GenericParser2 *gp = (GenericParser2 *)_this;
    TextPool *topPool;

    if (cleanFirst) {

        GPGroup_Clean((const GPGroup *)_this);

        {
            TextPool *pool = gp->textPoolList;
            while (pool) {
                TextPool *next = pool->next;
                Z_FreeInternal(pool->data);
                Z_FreeInternal(pool);
                pool = next;
            }
            gp->textPoolList = NULL;
        }
    }

    if (gp->textPoolList == NULL) {
        TextPool *newPool = (TextPool *)Z_MallocInternal(0x10);
        newPool->next = NULL;
        newPool->capacity = 0x2800;
        newPool->used = 0;
        newPool->data = (char *)Z_MallocInternal(0x2800);
        gp->textPoolList = newPool;
    }

    gp->writeable = (byte)writeable;
    ((GPGroup *)_this)->cleanFlag = (byte)writeable;

    topPool = gp->textPoolList;
    return (Bool)GPGroup_Parse((const GPGroup *)_this, dataPtr, &topPool);
}
