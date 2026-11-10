#include "common_types.h"
#include "imports.h"

extern int I_stricmp(const char *s1, const char *s2);
extern void R_Error(int code, const char *fmt, ...);
extern FontHandle R_LoadFont(const char *fontName, int imageTrack);
extern void R_AddCmdDrawTextWithCursor(const char *text, int maxChars, FontHandle font, float x, float y, float xScale, float yScale, const vec_t *color, int style, int cursorPos, int cursor);
extern void R_AddCmdDrawStretchPic(float x, float y, float w, float h, float s0, float t0, float s1, float t1, const vec_t *color, MaterialHandle material);
extern int ColorIndex(int c);
extern const char *CL_GetHudMsgIconMaterialName(int index);
extern MaterialHandle Material_RegisterHandle(const char *name, int lightmapIndex, int imageTrack);
extern refimport_t ri;

static int registeredFontCount;
static Font *registeredFont[16];
static Glyph nullGlyph;

static FontHandle R_ResolveFont(FontHandle font)
{
    if (font) {
        return font;
    }
    if (registeredFontCount > 0) {
        return registeredFont[0];
    }
    return 0;
}

const Glyph *R_GetCharacterGlyph(FontHandle font, unsigned int letter);
FontHandle R_RegisterFont(const char *fontName, int imageTrack);
int R_DuplicateFont(FontHandle fontCopy, const char *name);
void R_InitFonts(void);
void R_ShutdownFonts(void);
float R_NormalizedTextScale(FontHandle font, float scale);
int R_TextHeight(FontHandle font);
int R_DrawText(const char *text, int maxChars, FontHandle font, float x, float y, float xScale, float yScale, const vec_t *color, int style);
static const short int *__attribute_regparm__(3) R_GetConsoleString(const short int *string, int *limit, char *text, vec_t *color, Bool *foundIcon);
static const short int *__attribute_regparm__(3) R_GetConsoleIcon(const short int *string, int *maxChars, float *iconWidth, float *iconHeight, MaterialHandle *iconMaterial, vec_t *color, Bool *iconHorzFlip);
int R_TextWidth(const char *text, int maxChars, FontHandle font);
int R_ConsoleTextWidth(const short int *string, int maxChars, FontHandle font);
int R_DrawConsoleText(const short int *string, int maxChars, FontHandle font, float x, float y, float xScale, float yScale, const vec_t *color, int style);

static inline __attribute__((always_inline))
const Glyph *
R_GetCharacterGlyph_inl(FontHandle font, unsigned int letter)
{
    Glyph *glyphs;
    int numGlyphs, top, bot, mid;

    if (letter - 0x20 <= 0x5F)
        return &font->glyphs[letter - 0x20];

    numGlyphs = font->glyphCount;
    top = numGlyphs - 1;
    bot = 0x60;

    if (top <= 0x5F) {
        glyphs = font->glyphs;
        return &glyphs[14];
    }

    glyphs = font->glyphs;
    mid = (numGlyphs + 0x5F) / 2;
    if (*(unsigned short *)&glyphs[mid] == letter)
        return (const Glyph *)&glyphs[mid];

    while (top >= bot) {
        mid = (top + bot) / 2;
        if (*(unsigned short *)&glyphs[mid] == letter)
            return (const Glyph *)&glyphs[mid];
        if (letter > *(unsigned short *)&glyphs[mid])
            bot = mid + 1;
        else
            top = mid - 1;
    }

    return &glyphs[14];
}

const Glyph *R_GetCharacterGlyph(FontHandle font, unsigned int letter)
{
    return R_GetCharacterGlyph_inl(font, letter);
}

FontHandle R_RegisterFont(const char *fontName, int imageTrack)
{
    int fontIndex;
    FontHandle font;

    for (fontIndex = 0; fontIndex < registeredFontCount; fontIndex++) {
        if (I_stricmp(fontName, *(const char **)registeredFont[fontIndex]) == 0) {
            return registeredFont[fontIndex];
        }
    }

    if (registeredFontCount > 15) {
        R_Error(1, (const char *)"R_RegisterFont: Too many fonts registered already.\n");
        return 0;
    }

    font = R_LoadFont(fontName, imageTrack);
    if (!font) {
        R_Error(1, (const char *)"R_RegisterFont: Error while reading font '%s'", fontName);
    }

    registeredFont[registeredFontCount] = font;
    registeredFontCount++;

    return font;
}

