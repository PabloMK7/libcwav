#include "cwav.h"
#include "internal/cwav_defs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CWAVTOIMPL(c) ((cwav_t*)c->cwav)

static u32 cwav_defaultVAToPA(const void* addr) { return osConvertVirtToPhys(addr); }
static vaToPaCallback_t cwavCurrentVAPAConvCallback = cwav_defaultVAToPA;

static u32 cwavAddedToList = 0;
static u32 cwavListCount = 0;
static CWAV** cwavList = NULL;

static Result csndPlaySoundFixed(int chn, u32 flags, u32 sampleRate, float vol, float pan, void* data0, void* data1, u32 size)
{
    if (!(csndChannels & BIT(chn)))
        return 1;

    u32 paddr0 = 0, paddr1 = 0;

    int encoding = (flags >> 12) & 3;
    int loopMode = (flags >> 10) & 3;

    if (!loopMode) flags |= SOUND_ONE_SHOT;

    if (encoding != CSND_ENCODING_PSG)
    {
        if (data0) paddr0 = cwavCurrentVAPAConvCallback(data0);
        if (data1) paddr1 = cwavCurrentVAPAConvCallback(data1);
    }

    u32 timer = CSND_TIMER(sampleRate);
    if (timer < 0x0042) timer = 0x0042;
    else if (timer > 0xFFFF) timer = 0xFFFF;
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

static void cwav_Register(CWAV* cwav) {
    if (cwavListCount == 0) {
        cwavList = malloc( 8 * sizeof(CWAV*));
        memset(cwavList, 0, 8 * sizeof(CWAV*));
        cwavListCount = 8;
    }
    while (true) {
        for (int i = 0; i < cwavListCount; i++) {
            if (cwavList[i] == NULL) {
                cwavList[i] = cwav;
                cwavAddedToList++;
                return;
            }
        }
        cwavList = realloc(cwavList, cwavListCount * 2 * sizeof(CWAV*));
        memset(cwavList + cwavListCount, 0, cwavListCount * sizeof(CWAV*));
        cwavListCount *= 2;
    }
}

static void cwav_DeRegister(CWAV* cwav) {
    for (int i = 0; i < cwavListCount; i++) {
        if (cwavList[i] == cwav) {
            cwavList[i] = NULL;
            cwavAddedToList--;
            for (int j = i; j < cwavListCount - 1 && cwavList[j+1] != NULL; j++)
                cwavList[j] = cwavList[j+1];
            break;
        }
    }
    if (!cwavAddedToList) {
        cwavListCount = 0;
        free(cwavList);
        cwavList = NULL;
    }
}

static void cwav_UpdatePlayingStatus() {
    for (int i = 0; i < cwavListCount && cwavList[i] != NULL; i++) {
        cwav_t* currCwav = CWAVTOIMPL(cwavList[i]);
        for (int j = 0; j < currCwav->totalMultiplePlay; j++) {
            for (int k = 0; k < currCwav->channelcount; k++) {
                if (currCwav->playingChanIds[j][k] == -1)
                    continue;
                u8 stat = 0;
                Result res = csndIsPlaying(currCwav->playingChanIds[j][k], &stat);
                csndExecCmds(true);
                if (!stat || R_FAILED(res)) currCwav->playingChanIds[j][k] = -1;
            }
        }
    }
}

static u32 cwav_parseInfoBlock(cwav_t* cwav) {
    u32 infoSize = cwav->cwavHeader->info_blck.size;
    cwav->cwavInfo = (cwavInfoBlock_t *)((u32)(cwav->fileBuf) + cwav->cwavHeader->info_blck.ref.offset);
    if (cwav->cwavInfo->header.magic != 0x4F464E49 || cwav->cwavInfo->header.size != infoSize)
        return CWAV_INVAID_INFO_BLOCK; //magic INFO or size mismatch

    cwav->channelcount = cwav->cwavInfo->channelInfoRefs.count;

    u32 encoding = cwav->cwavInfo->encoding;
    if (encoding != IMA_ADPCM && encoding != PCM8 && encoding != PCM16)
        return CWAV_UNSUPPORTED_AUDIO_ENCODING; //Only IMA ADPCM and PCM8/16 supported for now
    
    cwav->channelInfos = (cwavchannelInfo_t**)malloc(4 * cwav->channelcount);

    for (int i = 0; i < cwav->channelcount; i++) {
        cwavReference_t *currRef = &(cwav->cwavInfo->channelInfoRefs.references[i]);
        if (currRef->refType != CHANNEL_INFO) {
            return CWAV_INVAID_INFO_BLOCK;
        }
        cwav->channelInfos[i] = (cwavchannelInfo_t*)((u8*)(&(cwav->cwavInfo->channelInfoRefs.count)) + currRef->offset);
        if (cwav->channelInfos[i]->samples.refType != SAMPLE_DATA) return CWAV_INVAID_INFO_BLOCK;
    }
    if (encoding == IMA_ADPCM) {
        cwav->IMAADPCMInfos = (cwavIMAADPCMInfo_t**)malloc(4 * cwav->channelcount);
        for (int i = 0; i < cwav->channelcount; i++) {
            if (cwav->channelInfos[i]->ADPCMInfo.refType != IMA_ADPCM_INFO) return CWAV_INVAID_INFO_BLOCK;
            cwav->IMAADPCMInfos[i] = (cwavIMAADPCMInfo_t*)(cwav->channelInfos[i]->ADPCMInfo.offset + (u8*)(&(cwav->channelInfos[i]->samples)));
        }
    }
    return CWAV_SUCCESS;
}

static void cwav_initialize(CWAV* out, u8 maxSPlays) {
    cwav_t* cwav = CWAVTOIMPL(out);

    out->monoPan = 0.f;
    out->volume = 1.f;

    if (maxSPlays == 0) {
        out->loadStatus = CWAV_INVALID_ARGUMENT;
        return;
    }

    cwav->cwavHeader = (cwavHeader_t*)cwav->fileBuf;
    if (cwav->cwavHeader->magic != 0x56415743 || cwav->cwavHeader->endian != 0xFEFF || cwav->cwavHeader->version != 0x02010000 || cwav->cwavHeader->blockCount != 2) {
        out->loadStatus = CWAV_UNKNOWN_FILE_FORMAT;
        return;
    }

    cwavLoadStatus_t ret = cwav_parseInfoBlock(cwav); 
    if (ret != CWAV_SUCCESS) {
        out->loadStatus = ret;
        return;
    }

    cwav->cwavData = (cwavDataBlock_t*)((u32)(cwav->fileBuf) + cwav->cwavHeader->data_blck.ref.offset); 
    if (cwav->cwavData->header.magic != 0x41544144) {
        out->loadStatus = CWAV_INVAID_DATA_BLOCK;
        return;
    }

    cwav->totalMultiplePlay = maxSPlays;
    cwav->playingChanIds = (int**)malloc(cwav->totalMultiplePlay * sizeof(int*));
    for (int i = 0; i < cwav->totalMultiplePlay; i++) {
        cwav->playingChanIds[i] = (int*)malloc(cwav->channelcount * sizeof(int));
        for (int j = 0; j < cwav->channelcount; j++) cwav->playingChanIds[i][j] = -1;
    }
    
    cwav->currMultiplePlay = 0;
    out->numChannels = cwav->channelcount;
    out->isLooped = cwav->cwavInfo->isLooped;

    cwav_Register(out);
    out->loadStatus = CWAV_SUCCESS;
}

static void cwav_stopImpl(cwav_t* cwav, int leftChannel, int rightChannel, u8 multipleID) {
    if (!cwav) return;

    if (multipleID > cwav->totalMultiplePlay)
        return;

    if (leftChannel >= 0 && leftChannel < (int)cwav->channelcount) {
        if (cwav->playingChanIds[multipleID][leftChannel] != -1) {
            CSND_SetPlayState(cwav->playingChanIds[multipleID][leftChannel], 0);
            cwav->playingChanIds[multipleID][leftChannel] = -1;
        }
    }
    if (rightChannel >= 0 && rightChannel < (int)cwav->channelcount) {
        if (cwav->playingChanIds[multipleID][rightChannel] != -1) {
            CSND_SetPlayState(cwav->playingChanIds[multipleID][rightChannel], 0);
            cwav->playingChanIds[multipleID][rightChannel] = -1;
        }
    }
    if (leftChannel < 0 && rightChannel < 0) {
        for (u32 i = 0; i < cwav->channelcount; i++) {
            if (cwav->playingChanIds[multipleID][i] != -1) {
                CSND_SetPlayState(cwav->playingChanIds[multipleID][i], 0);
                cwav->playingChanIds[multipleID][i] = -1;
            }
        }
    }
    csndExecCmds(true);
}

static void cwav_libAptHook(APT_HookType hook, void* param) {
    cwavNotifyAptEvent(hook);
}

void cwavNotifyAptEvent(APT_HookType event) {
    switch (event)
    {
    case APTHOOK_ONSUSPEND:
    case APTHOOK_ONEXIT:
    case APTHOOK_ONSLEEP:
        for (int i = 0; i < cwavListCount && cwavList[i] != NULL; i++)
            cwavStop(cwavList[i], -1, -1);
        break;
    default:
        break;
    }
}

void cwavDoAptHook() {
    static aptHookCookie cookie;
    static bool hooked = false;
    if (!hooked) {
        aptHook(&cookie, cwav_libAptHook, NULL);
        hooked = true;
    }
}

void cwavSetVAToPACallback(vaToPaCallback_t callback) {
    if (callback != NULL)
        cwavCurrentVAPAConvCallback = callback;
    else
        cwavCurrentVAPAConvCallback = cwav_defaultVAToPA;
}

void cwavLoad(CWAV* out, const void* bcwavFileBuffer, u8 maxSPlays) {
    if (!out) return;
    cwav_t* cwav = malloc(sizeof(cwav_t));
    out->cwav = cwav;
    memset(cwav, 0, sizeof(cwav_t));

    if (bcwavFileBuffer == NULL) {
        out->loadStatus = CWAV_INVALID_ARGUMENT;
        return;
    }

    cwav->fileBuf = (void*)bcwavFileBuffer;

    cwav_initialize(out, maxSPlays);
}

void cwavFree(CWAV* cwav) {
    if (!cwav) return;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);
    if (cwav_) {
        if (cwav->loadStatus == CWAV_SUCCESS) {
            cwavStop(cwav, -1, -1);
            cwav_DeRegister(cwav);

            for (int i = 0; i < cwav_->totalMultiplePlay; i++)
                free(cwav_->playingChanIds[i]);
            
            free(cwav_->playingChanIds);
        }
        if (cwav_->channelInfos) free(cwav_->channelInfos);
        if (cwav_->IMAADPCMInfos) free(cwav_->IMAADPCMInfos);
        free(cwav_);
        cwav_ = NULL;
    }
    cwav->loadStatus = CWAV_NOT_ALLOCATED;
}

