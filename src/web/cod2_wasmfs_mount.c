#include <emscripten/emscripten.h>

#if defined(WEB_WASMFS)
#include <emscripten/wasmfs.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

EMSCRIPTEN_KEEPALIVE int cod2_mount_opfs(void) {
    backend_t opfs = wasmfs_create_opfs_backend();
    if (!opfs) {
        return -1;
    }

    if (wasmfs_create_directory("/opfs", 0777, opfs) != 0) {

        if (errno != EEXIST) {
            return -2;
        }
    }
    return 0;
}

#else

#endif