int R_DuplicateFont(FontHandle fontCopy, const char *name)
{
    int fontIndex;
    FontHandle existing;
    FontHandle newFont;
    char *nameCopy;
    const char *oldName;
    refimport_t *riPtr;

    for (fontIndex = 0; fontIndex < registeredFontCount; fontIndex++) {
        existing = registeredFont[fontIndex];
        if (I_stricmp(name, *(const char **)existing) == 0) {

            oldName = *(const char **)existing;
            memcpy(existing, fontCopy, sizeof(Font_s));
            *(const char **)existing = oldName;
            return 0;
        }
    }

    if (registeredFontCount > 15) {
        R_Error(1, (const char *)"R_DuplicateFont: Too many fonts registered already.\n");
        return 0;
    }

    riPtr = &ri;
    newFont = (FontHandle)(riPtr->Hunk_AllocInternal((int)sizeof(Font_s)));   /* was 0x14 (x86 sizeof(Font)); x64 Font_s is 32 -> memcpy overran */
    memcpy(newFont, fontCopy, sizeof(Font_s));

    nameCopy = (char *)riPtr->Hunk_AllocInternal(strlen(name) + 1);
    strcpy(nameCopy, name);
    *(const char **)newFont = nameCopy;

    registeredFont[fontIndex] = newFont;
    registeredFontCount++;

    return 0;
}

void R_InitFonts(void)
{
}

void R_ShutdownFonts(void)
{
    registeredFontCount = 0;
}

float R_NormalizedTextScale(FontHandle font, float scale)
{
    return 48.0f * scale / (float)font->pixelHeight;
}

int R_TextHeight(FontHandle font)
{
    return font->pixelHeight;
}

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
/* CoD2x text.cpp: double colours and cl_hideServerIp in drawn text. */
extern const char *Cod2x_HideIpText(const char *text, char *buffer, size_t size);
extern const char *Cod2x_ResolveDoubleColors(const char *text, char *buffer, size_t size);
extern const short *Cod2x_HideIpCells(const short *cells, int count, short *buffer, int size);
#endif

int R_DrawText(const char *text, int maxChars, FontHandle font, float x, float y, float xScale, float yScale, const vec_t *color, int style)
{
#if defined(COD2_X64)
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
    /* CoD2x text.cpp: vanilla double colours ("^^11Name") draw as the colour they stand for. */
    char resolved[1024];
    text = Cod2x_ResolveDoubleColors(text, resolved, sizeof(resolved));
#endif
    R_AddCmdDrawTextWithCursor(text, maxChars, font, x, y, xScale, yScale, color, style, -1, 0);
    return 0;
#else
    int (*draw)(const char *, int, FontHandle, float, float, float, float, const vec_t *, int, int, int) =
        (int (*)(const char *, int, FontHandle, float, float, float, float, const vec_t *, int, int, int))R_AddCmdDrawTextWithCursor;
    return draw(text, maxChars, font, x, y, xScale, yScale, color, style, -1, 0);
#endif
}

