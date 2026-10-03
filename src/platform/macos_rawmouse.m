#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#include "macos_rawmouse.h"
#include <pthread.h>

static pthread_mutex_t mouseLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableArray<GCMouse *> *attached;
static id connectObserver, disconnectObserver;
static dispatch_queue_t mouseQueue;
static double deltaX, deltaY;
static uint64_t eventCount;
static int active, deviceCount;

static void AttachMouse(GCMouse *mouse)
{
    if ([attached containsObject:mouse] || !mouse.mouseInput)
        return;
    [attached addObject:mouse];
    mouse.handlerQueue = mouseQueue;
    mouse.mouseInput.mouseMovedHandler = ^(GCMouseInput *input, float x, float y) {
        (void)input;
        pthread_mutex_lock(&mouseLock);
        if (active) {
            deltaX += x;
            deltaY -= y;
            ++eventCount;
        }
        pthread_mutex_unlock(&mouseLock);
    };
    pthread_mutex_lock(&mouseLock);
    deviceCount = (int)attached.count;
    pthread_mutex_unlock(&mouseLock);
}

void MacRawMouse_Init(void)
{
    if (attached)
        return;
    attached = [[NSMutableArray alloc] init];
    mouseQueue = dispatch_queue_create("org.opencod2.rawmouse", DISPATCH_QUEUE_SERIAL);
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    connectObserver = [center addObserverForName:GCMouseDidConnectNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        AttachMouse(note.object);
    }];
    disconnectObserver = [center addObserverForName:GCMouseDidDisconnectNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        GCMouse *mouse = note.object;
        mouse.mouseInput.mouseMovedHandler = nil;
        [attached removeObject:mouse];
        pthread_mutex_lock(&mouseLock);
        deviceCount = (int)attached.count;
        deltaX = deltaY = 0;
        pthread_mutex_unlock(&mouseLock);
    }];
    for (GCMouse *mouse in GCMouse.mice)
        AttachMouse(mouse);
}

void MacRawMouse_Shutdown(void)
{
    MacRawMouse_SetActive(0);
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    if (connectObserver) [center removeObserver:connectObserver];
    if (disconnectObserver) [center removeObserver:disconnectObserver];
    for (GCMouse *mouse in attached)
        mouse.mouseInput.mouseMovedHandler = nil;
    if (mouseQueue)
        dispatch_sync(mouseQueue, ^{});
    attached = nil;
    connectObserver = disconnectObserver = nil;
    mouseQueue = nil;
    pthread_mutex_lock(&mouseLock);
    deviceCount = 0;
    pthread_mutex_unlock(&mouseLock);
}

void MacRawMouse_SetActive(int value)
{
    pthread_mutex_lock(&mouseLock);
    if (value != active) {
        deltaX = deltaY = 0;
        active = value;
    }
    pthread_mutex_unlock(&mouseLock);
}
int MacRawMouse_Available(void)
{
    pthread_mutex_lock(&mouseLock);
    int count = deviceCount;
    pthread_mutex_unlock(&mouseLock);
    return count;
}
int MacRawMouse_Read(int *dx, int *dy)
{
    pthread_mutex_lock(&mouseLock);
    *dx = (int)deltaX; *dy = (int)deltaY;
    deltaX -= *dx; deltaY -= *dy;
    pthread_mutex_unlock(&mouseLock);
    return *dx != 0 || *dy != 0;
}
uint64_t MacRawMouse_EventCount(void)
{
    pthread_mutex_lock(&mouseLock);
    uint64_t count = eventCount;
    pthread_mutex_unlock(&mouseLock);
    return count;
}
