#include "internal/cwav_env.h"
#include "3ds.h"
#include <string.h>
#include <stdlib.h>

#ifdef CWAV_DISABLE_DSP
#ifdef CWAV_DISABLE_CSND
#error "Cannot disable DSP and CSND at the same time!"
#endif
#endif

#ifdef CWAV_DISABLE_DSP
#pragma message "DSP service support has been disabled!"
#endif
#ifdef CWAV_DISABLE_CSND
#pragma message "CSND service support has been disabled!"
#endif

#if !defined CWAV_DISABLE_DSP && defined CWAV_DISABLE_CSND
static const u32 g_currentEnv = CWAV_ENV_DSP;
#elif defined CWAV_DISABLE_DSP && !defined CWAV_DISABLE_CSND
static const u32 g_currentEnv = CWAV_ENV_CSND;
#else
static u32 g_currentEnv = CWAV_ENV_DSP;
#endif

#ifndef CWAV_DISABLE_DSP
static ndspWaveBuf* g_ndspWaveBuffers = NULL;
#endif

#ifndef CWAV_DISABLE_CSND
u32 cwav_defaultVAToPA(const void* addr)
{
    return osConvertVirtToPhys(addr);
}
vaToPaCallback_t cwavCurrentVAPAConvCallback = cwav_defaultVAToPA;
#else
vaToPaCallback_t cwavCurrentVAPAConvCallback = NULL;
#endif

#ifndef CWAV_DISABLE_CSND
static Result csndPlaySoundFixed(int chn, u32 flags, u32 sampleRate, float vol, float pan, float pitch, void* data0, void* data1, u32 size)
{
    if (!(csndChannels & BIT(chn)))
        return 1;

    u32 paddr0 = 0, paddr1 = 0;

    int encoding = (flags >> 12) & 3;
    int loopMode = (flags >> 10) & 3;

    if (!loopMode)
        flags |= SOUND_ONE_SHOT;

    if (encoding != CSND_ENCODING_PSG)
    {
        if (data0)
            paddr0 = cwavCurrentVAPAConvCallback(data0);
        if (data1)
            paddr1 = cwavCurrentVAPAConvCallback(data1);
    }

    u32 timer = CSND_TIMER((u32)(sampleRate * pitch));
    if (timer < 0x0042) 
        timer = 0x0042;
    else if (timer > 0xFFFF) 
        timer = 0xFFFF;
    flags &= ~0xFFFF001F;
    flags |= SOUND_ENABLE | SOUND_CHANNEL(chn) | (timer << 16);

    u32 volumes = CSND_VOL(vol, pan);
    CSND_SetChnRegs(flags, paddr0, paddr1, size, volumes, volumes);

    if (loopMode == CSND_LOOPMODE_NORMAL && paddr1 > paddr0)
    {
        // Now that the first block is playing, configure the size of the subsequent blocks
        size -= paddr1 - paddr0;
        CSND_SetBlock(chn, 1, paddr1, size);
    }

    return csndExecCmds(true);
}
#endif

void cwavEnvUseEnvironment(cwavEnvMode_t envMode)
{
#ifndef CWAV_DISABLE_CSND
#ifndef CWAV_DISABLE_DSP
    g_currentEnv = envMode;
#endif
#endif
}

cwavEnvMode_t cwavEnvGetEnvironment()
{
    return g_currentEnv;
}

void cwavEnvInitialize()
{

    if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        g_ndspWaveBuffers = malloc(sizeof(ndspWaveBuf) * 24 * 2);
        memset(g_ndspWaveBuffers, 0, sizeof(ndspWaveBuf) * 24 * 2);
#endif
    }
}

void cwavEnvFinalize()
{
    if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        if (g_ndspWaveBuffers)
            free(g_ndspWaveBuffers);
        g_ndspWaveBuffers = NULL;
#endif
    }
}

#ifndef CWAV_DISABLE_DSP
static inline ndspWaveBuf* cwavEnvGetNdspWaveBuffer(u32 channel, u32 block)
{
    return &g_ndspWaveBuffers[channel * 2 + block];
}
#endif

bool cwavEnvCompatibleEncoding(cwavEncoding_t encoding)
{
    if (encoding == PCM8 || encoding == PCM16)
        return true;

#ifndef CWAV_DISABLE_CSND
    if (encoding == IMA_ADPCM && g_currentEnv == CWAV_ENV_CSND)
        return true;
#endif

#ifndef CWAV_DISABLE_DSP
    if (encoding == DSP_ADPCM && g_currentEnv == CWAV_ENV_DSP)
        return true;
#endif

    return false;
}

u32 cwavEnvGetChannelAmount()
{
    if (g_currentEnv == CWAV_ENV_CSND) 
    {
#ifndef CWAV_DISABLE_CSND
        return CSND_NUM_CHANNELS;
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        return 24;
#endif
    }
    return 0;
}

bool cwavEnvIsChannelAvailable(u32 channel) 
{
    if (g_currentEnv == CWAV_ENV_CSND) 
    {
#ifndef CWAV_DISABLE_CSND
        return ((csndChannels >> channel) & 1);
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        return true;
#endif
    }
    return false;
}

