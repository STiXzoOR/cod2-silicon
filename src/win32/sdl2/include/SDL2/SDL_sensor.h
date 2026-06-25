#ifndef SDL_sensor_h_
#define SDL_sensor_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus

extern "C" {

#endif

struct _SDL_Sensor;
typedef struct _SDL_Sensor SDL_Sensor;

typedef Sint32 SDL_SensorID;

typedef enum
{
    SDL_SENSOR_INVALID = -1,
    SDL_SENSOR_UNKNOWN,
    SDL_SENSOR_ACCEL,
    SDL_SENSOR_GYRO,
    SDL_SENSOR_ACCEL_L,
    SDL_SENSOR_GYRO_L,
    SDL_SENSOR_ACCEL_R,
    SDL_SENSOR_GYRO_R
} SDL_SensorType;

#define SDL_STANDARD_GRAVITY    9.80665f

extern DECLSPEC void SDLCALL SDL_LockSensors(void);
extern DECLSPEC void SDLCALL SDL_UnlockSensors(void);

extern DECLSPEC int SDLCALL SDL_NumSensors(void);

extern DECLSPEC const char *SDLCALL SDL_SensorGetDeviceName(int device_index);

extern DECLSPEC SDL_SensorType SDLCALL SDL_SensorGetDeviceType(int device_index);

extern DECLSPEC int SDLCALL SDL_SensorGetDeviceNonPortableType(int device_index);

extern DECLSPEC SDL_SensorID SDLCALL SDL_SensorGetDeviceInstanceID(int device_index);

extern DECLSPEC SDL_Sensor *SDLCALL SDL_SensorOpen(int device_index);

extern DECLSPEC SDL_Sensor *SDLCALL SDL_SensorFromInstanceID(SDL_SensorID instance_id);

extern DECLSPEC const char *SDLCALL SDL_SensorGetName(SDL_Sensor *sensor);

extern DECLSPEC SDL_SensorType SDLCALL SDL_SensorGetType(SDL_Sensor *sensor);

extern DECLSPEC int SDLCALL SDL_SensorGetNonPortableType(SDL_Sensor *sensor);

extern DECLSPEC SDL_SensorID SDLCALL SDL_SensorGetInstanceID(SDL_Sensor *sensor);

extern DECLSPEC int SDLCALL SDL_SensorGetData(SDL_Sensor *sensor, float *data, int num_values);

extern DECLSPEC int SDLCALL SDL_SensorGetDataWithTimestamp(SDL_Sensor *sensor, Uint64 *timestamp, float *data, int num_values);

extern DECLSPEC void SDLCALL SDL_SensorClose(SDL_Sensor *sensor);

extern DECLSPEC void SDLCALL SDL_SensorUpdate(void);

#ifdef __cplusplus

}

#endif
#include "close_code.h"

#endif