static __attribute_regparm__(3)
    const short int *R_GetConsoleString(const short int *string, int *limit, char *text, vec_t *color, Bool *foundIcon)
{
    int currentColor;
    int markedEnd;
    const short int *stringEndPos;
    int i;
    int pos;
    unsigned short entry;
    unsigned char ch;
    unsigned char type;

    if (*limit > 0x3ff) {
        *limit = 0x3ff;
    }

    text[0] = '\0';
    currentColor = (int)ColorIndex(0x37);
    *foundIcon = 0;

    if (*limit <= 0) {
        markedEnd = -1;
        stringEndPos = 0;
        i = 0;
        pos = 0;
        goto finish;
    }

    stringEndPos = string;
    markedEnd = -1;
    i = 0;
    pos = 0;

    for (i = 0; i < *limit; i++) {
        stringEndPos = &string[i];
        entry = *(const unsigned short *)&string[i];
        ch = (unsigned char)entry;
        type = (unsigned char)(entry >> 8);

        if (type == currentColor) {

            text[pos] = ch;
            if (ch == 0x20) {
                if (markedEnd == -1) {
                    markedEnd = pos;
                }
                pos++;
                continue;
            }
            markedEnd = -1;
            pos++;
            continue;
        }

        if (type == 0x0d || type == 0x10 || type == 0x11 || type == 0x12) {
            *foundIcon = 1;
            markedEnd = -1;
            pos = pos;
            goto finish;
        }

        if (type == 0x0a) {
            if (color) {
                color[0] = (float)(unsigned int)ch * 0.003921568859368563f;
            }
            continue;
        }

        if (type == 0x0b) {
            if (color) {
                color[1] = (float)(unsigned int)ch * 0.003921568859368563f;
            }
            continue;
        }

        if (type == 0x0c) {
            if (color) {
                color[2] = (float)(unsigned int)ch * 0.003921568859368563f;
            }
            continue;
        }

        text[pos] = '^';
        text[pos + 1] = (char)(type + 0x30);
        pos += 2;
        currentColor = type;
        stringEndPos = &string[i];

        entry = *(const unsigned short *)&string[i];
        ch = (unsigned char)entry;
        text[pos] = ch;
        if (ch == 0x20) {
            if (markedEnd == -1) {
                markedEnd = pos;
            }
            pos++;
            continue;
        }
        markedEnd = -1;
        pos++;
    }

    stringEndPos = 0;

finish:
    text[pos] = '\0';
    if (markedEnd >= 0) {
        text[markedEnd] = '\0';
    }
    *limit -= i;
    return stringEndPos;
}

