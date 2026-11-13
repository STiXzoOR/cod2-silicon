#ifndef COD2_MACOS_TEXT_H
#define COD2_MACOS_TEXT_H
/* Returns malloc-owned Windows text bytes for the selected game language. */
char *MacText_EncodeUTF8(const char *text, int language);
#endif
