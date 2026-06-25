#include "iokit_stubs.h"

CFMutableDictionaryRef IOServiceMatching(const char *name)
{
    return (CFMutableDictionaryRef)0;
}

int IOServiceGetMatchingServices(mach_port_t masterPort, CFMutableDictionaryRef matching, io_iterator_t *existing)
{
    if (existing)
        *existing = 0;
    return 0;
}

io_object_t IOIteratorNext(io_iterator_t iterator)
{
    return (io_object_t)0;
}

int IOObjectRelease(io_object_t object)
{
    return 0;
}

int IORegistryEntryCreateCFProperties(io_object_t entry, void *properties, void *allocator, unsigned int options)
{
    return 0;
}

int IOCreatePlugInInterfaceForService(io_service_t service, void *pluginType, void *interfaceType, void **theInterface, int *theScore)
{
    return -1;
}

int IOHIDDeviceOpen(void *device, unsigned int options)
{
    return 0;
}

int IOHIDDeviceClose(void *device, unsigned int options)
{
    return 0;
}

mach_port_t IOMasterPort(unsigned int bootstrapPort, mach_port_t *masterPort)
{
    if (masterPort)
        *masterPort = 0;
    return 0;
}
