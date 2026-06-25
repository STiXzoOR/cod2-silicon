#include "common_types.h"
#include "imports.h"

#include <string.h>

extern vec_t Vec2Normalize(vec_t *v);

static void SwapVec2(vec2_t a, vec2_t b)
{
    float tmp0 = a[0];
    float tmp1 = a[1];
    a[0] = b[0];
    a[1] = b[1];
    b[0] = tmp0;
    b[1] = tmp1;
}

static int Com_RecursivelyGrowHull(vec2_t *points, int numPoints, int hullIdx1, int hullIdx2, vec2_t *hull, int hullPointCount)
{
    vec2_t edgeEq;
    float dist;
    int topIndex;
    int botIndex;
    int frontIndex;
    float maxDist;
    int newIndex;

    edgeEq[0] = hull[hullIdx1][1] - hull[hullIdx2][1];
    edgeEq[1] = hull[hullIdx2][0] - hull[hullIdx1][0];
    Vec2Normalize(edgeEq);

    dist = edgeEq[0] * hull[hullIdx1][0] + edgeEq[1] * hull[hullIdx1][1];

    topIndex = numPoints - 1;
    if (topIndex < 0) {
        return hullPointCount;
    }

    botIndex = 0;
    maxDist = 0.001f;
    frontIndex = -1;

    while (botIndex <= topIndex) {
        float d;

        while (botIndex <= topIndex) {
            d = edgeEq[0] * points[botIndex][0] + edgeEq[1] * points[botIndex][1] - dist;

            if (d <= 0.0f) {

                goto scan_top;
            }

            if (d > maxDist) {
                maxDist = d;
                frontIndex = botIndex;
            }
            botIndex++;
        }
        break;

    scan_top:

        while (topIndex >= botIndex) {
            d = edgeEq[0] * points[topIndex][0] + edgeEq[1] * points[topIndex][1] - dist;

            if (d > 0.0f) {

                if (d > maxDist) {
                    maxDist = d;
                    frontIndex = botIndex;
                }

                if (botIndex != topIndex) {
                    SwapVec2(points[botIndex], points[topIndex]);
                }
                botIndex++;
                topIndex--;
                break;
            }

            topIndex--;
        }
    }

    if (frontIndex < 0) {
        return hullPointCount;
    }

    if (frontIndex != topIndex) {
        SwapVec2(points[frontIndex], points[topIndex]);
    }

    newIndex = hullIdx1 + 1;

    memmove(&hull[newIndex + 1], &hull[newIndex], (hullPointCount - newIndex) * sizeof(vec2_t));

    hull[newIndex][0] = points[topIndex][0];
    hull[newIndex][1] = points[topIndex][1];
    hullPointCount++;

    if (topIndex == 0) {
        return hullPointCount;
    }

    if (hullIdx2 != 0) {
        hullIdx2 = hullIdx1 + 2;
    }

    hullPointCount = Com_RecursivelyGrowHull(points, topIndex, newIndex, hullIdx2, hull, hullPointCount);

    hullPointCount = Com_RecursivelyGrowHull(points, topIndex, hullIdx1, newIndex, hull, hullPointCount);

    return hullPointCount;
}