void cwavEnvSetADPCMState(cwav_t* cwav, u32 channel)
{
    if (g_currentEnv == CWAV_ENV_CSND) 
    {
#ifndef CWAV_DISABLE_CSND
        CSND_SetAdpcmState(cwav->playingChanIds[cwav->currMultiplePlay][channel], 0, cwav->IMAADPCMInfos[channel]->context.data, cwav->IMAADPCMInfos[channel]->context.tableIndex);

        if (cwav->cwavInfo->isLooped)
        {
            CSND_SetAdpcmState(cwav->playingChanIds[cwav->currMultiplePlay][channel], 1, cwav->IMAADPCMInfos[channel]->loopContext.data, cwav->IMAADPCMInfos[channel]->loopContext.tableIndex);
        }
        else
        {
            CSND_SetAdpcmState(cwav->playingChanIds[cwav->currMultiplePlay][channel], 1, cwav->IMAADPCMInfos[channel]->context.data, cwav->IMAADPCMInfos[channel]->context.tableIndex);
        }
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        ndspChnSetAdpcmCoefs(cwav->playingChanIds[cwav->currMultiplePlay][channel], cwav->DSPADPCMInfos[channel]->param.coefs);

        ndspWaveBuf* block0Buff = cwavEnvGetNdspWaveBuffer(cwav->playingChanIds[cwav->currMultiplePlay][channel], 0);
        ndspWaveBuf* block1Buff = cwavEnvGetNdspWaveBuffer(cwav->playingChanIds[cwav->currMultiplePlay][channel], 1);      

        if (cwav->cwavInfo->isLooped)
        {
            block0Buff->adpcm_data = (ndspAdpcmData*)&cwav->DSPADPCMInfos[channel]->context;
            block1Buff->adpcm_data = (ndspAdpcmData*)&cwav->DSPADPCMInfos[channel]->loopContext;
        }
        else
        {
            block1Buff->adpcm_data = (ndspAdpcmData*)&cwav->DSPADPCMInfos[channel]->context;
        }
#endif
    }
}

void cwavEnvPlay(u32 channel, bool isLooped, cwavEncoding_t encoding, u32 sampleRate, float volume, float pan, float pitch, void* block0, void* block1, u32 loopStart, u32 loopEnd, u32 totalSize)
{
    if (g_currentEnv == CWAV_ENV_CSND)
    {
#ifndef CWAV_DISABLE_CSND
        u32 encFlag = 0;
        switch (encoding)
        {
        case PCM8:
            encFlag = SOUND_FORMAT_8BIT;
            break;
        case PCM16:
            encFlag = SOUND_FORMAT_16BIT;
            break;
        case IMA_ADPCM:
            encFlag = SOUND_FORMAT_ADPCM;
            break;
        default:
            break;
        }
        
        csndPlaySoundFixed(channel, (isLooped ? SOUND_REPEAT : SOUND_ONE_SHOT) | encFlag, sampleRate, volume, pan, pitch, block0, block1, totalSize);
        csndExecCmds(true);
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        u32 encFlag = 0;
        switch (encoding)
        {
        case PCM8:
            encFlag = NDSP_FORMAT_PCM8;
            break;
        case PCM16:
            encFlag = NDSP_FORMAT_PCM16;
            break;
        case DSP_ADPCM:
            encFlag = NDSP_FORMAT_ADPCM;
            break;
        default:
            break;
        }

        ndspWaveBuf* block0Buff = cwavEnvGetNdspWaveBuffer(channel, 0);
        ndspWaveBuf* block1Buff = cwavEnvGetNdspWaveBuffer(channel, 1);

        float mix[12] = {0};
        float rightPan = (pan + 1.f) / 2.f;
        float leftPan = 1.f - rightPan;
        mix[0] = 0.8f * leftPan * volume; // Left front
        mix[2] = 0.2f * leftPan * volume; // Left back
        mix[1] = 0.8f * rightPan * volume; // Right front
        mix[3] = 0.2f * rightPan * volume; // Right back

        ndspChnSetFormat(channel, encFlag);
        ndspChnSetRate(channel, (float)(sampleRate) * pitch);
        ndspChnSetMix(channel, mix);

        block1Buff->data_vaddr = isLooped ? block1 : block0;
        block1Buff->nsamples = loopEnd - loopStart;
        block1Buff->looping = isLooped;

        if (isLooped)
        {
            block0Buff->data_vaddr = block0;
            block0Buff->nsamples = loopStart;
            block0Buff->looping = false;
            
            ndspChnWaveBufAdd(channel, block0Buff);
        }

        ndspChnWaveBufAdd(channel, block1Buff);
#endif
    }
}

bool cwavEnvChannelIsPlaying(u32 channel) 
{
    if (g_currentEnv == CWAV_ENV_CSND)
    {
#ifndef CWAV_DISABLE_CSND
        u8 stat = 0;
        csndIsPlaying(channel, &stat);
        csndExecCmds(true);
        return stat;
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        ndspWaveBuf* block0Buff = cwavEnvGetNdspWaveBuffer(channel, 0);
        ndspWaveBuf* block1Buff = cwavEnvGetNdspWaveBuffer(channel, 1);

        return (block0Buff->status == NDSP_WBUF_QUEUED || block0Buff->status == NDSP_WBUF_PLAYING ||
                block1Buff->status == NDSP_WBUF_QUEUED || block1Buff->status == NDSP_WBUF_PLAYING);
#endif
    }
    return false;
}

void cwavEnvStop(u32 channel)
{
    if (g_currentEnv == CWAV_ENV_CSND)
    {
#ifndef CWAV_DISABLE_CSND
        CSND_SetPlayState(channel, 0);
        csndExecCmds(true);
#endif
    }
    else if (g_currentEnv == CWAV_ENV_DSP)
    {
#ifndef CWAV_DISABLE_DSP
        ndspChnReset(channel);
        ndspWaveBuf* block0Buff = cwavEnvGetNdspWaveBuffer(channel, 0);
        ndspWaveBuf* block1Buff = cwavEnvGetNdspWaveBuffer(channel, 1);
        block0Buff->status = block1Buff->status = NDSP_WBUF_FREE;
#endif
    }
}