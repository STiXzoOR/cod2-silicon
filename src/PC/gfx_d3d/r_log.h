#ifndef COD2_RENDERER_LOG_H
#define COD2_RENDERER_LOG_H

/* Windows 1.3 renderer diagnostics (gfx_d3d_mp_x86_s.dll, 10037c50). */
extern int R_LogEnabled(void);
extern void R_LogPrintf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
extern void R_LogFrameEnd(void);
extern void R_LogState(unsigned int state, unsigned int changed, int word);
extern const char *R_LogTechniqueName(int technique);

#define R_LOG(...) do { \
    if (R_LogEnabled()) R_LogPrintf(__VA_ARGS__); \
} while (0)
#define R_LOG_STATE(state, changed, word) do { \
    if (R_LogEnabled()) R_LogState(state, changed, word); \
} while (0)

#endif
