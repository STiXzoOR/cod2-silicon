#include "common_types.h"
#include "imports.h"

#include <stdlib.h>

extern void R_GetImageList(ImageList *list);
extern void RB_DrawStretchPic(MaterialHandle material, float x, float y, float w, float h,
                              float s0, float t0, float s1, float t1,
                              unsigned int color, int splitScreen);
extern void RB_EndSurface(void);

extern r_backEndGlobals_t backEnd;
extern r_global_permanent_t rgp;

static int RB_CompareTouchImages(const void *e0, const void *e1)
{
    GfxImage *img0 = *(GfxImage *const *)e0;
    GfxImage *img1 = *(GfxImage *const *)e1;

    if (!img1->semantic)
        return -1;
    if (!img0->semantic)
        return 1;

    int diff = img1->cardMemory.platform[0] - img0->cardMemory.platform[0];
    if (diff != 0)
        return diff;

    return (int)(unsigned char)img0->semantic - (int)(unsigned char)img1->semantic;
}

void RB_TouchAllImages(void)
{
    ImageList imageList;
    int imageIndex;
    r_backEndGlobals_t *be = (r_backEndGlobals_t *)imp_backEnd;

    R_GetImageList(&imageList);
    qsort(imageList.image, imageList.count, sizeof(GfxImage *), RB_CompareTouchImages);

    for (imageIndex = 0; imageIndex < imageList.count; imageIndex++) {
        GfxImage *image = imageList.image[imageIndex];
        if (!image->semantic)
            break;

        if (image->mapType != 3)
            continue;

        be->currentFeedbackImage = image;
        RB_DrawStretchPic(rgp.feedbackReplaceMaterial, 0.0f, 0.0f, 8.0f, 8.0f,
                          0.0f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFF, 0xa);
        RB_EndSurface();
    }

    be->currentFeedbackImage = NULL;
    return;
}
