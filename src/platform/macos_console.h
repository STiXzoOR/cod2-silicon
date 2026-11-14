#ifndef COD2_MACOS_CONSOLE_H
#define COD2_MACOS_CONSOLE_H

/* The AppKit adapter owns the controls; mac_syscon owns engine commands. */
void MacConsole_Create(void (*submit)(const char *), void (*close)(void),
                       void (*visibility)(int));
void MacConsole_Destroy(void);
void MacConsole_Show(int level);
void MacConsole_AppendText(const char *text, int language);
void MacConsole_SetErrorText(void);

#endif
