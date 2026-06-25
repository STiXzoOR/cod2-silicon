#include "common_types.h"
#include "imports.h"

extern INT32 CStreamSound_sQTStreams;

void CStreamSound_CStreamSound(const CStreamSound *_this, CSoundEngine *inEngine);
void ZN12CStreamSoundD1Ev(void);
void ZN12CStreamSoundD0Ev(void);
void CStreamSound_stop_sample(const CStreamSound *_this);
void CStreamSound_end_sample(const CStreamSound *_this);
void CStreamSound_get_sample_ms_position(const CStreamSound *_this, long int *total_milliseconds, long int *current_milliseconds);
void CStreamSound_set_sample_ms_position(const CStreamSound *_this, long int milliseconds);
void CStreamSound_ChangedVolume(const CStreamSound *_this);
void CStreamSound_UpdateAllStreams(void);
void CStreamSound_close_stream(const CStreamSound *_this);
void CStreamSound_resume_sample(const CStreamSound *_this);
void CStreamSound_set_sample_playback_rate(const CStreamSound *_this, long int playback_rate);
bool CStreamSound_open_stream(const CStreamSound *_this, const char *filename);
static void __static_initialization_and_destruction_0(void);
static void GLOBAL__D__ZN12CStreamSound10sQTStreamsE(void);
static void GLOBAL__I__ZN12CStreamSound10sQTStreamsE(void);
void ZNSt6vectorIP12CStreamSoundSaIS1_EE5eraseEN9__gnu_cxx17__normal_iteratorIPS1_S3_EE(void);
void ZNSt6vectorIP12CStreamSoundSaIS1_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS1_S3_EERKS1_(void);

void CStreamSound_CStreamSound(const CStreamSound *_this, CSoundEngine *inEngine)
{
    (void)_this;
    (void)inEngine;
}

void ZN12CStreamSoundD1Ev(void) {}

void ZN12CStreamSoundD0Ev(void) {}

void CStreamSound_stop_sample(const CStreamSound *_this)
{
    (void)_this;
}

void CStreamSound_end_sample(const CStreamSound *_this)
{
    (void)_this;
}

void CStreamSound_get_sample_ms_position(const CStreamSound *_this, long int *total_milliseconds, long int *current_milliseconds)
{
    (void)_this;
    (void)total_milliseconds;
    (void)current_milliseconds;
}

void CStreamSound_set_sample_ms_position(const CStreamSound *_this, long int milliseconds)
{
    (void)_this;
    (void)milliseconds;
}

void CStreamSound_ChangedVolume(const CStreamSound *_this)
{
    (void)_this;
}

void CStreamSound_UpdateAllStreams(void) {}

void CStreamSound_close_stream(const CStreamSound *_this) {}

void CStreamSound_resume_sample(const CStreamSound *_this) {}

void CStreamSound_set_sample_playback_rate(const CStreamSound *_this, long int playback_rate) {}

bool CStreamSound_open_stream(const CStreamSound *_this, const char *filename)
{
    (void)_this;
    (void)filename;
    return 0;
}

static void __static_initialization_and_destruction_0(void) {}

static void GLOBAL__D__ZN12CStreamSound10sQTStreamsE(void)  {}

static void GLOBAL__I__ZN12CStreamSound10sQTStreamsE(void)  {}

void ZNSt6vectorIP12CStreamSoundSaIS1_EE5eraseEN9__gnu_cxx17__normal_iteratorIPS1_S3_EE(void)  {}

void ZNSt6vectorIP12CStreamSoundSaIS1_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS1_S3_EERKS1_(void) {}
