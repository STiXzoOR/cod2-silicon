#ifndef SDL_hidapi_h_
#define SDL_hidapi_h_

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_hid_device_;
typedef struct SDL_hid_device_ SDL_hid_device;

typedef struct SDL_hid_device_info
{

    char *path;

    unsigned short vendor_id;

    unsigned short product_id;

    wchar_t *serial_number;

    unsigned short release_number;

    wchar_t *manufacturer_string;

    wchar_t *product_string;

    unsigned short usage_page;

    unsigned short usage;

    int interface_number;

    int interface_class;
    int interface_subclass;
    int interface_protocol;

    struct SDL_hid_device_info *next;
} SDL_hid_device_info;

extern DECLSPEC int SDLCALL SDL_hid_init(void);

extern DECLSPEC int SDLCALL SDL_hid_exit(void);

extern DECLSPEC Uint32 SDLCALL SDL_hid_device_change_count(void);

extern DECLSPEC SDL_hid_device_info * SDLCALL SDL_hid_enumerate(unsigned short vendor_id, unsigned short product_id);

extern DECLSPEC void SDLCALL SDL_hid_free_enumeration(SDL_hid_device_info *devs);

extern DECLSPEC SDL_hid_device * SDLCALL SDL_hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number);

extern DECLSPEC SDL_hid_device * SDLCALL SDL_hid_open_path(const char *path, int bExclusive );

extern DECLSPEC int SDLCALL SDL_hid_write(SDL_hid_device *dev, const unsigned char *data, size_t length);

extern DECLSPEC int SDLCALL SDL_hid_read_timeout(SDL_hid_device *dev, unsigned char *data, size_t length, int milliseconds);

extern DECLSPEC int SDLCALL SDL_hid_read(SDL_hid_device *dev, unsigned char *data, size_t length);

extern DECLSPEC int SDLCALL SDL_hid_set_nonblocking(SDL_hid_device *dev, int nonblock);

extern DECLSPEC int SDLCALL SDL_hid_send_feature_report(SDL_hid_device *dev, const unsigned char *data, size_t length);

extern DECLSPEC int SDLCALL SDL_hid_get_feature_report(SDL_hid_device *dev, unsigned char *data, size_t length);

extern DECLSPEC void SDLCALL SDL_hid_close(SDL_hid_device *dev);

extern DECLSPEC int SDLCALL SDL_hid_get_manufacturer_string(SDL_hid_device *dev, wchar_t *string, size_t maxlen);

extern DECLSPEC int SDLCALL SDL_hid_get_product_string(SDL_hid_device *dev, wchar_t *string, size_t maxlen);

extern DECLSPEC int SDLCALL SDL_hid_get_serial_number_string(SDL_hid_device *dev, wchar_t *string, size_t maxlen);

extern DECLSPEC int SDLCALL SDL_hid_get_indexed_string(SDL_hid_device *dev, int string_index, wchar_t *string, size_t maxlen);

extern DECLSPEC void SDLCALL SDL_hid_ble_scan(SDL_bool active);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
