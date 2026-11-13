#ifndef COD2_WIN_SORT_H
#define COD2_WIN_SORT_H

#include <stddef.h>

/* Windows 1.3 CRT qsort (0x57ac90, small-range helper 0x57ac20).
 * Equal elements have observable HUD/FX draw order; libc qsort is not portable.
 * Reconstructed from the owned executable's comparisons, pivot and swap order. */
static void Com_SwapSortSlots(unsigned char *base, size_t width, size_t a, size_t b)
{
    if (a == b)
        return;
    for (size_t byte = 0; byte < width; ++byte) {
        unsigned char value = base[a * width + byte];
        base[a * width + byte] = base[b * width + byte];
        base[b * width + byte] = value;
    }
}

static void Com_SortWindowsRange(unsigned char *base, size_t width, int low, int high,
                                int (*compare)(const void *, const void *))
{
    while (low < high) {
        if (high - low < 8) {
            for (int end = high; end > low; --end) {
                int maximum = low;
                for (int i = low + 1; i <= end; ++i) {
                    if (compare(base + i * width, base + maximum * width) > 0)
                        maximum = i;
                }
                Com_SwapSortSlots(base, width, maximum, end);
            }
            return;
        }

        int pivot = low + (high - low + 1) / 2;
        if (compare(base + low * width, base + pivot * width) > 0)
            Com_SwapSortSlots(base, width, low, pivot);
        if (compare(base + low * width, base + high * width) > 0)
            Com_SwapSortSlots(base, width, low, high);
        if (compare(base + pivot * width, base + high * width) > 0)
            Com_SwapSortSlots(base, width, pivot, high);

        int left = low;
        int right = high;
        for (;;) {
            if (left < pivot) {
                do { ++left; }
                while (left < pivot && compare(base + left * width, base + pivot * width) <= 0);
            }
            if (left >= pivot) {
                do { ++left; }
                while (left <= high && compare(base + left * width, base + pivot * width) <= 0);
            }
            do { --right; }
            while (right > pivot && compare(base + right * width, base + pivot * width) > 0);
            if (left > right)
                break;
            Com_SwapSortSlots(base, width, left, right);
            if (pivot == right)
                pivot = left;
        }

        ++right;
        if (right > pivot) {
            do { --right; }
            while (right > pivot && compare(base + right * width, base + pivot * width) == 0);
        }
        if (right <= pivot) {
            do { --right; }
            while (right > low && compare(base + right * width, base + pivot * width) == 0);
        }

        /* Process the smaller partition first to bound recursion depth. */
        if (right - low < high - left) {
            if (low < right)
                Com_SortWindowsRange(base, width, low, right, compare);
            low = left;
        } else {
            if (left < high)
                Com_SortWindowsRange(base, width, left, high, compare);
            high = right;
        }
    }
}

static void Com_SortLikeWindows(void *base, size_t count, size_t width,
                         int (*compare)(const void *, const void *))
{
    if (count < 2 || width == 0)
        return;
    Com_SortWindowsRange(base, width, 0, (int)count - 1, compare);
}

#endif