int Com_ConvexHull(vec2_t *points, int pointCount, vec2_t *hull)
{
    int topIdx, botIdx;
    int pointIndex1;
    int hullPointCount;
    vec2_t edgeEq;
    float dist;
    int frontIndex, backIndex;
    float maxFrontDist, maxBackDist;
    int i;
    int remainingCount;
    int insertIdx;

    topIdx = 0;
    botIdx = 0;

    if (pointCount > 1) {

        int j;
        for (j = 1; j < pointCount; j++) {
            if (points[j][1] >= points[botIdx][1]) {
                botIdx = j;
            }
            if (points[j][1] < points[topIdx][1]) {
                topIdx = j;
            }
        }
    }

    hull[0][0] = points[topIdx][0];
    hull[0][1] = points[topIdx][1];
    hull[1][0] = points[botIdx][0];
    hull[1][1] = points[botIdx][1];

    if (topIdx > botIdx) {

        pointIndex1 = pointCount - 1;
        if (topIdx != pointIndex1) {
            SwapVec2(points[topIdx], points[pointIndex1]);
        }

        pointIndex1 = pointCount - 2;
        if (botIdx != pointIndex1) {
            SwapVec2(points[botIdx], points[pointIndex1]);
        }
    } else {

        pointIndex1 = pointCount - 1;
        if (botIdx != pointIndex1) {
            SwapVec2(points[botIdx], points[pointIndex1]);
        }

        pointIndex1 = pointCount - 2;
        if (topIdx != pointIndex1) {
            SwapVec2(points[topIdx], points[pointIndex1]);
        }
    }

    remainingCount = pointCount - 2;

    edgeEq[0] = hull[1][1] - hull[0][1];
    edgeEq[1] = hull[0][0] - hull[1][0];
    Vec2Normalize(edgeEq);

    dist = edgeEq[0] * hull[0][0] + edgeEq[1] * hull[0][1];

    i = pointCount - 3;
    if (i < 0) {
        hullPointCount = 0;
        goto done;
    }

    {
        int botScan = 0;
        maxFrontDist = 0.001f;
        frontIndex = -1;
        maxBackDist = -0.001f;
        backIndex = -1;

        while (botScan <= i) {
            float d;

            while (botScan <= i) {
                d = edgeEq[0] * points[botScan][0] + edgeEq[1] * points[botScan][1] - dist;

                if (d < 0.0f) {

                    if (d < maxBackDist) {
                        backIndex = botScan;
                        maxBackDist = d;
                    }

                    goto convex_scan_top;
                }

                if (d > maxFrontDist) {
                    maxFrontDist = d;
                    frontIndex = botScan;
                }
                botScan++;
            }
            break;

        convex_scan_top:

            while (i >= botScan) {
                d = edgeEq[0] * points[i][0] + edgeEq[1] * points[i][1] - dist;

                if (d > 0.0f) {

                    if (d > maxFrontDist) {
                        maxFrontDist = d;
                        frontIndex = botScan;
                    }

                    if (botScan != i) {
                        SwapVec2(points[botScan], points[i]);
                    }

                    if (backIndex == botScan) {
                        backIndex = i;
                    }
                    botScan++;
                    i--;
                    break;
                }

                if (d < maxBackDist) {
                    maxBackDist = d;
                    backIndex = i;
                }
                i--;
            }
        }

        if (frontIndex < 0) {

            if (backIndex < 0) {
                hullPointCount = 0;
                goto done;
            }

            hullPointCount = 3;
            insertIdx = 1;
            goto handle_back_only;
        }

        if (frontIndex != i) {
            SwapVec2(points[frontIndex], points[i]);
        }

        hull[2][0] = points[i][0];
        hull[2][1] = points[i][1];

        if (i <= 0) {
            hullPointCount = 3;
        } else {

            hullPointCount = Com_RecursivelyGrowHull(points, i, 2, 0, hull, 3);

            hullPointCount = Com_RecursivelyGrowHull(points, i, 1, 2, hull, hullPointCount);
        }

        if (backIndex < 0) {
            goto done;
        }

        insertIdx = hullPointCount - 1;
        hullPointCount++;

    handle_back_only:

        if (backIndex != botScan) {
            SwapVec2(points[backIndex], points[botScan]);
        }

        memmove(&hull[2], &hull[1], insertIdx * sizeof(vec2_t));
        hull[1][0] = points[botScan][0];
        hull[1][1] = points[botScan][1];

        remainingCount -= botScan;
        remainingCount--;

        if (remainingCount <= 0) {
            goto done;
        }

        {
            vec2_t *backPoints = &points[botScan + 1];

            hullPointCount = Com_RecursivelyGrowHull(backPoints, remainingCount, 1, 2, hull, hullPointCount);
            hullPointCount = Com_RecursivelyGrowHull(backPoints, remainingCount, 0, 1, hull, hullPointCount);
        }
    }

done:
    return hullPointCount;
}
