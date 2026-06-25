#include "common_types.h"

#if defined(__x86_64__)
#    define COD2_SUBTABLE_PTR(p) ((intptr_t)(p))
#else
#    define COD2_SUBTABLE_PTR(p) ((int)(p))
#endif

extern const CodeConstantSource s_cameraConsts[];
extern const CodeConstantSource s_codeConsts[];
extern const CodeSamplerSource s_codeSamplers[];
extern const CodeConstantSource s_defaultCodeConsts[];
extern const CodeSamplerSource s_defaultCodeSamplers[];
extern const CodeConstantSource s_lightConsts[];
extern const CodeConstantSource s_lightGridConsts[];
extern const CodeSamplerSource s_lightGridSamplers[];
extern const CodeSamplerSource s_lightSamplers[];
extern const CodeSamplerSource s_lightmapSamplers[];
extern const CodeConstantSource s_nearPlaneConsts[];

const CodeConstantSource s_codeConsts[] = {
    { "camera", 0, COD2_SUBTABLE_PTR(s_cameraConsts), 0, 0 },
    { "nearPlane", 0, COD2_SUBTABLE_PTR(s_nearPlaneConsts), 0, 0 },
    { "light", 0, COD2_SUBTABLE_PTR(s_lightConsts), 2, 1 },
    { "lightGrid", 0, COD2_SUBTABLE_PTR(s_lightGridConsts), 0, 0 },
    { "baseLightingCoords", 152, 0, 0, 0 },
    { "lightingLookupScale", 153, 0, 0, 0 },
    { "debugWeights", 154, 0, 0, 0 },
    { "ambientColor", 158, 0, 0, 0 },
    { "entityAmbient", 162, 0, 0, 0 },
    { "materialColor", 155, 0, 0, 0 },
    { "devgui", 179, 0, 0, 0 },
    { "fogConsts", 156, 0, 0, 0 },
    { "fogColor", 157, 0, 0, 0 },
    { "meanBrightness", 159, 0, 0, 0 },
    { "glowSetup", 160, 0, 0, 0 },
    { "glowApply", 161, 0, 0, 0 },
    { "filterTap", 163, 0, 8, 1 },
    { "renderTargetSize", 171, 0, 0, 0 },
    { "shadowmapSize", 172, 0, 0, 0 },
    { "shadowParms", 173, 0, 0, 0 },
    { "clipSpaceLookupScale", 174, 0, 0, 0 },
    { "clipSpaceLookupOffset", 175, 0, 0, 0 },
    { "outdoorFeatherParms", 178, 0, 0, 0 },
    { "gameTime", 130, 0, 0, 0 },
    { "particleCloudColor", 176, 0, 0, 0 },
    { "particleCloudMatrix", 177, 0, 0, 0 },
    { "worldMatrix", 188, 0, 0, 0 },
    { "inverseWorldMatrix", 189, 0, 0, 0 },
    { "transposeWorldMatrix", 190, 0, 0, 0 },
    { "inverseTransposeWorldMatrix", 191, 0, 0, 0 },
    { "viewMatrix", 192, 0, 0, 0 },
    { "inverseViewMatrix", 193, 0, 0, 0 },
    { "transposeViewMatrix", 194, 0, 0, 0 },
    { "inverseTransposeViewMatrix", 195, 0, 0, 0 },
    { "projectionMatrix", 196, 0, 0, 0 },
    { "inverseProjectionMatrix", 197, 0, 0, 0 },
    { "transposeProjectionMatrix", 198, 0, 0, 0 },
    { "inverseTransposeProjectionMatrix", 199, 0, 0, 0 },
    { "worldViewMatrix", 200, 0, 0, 0 },
    { "inverseWorldViewMatrix", 201, 0, 0, 0 },
    { "transposeWorldViewMatrix", 202, 0, 0, 0 },
    { "inverseTransposeWorldViewMatrix", 203, 0, 0, 0 },
    { "viewProjectionMatrix", 204, 0, 0, 0 },
    { "inverseViewProjectionMatrix", 205, 0, 0, 0 },
    { "transposeViewProjectionMatrix", 206, 0, 0, 0 },
    { "inverseTransposeViewProjectionMatrix", 207, 0, 0, 0 },
    { "worldViewProjectionMatrix", 208, 0, 0, 0 },
    { "inverseWorldViewProjectionMatrix", 209, 0, 0, 0 },
    { "transposeWorldViewProjectionMatrix", 210, 0, 0, 0 },
    { "inverseTransposeWorldViewProjectionMatrix", 211, 0, 0, 0 },
    { "OpenGLworldViewProjectionMatrix", 212, 0, 0, 0 },
    { "OpenGinverseWorldViewProjectionMatrix", 213, 0, 0, 0 },
    { "OpenGtransposeWorldViewProjectionMatrix", 214, 0, 0, 0 },
    { "OpenGinverseTransposeWorldViewProjectionMatrix", 215, 0, 0, 0 },
    { "normalizedWorldMatrix", 216, 0, 0, 0 },
    { "inverseNormalizedWorldMatrix", 217, 0, 0, 0 },
    { "transposeNormalizedWorldMatrix", 218, 0, 0, 0 },
    { "inverseTransposeNormalizedWorldMatrix", 219, 0, 0, 0 },
    { "normalizedWorldViewMatrix", 220, 0, 0, 0 },
    { "inverseNormalizedWorldViewMatrix", 221, 0, 0, 0 },
    { "transposeNormalizedWorldViewMatrix", 222, 0, 0, 0 },
    { "inverseTransposeNormalizedWorldViewMatrix", 223, 0, 0, 0 },
    { "inverseNormalizedWorldViewProjectionMatrix", 225, 0, 0, 0 },
    { "normalizedWorldViewProjectionMatrix", 224, 0, 0, 0 },
    { "transposeNormalizedWorldViewProjectionMatrix", 226, 0, 0, 0 },
    { "inverseTransposeNormalizedWorldViewProjectionMatrix", 227, 0, 0, 0 },
    { "shadowLookupMatrix", 228, 0, 0, 0 },
    { "inverseShadowLookupMatrix", 229, 0, 0, 0 },
    { "transposeShadowLookupMatrix", 230, 0, 0, 0 },
    { "inverseTransposeShadowLookupMatrix", 231, 0, 0, 0 },
    { "lightGridLookupMatrix", 232, 0, 0, 0 },
    { "inverseLightGridLookupMatrix", 233, 0, 0, 0 },
    { "transposeLightGridLookupMatrix", 234, 0, 0, 0 },
    { "inverseTransposeLightGridLookupMatrix", 235, 0, 0, 0 },
    { "worldOutdoorLookupMatrix", 236, 0, 0, 0 },
    { "inverseWorldOutdoorLookupMatrix", 237, 0, 0, 0 },
    { "transposeWorldOutdoorLookupMatrix", 238, 0, 0, 0 },
    { "inverseTransposeWorldOutdoorLookupMatrix", 239, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeSamplerSource s_codeSamplers[] = {
    { "specularity", 3, 0, 0, 0 },
    { "white", 1, 0, 0, 0 },
    { "black", 0, 0, 0, 0 },
    { "identityNormalMap", 2, 0, 0, 0 },
    { "lightmap", 8, COD2_SUBTABLE_PTR(s_lightmapSamplers), 0, 0 },
    { "outdoor", 20, 0, 0, 0 },
    { "shadowCookie", 12, 0, 0, 0 },
    { "dynamicShadow", 19, 0, 0, 0 },
    { "feedback", 13, 0, 0, 0 },
    { "resolvedPostSun", 14, 0, 0, 0 },
    { "resolvedScene", 15, 0, 0, 0 },
    { "sky", 16, 0, 0, 0 },
    { "light", 17, COD2_SUBTABLE_PTR(s_lightSamplers), 2, 1 },
    { "lightGrid", 5, COD2_SUBTABLE_PTR(s_lightGridSamplers), 0, 0 },
    { "floatZ", 21, 0, 0, 0 },
    { "waterColor", 23, 0, 0, 0 },
    { "sunHalfAngle", 22, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeConstantSource s_defaultCodeConsts[] = {
    { "eyePosition", 139, 0, 0, 0 },
    { "eyeForward", 140, 0, 0, 0 },
    { "eyeLeft", 141, 0, 0, 0 },
    { "eyeUp", 142, 0, 0, 0 },
    { "nearPlaneOrg", 143, 0, 0, 0 },
    { "nearPlaneDx", 144, 0, 0, 0 },
    { "nearPlaneDy", 145, 0, 0, 0 },
    { "lightPosition0", 131, 0, 0, 0 },
    { "lightPosition1", 132, 0, 0, 0 },
    { "lightColor0", 133, 0, 0, 0 },
    { "lightColor1", 134, 0, 0, 0 },
    { "lightAmbient0", 135, 0, 0, 0 },
    { "lightAmbient1", 136, 0, 0, 0 },
    { "lightSpecular0", 137, 0, 0, 0 },
    { "lightSpecular1", 138, 0, 0, 0 },
    { "lightGridColorsR0", 146, 0, 0, 0 },
    { "lightGridColorsR1", 147, 0, 0, 0 },
    { "lightGridColorsG0", 148, 0, 0, 0 },
    { "lightGridColorsG1", 149, 0, 0, 0 },
    { "lightGridColorsB0", 150, 0, 0, 0 },
    { "lightGridColorsB1", 151, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeSamplerSource s_defaultCodeSamplers[] = {
    { "specularitySampler", 3, 0, 0, 0 },
    { "shadowCookieSampler", 12, 0, 0, 0 },
    { "feedbackSampler", 13, 0, 0, 0 },
    { "dynamicShadowSampler", 19, 0, 0, 0 },
    { "floatZSampler", 21, 0, 0, 0 },
    { "attenuationSampler", 17, 0, 0, 0 },
    { "lightmapWeightSampler", 7, 0, 0, 0 },
    { "lightmapSamplerR", 8, 0, 0, 0 },
    { "lightmapSamplerG", 9, 0, 0, 0 },
    { "lightmapSamplerB", 10, 0, 0, 0 },
    { "lightmapSamplerSun", 11, 0, 0, 0 },
    { "lightGridWeightSampler0", 5, 0, 0, 0 },
    { "lightGridWeightSampler1", 6, 0, 0, 0 },
    { "smodelLightingSampler", 4, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeConstantSource s_cameraConsts[] = {
    { "position", 139, 0, 0, 0 },
    { "forward", 140, 0, 0, 0 },
    { "left", 141, 0, 0, 0 },
    { "up", 142, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeConstantSource s_lightConsts[] = {
    { "position", 131, 0, 0, 0 },
    { "color", 133, 0, 0, 0 },
    { "ambient", 135, 0, 0, 0 },
    { "specular", 137, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeConstantSource s_lightGridConsts[] = {
    { "colorsR", 146, 0, 2, 1 },
    { "colorsG", 148, 0, 2, 1 },
    { "colorsB", 150, 0, 2, 1 },
    { 0, 0, 0, 0, 0 }
};

const CodeSamplerSource s_lightGridSamplers[] = {
    { "weights", 5, 0, 2, 1 },
    { 0, 0, 0, 0, 0 }
};

const CodeSamplerSource s_lightSamplers[] = {
    { "attenuation", 17, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeSamplerSource s_lightmapSamplers[] = {
    { "weights", 7, 0, 0, 0 },
    { "colorsR", 8, 0, 0, 0 },
    { "colorsG", 9, 0, 0, 0 },
    { "colorsB", 10, 0, 0, 0 },
    { "sun", 11, 0, 0, 0 },
    { "traditional", 8, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

const CodeConstantSource s_nearPlaneConsts[] = {
    { "org", 143, 0, 0, 0 },
    { "dx", 144, 0, 0, 0 },
    { "dy", 145, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
