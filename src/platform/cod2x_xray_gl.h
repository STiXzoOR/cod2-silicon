#ifndef COD2X_XRAY_GL_H
#define COD2X_XRAY_GL_H

/* Render-command payload; no engine or GL objects cross the thread boundary. */
typedef struct {
    int mode, visible, health;
    float color[4][4], healthColor[4];
    float nearPlane, threshold, outline, glow, core, fadeRadius, fadeAlpha;
    float origin[3];
} Cod2xXraySettings;

void Cod2xXray_GLBegin(const Cod2xXraySettings *settings);
void Cod2xXray_GLPlayer(unsigned int flags, const float *origin, const float *view,
                       const float *projection, int targetWidth, int targetHeight,
                       unsigned int first, unsigned int last,
                       unsigned int count, const void *indices);
void Cod2xXray_GLEnd(void);
int Cod2xXray_GLHeadAvailable(void);
void Cod2xXray_GLHead(int operation, float depth);
void Cod2xXray_GLRelease(void);
void Cod2xXray_GLHeadDraw(int before);

#endif