#ifdef DIRECT_SOUND_IMPLEMENTED
bool cwavPlayAsDirectSound(CWAV* cwav, int leftChannel, int rightChannel, CSND_DirectSoundModifiers* soundModifiers) {
    if (!cwav || cwav->loadStatus != CWAV_SUCCESS) return false;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);
    CSND_DirectSound dirSound;

    csndInitializeDirectSound(&dirSound);

    u32 channelCount = 0;
    if (leftChannel >= 0 && leftChannel < (int)cwav_->channelcount && rightChannel >= 0 && rightChannel < (int)cwav_->channelcount)
        channelCount = 2;
    else if (leftChannel >= 0 && leftChannel < (int)cwav_->channelcount)
        channelCount = 1;
    else
        return false;
    
    u32 encoding = cwav_->cwavInfo->encoding;
    u32 encFlag = 0;
    u32 size = 0;
    if (cwav_->cwavInfo->isLooped)
        return false;
    
    u8* leftSampleData = (u8*)((u32)cwav_->channelInfos[leftChannel]->samples.offset + (u8*)(&(cwav_->cwavData->data)));
    u8* rightSampleData = 0;
    if (channelCount == 2)
        rightSampleData = (u8*)((u32)cwav_->channelInfos[rightChannel]->samples.offset + (u8*)(&(cwav_->cwavData->data)));

    switch (encoding)
    {
    case IMA_ADPCM:
        encFlag = CSND_ENCODING_ADPCM;
        size = (cwav_->cwavInfo->LoopEnd / 2);

        memcpy(&dirSound.channelData.adpcmContext[0], &cwav_->IMAADPCMInfos[leftChannel]->context, sizeof(CSND_DirectSoundIMAADPCMContext));
        if (channelCount == 2)
            memcpy(&dirSound.channelData.adpcmContext[1], &cwav_->IMAADPCMInfos[rightChannel]->context, sizeof(CSND_DirectSoundIMAADPCMContext));
        
        break;
    case PCM8:
        encFlag = CSND_ENCODING_PCM8;
        size = (cwav_->cwavInfo->LoopEnd);
        break;
    case PCM16:
        encFlag = CSND_ENCODING_PCM16;
        size = (cwav_->cwavInfo->LoopEnd * 2);
        break;
    default:
        break;
    }

    soundModifiers->channelVolumes[0] = soundModifiers->channelVolumes[0] * cwav->volume;
    soundModifiers->channelVolumes[1] = soundModifiers->channelVolumes[1] * cwav->volume;

    memcpy(&dirSound.soundModifiers, soundModifiers, sizeof(CSND_DirectSoundModifiers));

    dirSound.channelData.channelAmount = channelCount;
    dirSound.channelData.channelEncoding = encFlag;
    dirSound.channelData.sampleRate = cwav_->cwavInfo->sampleRate;
    dirSound.channelData.sampleDataLength = size;
    if (leftSampleData) dirSound.channelData.sampleData[0] = (void*)cwavCurrentVAPAConvCallback(leftSampleData);
    if (rightSampleData) dirSound.channelData.sampleData[1] = (void*)cwavCurrentVAPAConvCallback(rightSampleData);

    return R_SUCCEEDED(csndPlayDirectSound(&dirSound, false));
}
#endif

