extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);

void *zcalloc(void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
    return Z_MallocInternal((int)(items * size));
}

void zcfree(void *opaque, void *ptr)
{
    (void)opaque;
    Z_FreeInternal(ptr);
}
