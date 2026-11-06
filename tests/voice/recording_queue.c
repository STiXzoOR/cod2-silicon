/* Synthetic boundary probe for the real dormant recording queue. */
#include "common_types.h"
#include <assert.h>
#include <fenv.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char enc_buffer[4096];
int enc_buffer_pos;
short partial_audio_buffer[640];
float voice_current_voicelevel, voice_current_scaler;
extern int samples_in_partial_audio_buffer;
extern int Record_QueueAudioDataForEncoding(audioSample_t *);
static int send_enabled, talking, frame_size, encoded_size, network_result;
static int first_event, current_id;
static void event(const char *name)
{
    printf("%s{\"service\":\"%s\"", first_event ? "" : ",", name);
    first_event = 0;
}

int Voice_SendVoiceData(void)
{
    event("Voice_SendVoiceData");
    printf("}");
    return send_enabled;
}

int IsTalking(void)
{
    event("IsTalking");
    printf("}");
    return talking;
}

void CL_VoiceTransmit(void)
{
    event("CL_VoiceTransmit");
    printf("}");
}

int Encode_GetFrameSize(void)
{
    event("Encode_GetFrameSize");
    printf("}");
    return frame_size;
}

void Com_Printf(const char *format, ...)
{
    (void)format;
    abort();
}

int Encode_Sample(short *in, char *out, int avail)
{
    event("Encode_Sample");
    printf(",\"avail\":%d,\"out_offset\":%td,\"pcm\":[", avail, out-enc_buffer);
    for(int i=0;i<frame_size;i++) printf("%s%d", i ? "," : "", in[i]);
    printf("]}");
    assert(avail >= encoded_size);
    for(int i=0;i<encoded_size;i++) out[i]=(char)(0x51+i);
    return encoded_size;
}

int Client_SendVoiceData(int n, char *p)
{
    event("Client_SendVoiceData");
    printf(",\"length\":%d,\"offset\":%td,\"bytes\":[", n, p-enc_buffer);
    for(int i=0;i<n;i++) printf("%s%u", i ? "," : "", (unsigned char)p[i]);
    printf("]}"); return network_result;
}

static int value(int i)
{
    return current_id < 9 ? i + 1 : ((i * 73 + 19) % 1801) - 900;
}

static unsigned fbits(float f)
{
    unsigned u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

int main(int argc, char **argv)
{
    assert(argc==2);
    int exact=atoi(argv[1]);
    assert(fesetround(FE_TONEAREST)==0);
    unsigned long fpcr; __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    assert(fpcr==0);
    int id,count,initial,scale_code,initial_offset,position;
    while(scanf("%d %d %d %d %d %d %d %d %d %d %d", &id,&count,&initial,&scale_code,
          &send_enabled,&talking,&frame_size,&encoded_size,&network_result,&position,&initial_offset)==11) {
        current_id=id;
        int capacity=exact ? count : 1024;
        int prefix=exact ? 0 : 4;
        short *owned=malloc((capacity+(exact?0:8))*sizeof(short)); assert(owned);
        short *pcm=owned+prefix;
        for(int i=0;i<capacity;i++) pcm[i]=(short)value(i);
        if(!exact) for(int i=0;i<4;i++) { owned[i]=(short)(12000+i); pcm[capacity+i]=(short)(13000+i); }
        for(int i=0;i<640;i++) partial_audio_buffer[i]=(short)(i<initial ? -3000+i : 7000+i);
        memset(enc_buffer,0x35,sizeof(enc_buffer));
        enc_buffer_pos=position; samples_in_partial_audio_buffer=initial;
        voice_current_scaler=scale_code / 4.0f; voice_current_voicelevel=123.25f;
        struct { uint64_t pre; audioSample_t sample; uint64_t post; } wrapped;
        memset(&wrapped,0xa5,sizeof(wrapped)); wrapped.pre=0x1122334455667788ULL;wrapped.post=0x8877665544332211ULL;
        audioSample_t *s=&wrapped.sample;
        s->buffer=(unsigned char*)pcm; s->lengthInBytes=count*2; s->lengthInSamples=count;
        s->bytesPerSample=2;s->frequency=8192;s->stereo=0;s->channels=1;s->sampleOffset=initial_offset;
        printf("{\"id\":%d,\"events\":[",id);first_event=1;
        unsigned long before_cr, after_cr, after_sr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(before_cr));
        int result=Record_QueueAudioDataForEncoding(s);
        __asm__ volatile("mrs %0, fpcr\nmrs %1, fpsr" : "=r"(after_cr), "=r"(after_sr));
        assert(before_cr == 0 && after_cr == before_cr);
        assert(wrapped.pre==0x1122334455667788ULL && wrapped.post==0x8877665544332211ULL);
        assert(s->buffer==(unsigned char*)pcm && s->lengthInBytes==count*2 && s->lengthInSamples==count);
        assert(s->bytesPerSample==2 && s->frequency==8192 && s->stereo==0 && s->channels==1);
        printf("],\"result\":%d,\"offset\":%d,\"partial_count\":%d,\"volume_bits\":%u,\"scale_bits\":%u,\"enc_pos\":%d,\"fpcr\":%lu,\"fpcr_before\":%lu,\"fpsr_after\":%lu,\"pcm\":[",result,s->sampleOffset,samples_in_partial_audio_buffer,fbits(voice_current_voicelevel),fbits(voice_current_scaler),enc_buffer_pos,after_cr,before_cr,after_sr);
        for(int i=0;i<capacity;i++) printf("%s%d",i?",":"",pcm[i]);
        printf("],\"partial\":[");
        for(int i=0;i<640;i++) printf("%s%d",i?",":"",partial_audio_buffer[i]);
        printf("],\"encoded_changes\":[");int first=1;
        for(int i=0;i<4096;i++) if((unsigned char)enc_buffer[i]!=0x35) {
            printf("%s[%d,%u]",first?"":",",i,(unsigned char)enc_buffer[i]);first=0;
        }
        printf("],\"guards_preserved\":true,\"layout\":[%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu]}\n",sizeof(*s),offsetof(audioSample_t,buffer),offsetof(audioSample_t,lengthInBytes),offsetof(audioSample_t,lengthInSamples),offsetof(audioSample_t,bytesPerSample),offsetof(audioSample_t,frequency),offsetof(audioSample_t,stereo),offsetof(audioSample_t,channels),offsetof(audioSample_t,sampleOffset));
        if(!exact) for(int i=0;i<4;i++) assert(owned[i]==12000+i && pcm[capacity+i]==13000+i);
        free(owned);
    }
    return 0;
}