bool cwavPlay(CWAV* cwav, int leftChannel, int rightChannel) {
    if (!cwav || cwav->loadStatus != CWAV_SUCCESS) return false;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);

    bool stereo = true;
    if (rightChannel < 0) {
        stereo = false;
    }
    if (leftChannel < 0 || leftChannel >= (int)(cwav_->channelcount) || rightChannel >= (int)(cwav_->channelcount)) {
        return false;
    }

    cwav_UpdatePlayingStatus();

    cwav_->currMultiplePlay++;
    if (cwav_->currMultiplePlay >= cwav_->totalMultiplePlay)
        cwav_->currMultiplePlay = 0;
    
    cwav_stopImpl(cwav_, leftChannel, rightChannel, cwav_->currMultiplePlay);

    int prevchan = -1;
    for (int i = 0; i < ((stereo) ? 2 : 1); i++) {

        cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel] = -1;
        for (int j = 0; j < CSND_NUM_CHANNELS; j++) {
            if (!((csndChannels >> j) & 1)) continue;
            u8 stat = 0;
            csndIsPlaying(j, &stat);
            csndExecCmds(true);
            if (stat || j == prevchan) continue;
            cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel] = j;
            break;
        }
        if (cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel] == -1) {
            return false;
        }

        prevchan = cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel];

        u8* block0 = NULL;
        u8* block1 = NULL;
        u32 size = 0;
        u32 encoding = cwav_->cwavInfo->encoding;
        u32 encFlag = 0;

        block0 = (u8*)((u32)cwav_->channelInfos[i ? rightChannel : leftChannel]->samples.offset + (u8*)(&(cwav_->cwavData->data)));

        switch (encoding)
        {
        case IMA_ADPCM:
            encFlag = SOUND_FORMAT_ADPCM;
            size = (cwav_->cwavInfo->LoopEnd / 2);

            CSND_SetAdpcmState(cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel], 0, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.tableIndex);

            if (cwav_->cwavInfo->isLooped) {
                block1 = ((cwav_->cwavInfo->loopStart) / 2) + block0;
                CSND_SetAdpcmState(cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel], 1, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->loopContext.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->loopContext.tableIndex);
            } else {
                block1 = block0;
                CSND_SetAdpcmState(cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel], 1, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.tableIndex);
            }
            
            break;
        case PCM8:
            encFlag = SOUND_FORMAT_8BIT;

            size = (cwav_->cwavInfo->LoopEnd);
            if (cwav_->cwavInfo->isLooped) {
                block1 = (cwav_->cwavInfo->loopStart) + block0;
            } else {
                block1 = block0;
            }

            break;
        case PCM16:
            encFlag = SOUND_FORMAT_16BIT;

            size = (cwav_->cwavInfo->LoopEnd * 2);
            if (cwav_->cwavInfo->isLooped) {
                block1 = ((cwav_->cwavInfo->loopStart) * 2) + block0;
            } else {
                block1 = block0;
            }

            break;
        default:
            break;
        }
        
        csndExecCmds(true);

        float pan = 0.f;
        float volume = cwav->volume;
        if (stereo) {
            if (i == 0)
            {
                pan = -1.0f;
            }
            else 
            {
                pan = 1.0f;
            }
        } else {
            pan = cwav->monoPan;
        }
        
        

        csndPlaySoundFixed(cwav_->playingChanIds[cwav_->currMultiplePlay][i ? rightChannel : leftChannel], (cwav_->cwavInfo->isLooped ? SOUND_REPEAT : SOUND_ONE_SHOT) | encFlag, cwav_->cwavInfo->sampleRate, volume, pan, block0, block1, size);
        csndExecCmds(true);
    }
    return true;
}

void cwavStop(CWAV* cwav, int leftChannel, int rightChannel) {
    if (!cwav || cwav->loadStatus != CWAV_SUCCESS) return;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);
    for (int i = 0; i < cwav_->totalMultiplePlay; i++)
        cwav_stopImpl(cwav_, leftChannel, rightChannel, i);
}