static __attribute_regparm__(3)
    const short int *R_GetConsoleIcon(const short int *string, int *maxChars, float *iconWidth, float *iconHeight, MaterialHandle *iconMaterial, vec_t *color, Bool *iconHorzFlip)
{
    int i;

    *iconWidth = 1.0f;
    *iconHeight = 1.0f;
    *iconMaterial = NULL;

    if (iconHorzFlip) {
        *iconHorzFlip = 0;
    }

    if (*maxChars <= 0) {
        return NULL;
    }

    for (i = 0; i < *maxChars; ++i) {
        unsigned short entry = (unsigned short)string[i];
        unsigned char value = (unsigned char)entry;

#if defined(COD2_X64)
        switch ((unsigned char)(entry >> 8)) {
#else
        switch (value) {
#endif
        case 0x0d:
            if (color) {
                color[0] = (float)value * 0.003921568859368563f;
            }
            break;

        case 0x0e:
            if (color) {
                color[1] = (float)value * 0.003921568859368563f;
            }
            break;

        case 0x0f:
            if (color) {
                color[2] = (float)value * 0.003921568859368563f;
            }
            break;

        case 0x10:
            *iconWidth *= (float)value * 0.03125f;
            break;

        case 0x11:
            *iconHeight *= (float)value * 0.03125f;
            break;

        case 0x12:
            *iconMaterial = Material_RegisterHandle(CL_GetHudMsgIconMaterialName(value), 0, 7);
            *maxChars -= (i + 1);
            return string + i + 1;

        case 0x13:
            *iconWidth *= (float)value * 0.03125f;
            if (iconHorzFlip) {
                *iconHorzFlip = 1;
            }
            break;

        default:
            break;
        }
    }

    return NULL;
}

static inline __attribute__((always_inline)) int R_TextWidth_inl(const char *text, int maxChars, FontHandle font)
{
    int lineWidth;
    int maxWidth;
    int count;
    unsigned int letter;
    const Glyph *glyph;

    if (maxChars > 0) {

    } else {
        maxChars = 0x7fffffff;
    }

    lineWidth = 0;
    maxWidth = 0;
    count = 0;

    while (*text && count < maxChars) {
        letter = ri.SEH_ReadCharFromString(&text, 0);
        if (letter == '\n') {
            lineWidth = 0;
            continue;
        }
        if (letter == '^') {
            if (text && *text != '^' && *text > '/' && *text <= '9') {
                text++;
                continue;
            }
        }
        glyph = R_GetCharacterGlyph_inl(font, letter);
        lineWidth += glyph->dx;
        if (lineWidth > maxWidth) {
            maxWidth = lineWidth;
        }
        count++;
    }

    return maxWidth;
}

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
/* ---- CoD2x text codes (CoD2x src/mss32/text.cpp) ---------------------------
   Text with CoD2x codes is drawn here piece by piece, each piece in its own colour
   and font, through the engine's own text command. Vanilla "^N" codes are read
   too, so a piece never holds one. The codes are parsed by Cod2x_TextCode. */
extern int Cod2x_TextCode(const char *p, char *code);
extern int Cod2x_ShadeColor(char code, float *rgb);
extern void R_AddCmdDrawTextCmd(const char *text, int maxChars, FontHandle font, float x, float y, float xScale,
                                float yScale, const vec_t *color, int style, int cursorPos, int cursor);
extern void RB_LookupColor(int c, byte *color);
extern r_global_permanent_t rgp;

enum { COD2X_FONT_DEFAULT, COD2X_FONT_BOLD, COD2X_FONT_MONO };

typedef struct {
    float color[4], previous[4], background[4];
    int rainbow, font, backgroundOn;
} Cod2xTextStyle;

/* "^7" is the colour the text was asked for; every colour keeps that alpha. */
static void R_Cod2xCodeColor(char code, const float *base, float *out)
{
    if (code == '7') {
        memcpy(out, base, sizeof(float) * 4);
        return;
    }
    if (!Cod2x_ShadeColor(code, out)) {
        byte rgb[4];
        RB_LookupColor((unsigned char)code, rgb);
        out[0] = rgb[0] / 255.0f;
        out[1] = rgb[1] / 255.0f;
        out[2] = rgb[2] / 255.0f;
    }
    out[3] = base[3];
}

static FontHandle R_Cod2xFindFont(const char *name)
{
    int i;
    for (i = 0; i < registeredFontCount; ++i)
        if (!I_stricmp(name, *(const char **)registeredFont[i]))
            return registeredFont[i];
    return NULL;
}

/* Only the typeface changes, not the size: *scale brings the font to the base
   font's height. Ordinary text is drawn from the extra big font so it looks the
   same for every player; a console or bold base font is kept. */
static FontHandle R_Cod2xStyleFont(int font, FontHandle base, float *scale)
{
    FontHandle resolved = NULL;
    *scale = 1.0f;
    if (font == COD2X_FONT_BOLD)
        resolved = R_Cod2xFindFont("$boldfont");
    else if (font == COD2X_FONT_MONO)
        resolved = R_Cod2xFindFont("$consolefont");
    else if (base && (base == R_Cod2xFindFont("$smallfont") || base == R_Cod2xFindFont("$font") ||
                      base == R_Cod2xFindFont("$bigfont")))
        resolved = R_Cod2xFindFont("$extrabigfont");
    if (!resolved || !base || resolved == base || base->pixelHeight <= 0 || resolved->pixelHeight <= 0)
        return base;
    *scale = (float)base->pixelHeight / (float)resolved->pixelHeight;
    return resolved;
}

static void R_Cod2xStyleInit(Cod2xTextStyle *style, const float *base)
{
    memcpy(style->color, base, sizeof(style->color));
    memcpy(style->previous, base, sizeof(style->previous));
    style->rainbow = 0;
    style->font = COD2X_FONT_DEFAULT;
    style->backgroundOn = 0;
}

static void R_Cod2xStyleApply(Cod2xTextStyle *style, char code, const float *base)
{
    float color[4];
    switch (code) {
    case '*': style->rainbow = 1; break;
    case '!': style->rainbow = 0; break;
    case '+': style->font = COD2X_FONT_BOLD; break;
    case '=': style->font = COD2X_FONT_MONO; break;
    case '-': style->font = COD2X_FONT_DEFAULT; break;
    case '_':
        /* Bar on: a half-transparent bar in this colour; the text goes back one colour. */
        style->backgroundOn = 1;
        memcpy(style->background, style->color, sizeof(style->background));
        style->background[3] = base[3] * 0.5f;
        memcpy(style->color, style->previous, sizeof(style->color));
        break;
    case '.': style->backgroundOn = 0; break;
    default:
        /* "^7" ends the styles too. */
        if (code == '7') {
            R_Cod2xStyleInit(style, base);
            break;
        }
        R_Cod2xCodeColor(code, base, color);
        memcpy(style->previous, style->color, sizeof(style->previous));
        memcpy(style->color, color, sizeof(style->color));
        break;
    }
}

/* A slow rainbow: one turn every ten seconds. */
static void R_Cod2xDrawColor(const Cod2xTextStyle *style, float *out)
{
    float hue, fraction;
    int sector;
    memcpy(out, style->color, sizeof(float) * 4);
    if (!style->rainbow)
        return;
    hue = (float)((unsigned int)ri.Milliseconds() % 10000u) * (6.0f / 10000.0f);
    sector = (int)hue;
    fraction = hue - (float)sector;
    switch (sector) {
    case 0: out[0] = 1.0f; out[1] = fraction; out[2] = 0.0f; break;
    case 1: out[0] = 1.0f - fraction; out[1] = 1.0f; out[2] = 0.0f; break;
    case 2: out[0] = 0.0f; out[1] = 1.0f; out[2] = fraction; break;
    case 3: out[0] = 0.0f; out[1] = 1.0f - fraction; out[2] = 1.0f; break;
    case 4: out[0] = fraction; out[1] = 0.0f; out[2] = 1.0f; break;
    default: out[0] = 1.0f; out[1] = 0.0f; out[2] = 1.0f - fraction; break;
    }
}

static int R_Cod2xHasCodes(const char *text)
{
    char code;
    for (; *text; ++text)
        if (*text != '^' && Cod2x_TextCode(text, &code))
            return 1;
    return 0;
}

/* The next piece of plain text: the engine still reads "^N", so a caret before a digit ends it. */
static int R_Cod2xPiece(const char *text, int len, int start, char *piece, int size)
{
    int n = 0;
    while (start + n < len && n < size - 1) {
        char c = text[start + n], next;
        piece[n++] = c;
        next = start + n < len ? text[start + n] : '\0';
        if (c == '^' && next >= '0' && next <= '9')
            break;
    }
    piece[n] = '\0';
    return n;
}

static float R_Cod2xPlainWidth(const char *text, int len, FontHandle font, const Cod2xTextStyle *style)
{
    float scale, width = 0.0f;
    FontHandle drawFont = R_Cod2xStyleFont(style->font, font, &scale);
    char piece[256];
    int start = 0;
    while (start < len) {
        int n = R_Cod2xPiece(text, len, start, piece, sizeof(piece));
        width += (float)R_TextWidth_inl(piece, 0, drawFont) * scale;
        start += n;
    }
    return width;
}

/* Draws plain text in one style and moves *x past it; the cursor goes before
   character cursorPos (after the text when it equals len). */
static void R_Cod2xDrawPlain(const char *text, int len, FontHandle font, float *x, float y, float xScale,
                             float yScale, const Cod2xTextStyle *textStyle, int style, int cursorPos, int cursor)
{
    float color[4], scale;
    int start = 0;
    if (len <= 0 && cursorPos < 0)
        return;
    R_Cod2xDrawColor(textStyle, color);
    if (textStyle->backgroundOn && len > 0) {
        /* From the top of the capitals to just below the baseline. */
        float width = R_Cod2xPlainWidth(text, len, font, textStyle) * xScale;
        float height = (float)font->pixelHeight * yScale;
        R_AddCmdDrawStretchPic(*x, y - height * 0.88f, width, height * 0.98f, 0.0f, 0.0f, 1.0f, 1.0f,
                               textStyle->background, rgp.whiteMaterial);
    }
    font = R_Cod2xStyleFont(textStyle->font, font, &scale);
    xScale *= scale;
    yScale *= scale;
    do {
        char piece[256];
        int n = R_Cod2xPiece(text, len, start, piece, sizeof(piece)), end = start + n;
        if (cursorPos >= start && (cursorPos < end || (end == len && cursorPos == len)))
            R_AddCmdDrawTextCmd(piece, 0x7fffffff, font, *x, y, xScale, yScale, color, style, cursorPos - start,
                                cursor);
        else
            R_AddCmdDrawTextCmd(piece, 0x7fffffff, font, *x, y, xScale, yScale, color, style, -1, 0);
        *x += (float)R_TextWidth_inl(piece, 0, font) * xScale;
        start = end;
    } while (start < len);
}

/* Draws text with CoD2x codes; returns 0, drawing nothing, for text without them.
   cursorPos counts the codes, as the engine does; a cursor on a code goes before
   the next character. */
int R_Cod2xDrawText(const char *text, int maxChars, FontHandle font, float x, float y, float xScale, float yScale,
                    const vec_t *color, int style, int cursorPos, int cursor)
{
    Cod2xTextStyle textStyle;
    int left = maxChars < 0 ? 0x7fffffff : maxChars;
    const float lineX = x;
    const char *codes = text, *segment = text, *p = text;
    if (!text || !R_Cod2xHasCodes(text))
        return 0;
    R_Cod2xStyleInit(&textStyle, color);
    for (;;) {
        char code;
        int length = 0, last = !*p || left <= 0;
        if (!last) {
            length = Cod2x_TextCode(p, &code);
            if (!length && *p != '\n' && *p != '\r') {
                ++p;
                --left;
                continue;
            }
        }
        if (p > segment || last) {
            int segmentCursor = -1;
            if (cursorPos >= 0) {
                int from = (int)(codes - text), to = (int)(p - text);
                if (cursorPos >= from && (cursorPos < to || (last && cursorPos == to)))
                    segmentCursor = cursorPos > (int)(segment - text) ? cursorPos - (int)(segment - text) : 0;
            }
            R_Cod2xDrawPlain(segment, (int)(p - segment), font, &x, y, xScale, yScale, &textStyle, style,
                             segmentCursor, cursor);
            codes = p;
        }
        if (last)
            break;
        if (length) {
            R_Cod2xStyleApply(&textStyle, code, color);
            p += length;
        } else {
            /* A new line goes back to the start; '\n' also one line down. */
            x = lineX;
            if (*p == '\n')
                y += (float)font->pixelHeight * yScale;
            ++p;
        }
        segment = p;
    }
    return 1;
}

/* Width of text with CoD2x codes, each character in the font it is drawn with;
   the widest line, like the engine. */
static int R_Cod2xTextWidth(const char *text, int maxChars, FontHandle font)
{
    static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    Cod2xTextStyle style;
    int left = maxChars <= 0 ? 0x7fffffff : maxChars;
    float width = 0.0f, widest = 0.0f;
    const char *p = text;
    R_Cod2xStyleInit(&style, white);
    while (*p && left > 0) {
        char code, c[2];
        float scale;
        FontHandle charFont;
        int length = Cod2x_TextCode(p, &code);
        if (length) {
            R_Cod2xStyleApply(&style, code, white);
            p += length;
            continue;
        }
        if (*p == '\n') {
            if (width > widest)
                widest = width;
            width = 0.0f;
            ++p;
            continue;
        }
        charFont = R_Cod2xStyleFont(style.font, font, &scale);
        c[0] = *p;
        c[1] = '\0';
        width += (float)R_TextWidth_inl(c, 1, charFont) * scale;
        ++p;
        --left;
    }
    if (width > widest)
        widest = width;
    return (int)(widest + 0.5f);
}

/* ---- Console cells with CoD2x colours ---------------------------------------
   A cell's high byte is its colour: 0-9 vanilla, and from 0x40 a CoD2x colour
   byte, 0x40 + colour + 40 * rainbow, where colours 10-39 are the shades. Lines
   holding such cells are drawn here; the console has one font and no bars. */
static int R_Cod2xCellsHaveCodes(const short *cells, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        if (((cells[i] >> 8) & 0xff) >= 0x40)
            return 1;
    return 0;
}

static void R_Cod2xCellStyle(Cod2xTextStyle *style, int value, const float *base)
{
    int index = 7, rainbow = 0;
    if (value >= 0 && value <= 9) {
        index = value;
    } else if (value >= 0x40 && value < 0x40 + 80) {
        index = (value - 0x40) % 40;
        rainbow = (value - 0x40) / 40;
    }
    R_Cod2xStyleInit(style, base);
    R_Cod2xStyleApply(style, index <= 9 ? (char)('0' + index)
                                        : (char)(index - 10 < 20 ? 'a' + index - 10 : 'A' + index - 30), base);
    style->rainbow = rainbow;
}

static float R_Cod2xConsoleLine(const short *cells, int count, FontHandle font, float x, float y, float xScale,
                                float yScale, const float *color, int style, int measure)
{
    Cod2xTextStyle textStyle;
    float base[4], iconColor[4];
    char text[1024];
    int length = 0, current = -1, i;
    R_Cod2xStyleInit(&textStyle, color);
    memcpy(base, color, sizeof(base));
    memcpy(iconColor, color, sizeof(iconColor));
    for (i = 0; i <= count; ++i) {
        int type = i < count ? (cells[i] >> 8) & 0xff : -2;
        char c = i < count ? (char)(cells[i] & 0xff) : '\0';
        int icon = type >= 0x0d && type <= 0x13;
        if (i < count && !c && !icon)
            continue;
        if (type != current || icon || (type >= 0x0a && type <= 0x0c)) {
            if (length) {
                if (measure)
                    x += R_Cod2xPlainWidth(text, length, font, &textStyle) * xScale;
                else
                    R_Cod2xDrawPlain(text, length, font, &x, y, xScale, yScale, &textStyle, style, -1, 0);
                length = 0;
            }
            if (i == count)
                break;
            if (type >= 0x0a && type <= 0x0c) {
                /* A colour for the text after it and the next icon, as the engine reads it. */
                base[type - 0x0a] = iconColor[type - 0x0a] = (float)(unsigned char)c * (1.0f / 255.0f);
                current = -1;
                continue;
            }
            if (icon) {
                /* A kill icon, drawn like the engine does. */
                float width, height;
                MaterialHandle material;
                Bool flip;
                int left = count - i;
                const short *next = R_GetConsoleIcon(cells + i, &left, &width, &height, &material,
                                                     iconColor, &flip);
                if (measure) {
                    x += width;
                } else {
                    float pixels = (float)font->pixelHeight;
                    width *= xScale * pixels;
                    height *= yScale * pixels;
                    R_AddCmdDrawStretchPic(x, y + (yScale * pixels * 0.8f + height) * -0.5f, width, height,
                                           flip ? 1.0f : 0.0f, 0.0f, flip ? 0.0f : 1.0f, 1.0f, iconColor,
                                           material);
                    x += width;
                }
                if (!next)
                    break;
                i = (int)(next - cells) - 1;
                current = -1;
                continue;
            }
            R_Cod2xCellStyle(&textStyle, type, base);
            current = type;
        }
        if (length < (int)sizeof(text) - 1)
            text[length++] = c;
    }
    return x;
}
#endif

int R_TextWidth(const char *text, int maxChars, FontHandle font)
{
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
    char resolved[1024], hidden[1024];
    /* CoD2x text.cpp: "^^11" measures like "^1". */
    text = Cod2x_ResolveDoubleColors(text, resolved, sizeof(resolved));
    text = Cod2x_HideIpText(text, hidden, sizeof(hidden));
    if (R_Cod2xHasCodes(text))
        return R_Cod2xTextWidth(text, maxChars, font);
#endif
    return R_TextWidth_inl(text, maxChars, font);
}

int R_ConsoleTextWidth(const short int *string, int maxChars, FontHandle font)
{
    const short int *stringRemaining;
    float width;
    char convertedString[1024];
    Bool iconWasFound;
    float iconWidth;
    float iconHeight;
    MaterialHandle iconMaterial;

    if (!string) {
        return 0;
    }

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
    {
        static short hidden[1024];
        string = Cod2x_HideIpCells(string, maxChars, hidden, 1024);
        if (R_Cod2xCellsHaveCodes(string, maxChars))
        {
            static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            return (int)R_Cod2xConsoleLine(string, maxChars, font, 0.0f, 0.0f, 1.0f, 1.0f, white, 0, 1);
        }
    }
#endif
    stringRemaining = string;
    width = 0.0f;

    while (stringRemaining) {
        stringRemaining = R_GetConsoleString(stringRemaining, &maxChars, convertedString, NULL, &iconWasFound);
        width += (float)R_TextWidth_inl(convertedString, maxChars, font);

        if (iconWasFound) {
            stringRemaining = R_GetConsoleIcon(stringRemaining, &maxChars, &iconWidth, &iconHeight, &iconMaterial, NULL, NULL);
            width += iconWidth;
        }
    }

    return (int)width;
}

int R_DrawConsoleText(const short int *string, int maxChars, FontHandle font, float x, float y, float xScale, float yScale, const vec_t *color, int style)
{
    const short int *stringRemaining;
    float xOfs;
    char convertedString[1024];
    Bool iconWasFound;
    vec4_t curColor;
    float iconWidth;
    float iconHeight;
    MaterialHandle iconMaterial;
    Bool iconHorzFlip;
    float fontPixelHeight;
    float scaledW;
    float scaledH;
    float iconY;
    float iconX;
    int textWidth;

    curColor[0] = color[0];
    curColor[1] = color[1];
    curColor[2] = color[2];
    curColor[3] = color[3];

    if (!string) {
        return 0;
    }

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
    {
        static short hidden[1024];
        string = Cod2x_HideIpCells(string, maxChars, hidden, 1024);
        if (R_Cod2xCellsHaveCodes(string, maxChars)) {
            R_Cod2xConsoleLine(string, maxChars, font, x, y, xScale, yScale, color, style, 0);
            return 0;
        }
    }
#endif
    stringRemaining = string;
    xOfs = 0.0f;

    while (stringRemaining) {
        stringRemaining = R_GetConsoleString(stringRemaining, &maxChars, convertedString, curColor, &iconWasFound);

        if (convertedString[0] != '\0') {
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
            /* The engine's own command: plain console text is not read for CoD2x codes. */
            R_AddCmdDrawTextCmd(convertedString, 0x7fffffff, font, x + xOfs, y, xScale, yScale, curColor, style, -1, 0);
#else
            R_AddCmdDrawTextWithCursor(convertedString, 0x7fffffff, font, x + xOfs, y, xScale, yScale, curColor, style, -1, 0);
#endif
        }

        if (!iconWasFound) {
            continue;
        }

        textWidth = R_TextWidth_inl(convertedString, maxChars, font);
        xOfs += (float)textWidth * xScale;

        stringRemaining = R_GetConsoleIcon(stringRemaining, &maxChars, &iconWidth, &iconHeight, &iconMaterial, curColor, &iconHorzFlip);

        fontPixelHeight = (float)font->pixelHeight;
        scaledW = xScale * fontPixelHeight * iconWidth;
        iconWidth = scaledW;
        scaledH = yScale * fontPixelHeight * iconHeight;
        iconHeight = scaledH;
        iconY = y + (yScale * fontPixelHeight * 0.800000011920929f + scaledH) * -0.5f;
        iconX = x + xOfs;

        if (iconHorzFlip) {
            R_AddCmdDrawStretchPic(iconX, iconY, scaledW, scaledH, 1.0f, 0.0f, 0.0f, 1.0f, curColor, iconMaterial);
        } else {
            R_AddCmdDrawStretchPic(iconX, iconY, scaledW, scaledH, 0.0f, 0.0f, 1.0f, 1.0f, curColor, iconMaterial);
        }

        xOfs += scaledW;
    }

    return 0;
}
