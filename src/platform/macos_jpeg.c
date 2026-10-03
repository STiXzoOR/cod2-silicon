#include "macos_jpeg.h"
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

unsigned char *MacJpeg_Encode(const unsigned char *rgb, int width, int height, int quality, size_t *size)
{
    unsigned char *result = NULL;
    *size = 0;
    if (!rgb || width <= 0 || height <= 0 || (size_t)width * height > INT_MAX / 4)
        return NULL;
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgb, (size_t)width * height * 3, NULL);
    CGImageRef image = CGImageCreate(width, height, 8, 24, (size_t)width * 3, color,
                                     kCGImageAlphaNone, provider, NULL, false, kCGRenderingIntentDefault);
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    CGImageDestinationRef dest = data ? CGImageDestinationCreateWithData(data, CFSTR("public.jpeg"), 1, NULL) : NULL;
    float value = (float)(quality < 0 ? 0 : quality > 100 ? 100 : quality) / 100;
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberFloatType, &value);
    const void *key = kCGImageDestinationLossyCompressionQuality;
    const void *property = number;
    CFDictionaryRef properties = CFDictionaryCreate(NULL, &key, &property, 1,
                                                     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (image && dest) {
        CGImageDestinationAddImage(dest, image, properties);
        if (CGImageDestinationFinalize(dest)) {
            size_t length = (size_t)CFDataGetLength(data);
            result = malloc(length);
            if (result) {
                memcpy(result, CFDataGetBytePtr(data), length);
                *size = length;
            }
        }
    }
    if (properties) CFRelease(properties);
    if (number) CFRelease(number);
    if (dest) CFRelease(dest);
    if (data) CFRelease(data);
    if (image) CGImageRelease(image);
    if (provider) CGDataProviderRelease(provider);
    if (color) CGColorSpaceRelease(color);
    return result;
}

unsigned char *MacJpeg_Decode(const unsigned char *data, size_t size, int maxDimension, int *width, int *height)
{
    unsigned char *pixels = NULL;
    CFDataRef bytes = CFDataCreate(NULL, data, (CFIndex)size);
    CGImageSourceRef source = bytes ? CGImageSourceCreateWithData(bytes, NULL) : NULL;
    CGImageRef image = source ? CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
    if (image) {
        size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
        if (w && h && w <= (size_t)maxDimension && h <= (size_t)maxDimension && w * h <= INT_MAX / 4) {
            pixels = malloc(w * h * 4);
            CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
            CGContextRef context = pixels ? CGBitmapContextCreate(pixels, w, h, 8, w * 4, color,
                                           kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big) : NULL;
            if (context) {
                CGContextDrawImage(context, CGRectMake(0, 0, w, h), image);
                *width = (int)w;
                *height = (int)h;
                CGContextRelease(context);
            } else {
                free(pixels);
                pixels = NULL;
            }
            if (color) CGColorSpaceRelease(color);
        }
        CGImageRelease(image);
    }
    if (source) CFRelease(source);
    if (bytes) CFRelease(bytes);
    return pixels;
}
