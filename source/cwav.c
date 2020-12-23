#include "cwav.h"
#include "internal/cwav_defs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CWAVTOIMPL(c) ((cwav_t*)c->cwav)

static vaToPaCallback_t cwavCurrentVAPAConvCallback = osConvertVirtToPhys;

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

static u32 cwav_parseInfoBlock(cwav_t* cwav) {
    u32 infoSize = cwav->cwavHeader->info_blck.size;
    cwav->cwavInfo = (cwavInfoBlock_t *)((u32)(cwav->fileBuf) + cwav->cwavHeader->info_blck.ref.offset);
    if (cwav->cwavInfo->header.magic != 0x4F464E49 || cwav->cwavInfo->header.size != infoSize)
        return CWAV_INVAID_INFO_BLOCK; //magic INFO or size mismatch

    cwav->channelcount = cwav->cwavInfo->channelInfoRefs.count;

    u32 encoding = cwav->cwavInfo->encoding;
    if (encoding != IMA_ADPCM)
        return CWAV_UNSUPPORTED_AUDIO_ENCODING; //Only IMA ADPCM supported for now
    
    cwav->channelInfos = (cwavchannelInfo_t**)malloc(4 * cwav->channelcount);

    for (int i = 0; i < cwav->channelcount; i++) {
        cwavReference_t *currRef = &(cwav->cwavInfo->channelInfoRefs.references[i]);
        if (currRef->refType != CHANNEL_INFO) {
            return CWAV_INVAID_INFO_BLOCK;
        }
        cwav->channelInfos[i] = (cwavchannelInfo_t*)((u8*)(&(cwav->cwavInfo->channelInfoRefs.count)) + currRef->offset);
        if (cwav->channelInfos[i]->samples.refType != SAMPLE_DATA) return CWAV_INVAID_INFO_BLOCK;
    }

    cwav->IMAADPCMInfos = (cwavIMAADPCMInfo_t**)malloc(4 * cwav->channelcount);
    for (int i = 0; i < cwav->channelcount; i++) {
        if (cwav->channelInfos[i]->ADPCMInfo.refType != IMA_ADPCM_INFO) return CWAV_INVAID_INFO_BLOCK;
        cwav->IMAADPCMInfos[i] = (cwavIMAADPCMInfo_t*)(cwav->channelInfos[i]->ADPCMInfo.offset + (u8*)(&(cwav->channelInfos[i]->samples)));
    }

    return CWAV_SUCCESS;
}

void cwavSetVAToPACallback(vaToPaCallback_t callback) {
    if (callback != NULL)
        cwavCurrentVAPAConvCallback = callback;
    else
        cwavCurrentVAPAConvCallback = osConvertVirtToPhys;
}

CWAV* cwavLoadFromFile(const char* filename) {

    CWAV* out = malloc(sizeof(CWAV));
    cwav_t* cwav = malloc(sizeof(cwav_t));

    out->monoPan = 0.f;
    out->volume = 1.f;
    out->cwav = cwav;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        out->loadStatus = CWAV_FILE_OPEN_FAILED;
        return out;
    }

    memset(cwav, 0, sizeof(cwav_t));

    fseek(file, 0, SEEK_END);
    u32 fileSize = ftell(file);
    cwav->fileBuf = linearAlloc(fileSize);
    if (!cwav->fileBuf) {
        fclose(file);
        out->loadStatus = CWAV_FILE_TOO_LARGE;
        return out;
    }

    fseek(file, 0, SEEK_SET); 
    fread(cwav->fileBuf, 1, fileSize, file);
    fclose(file);

    cwav->cwavHeader = (cwavHeader_t*)cwav->fileBuf;
    if (cwav->cwavHeader->magic != 0x56415743 || cwav->cwavHeader->endian != 0xFEFF || cwav->cwavHeader->version != 0x02010000 || cwav->cwavHeader->blockCount != 2) {
        out->loadStatus = CWAV_UNKNOWN_FILE_FORMAT;
        return out;
    }

    u32 ret = cwav_parseInfoBlock(cwav); 
    if (ret) {
        out->loadStatus = ret;
        return out;
    }

    cwav->cwavData = (cwavDataBlock_t*)((u32)(cwav->fileBuf) + cwav->cwavHeader->data_blck.ref.offset); 
    if (cwav->cwavData->header.magic != 0x41544144) {
        out->loadStatus = CWAV_INVAID_DATA_BLOCK;
        return out;
    }

    cwav->playingchanids = (int*)malloc(cwav->channelcount * 4);
    for (int i = 0; i < cwav->channelcount; i++) cwav->playingchanids[i] = -1;

    out->numChannels = cwav->channelcount;
    out->isLooped = cwav->cwavInfo->isLooped;

    out->loadStatus = CWAV_SUCCESS;
    return out;
}

bool cwavPlay(CWAV* cwav, int leftChannel, int rightChannel) {
    if (!cwav) return false;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);

    bool stereo = true;
    if (rightChannel < 0) {
        stereo = false;
    }
    if (leftChannel < 0 || leftChannel >= (int)(cwav_->channelcount) || rightChannel >= (int)(cwav_->channelcount)) {
        return false;
    }

    cwavStop(cwav, leftChannel, rightChannel);

    int prevchan = -1;
    for (int i = 0; i < ((stereo) ? 2 : 1); i++) {

        for (int j = 0; j < CSND_NUM_CHANNELS; j++) {
            if (!((csndChannels >> j) & 1)) continue;
            u8 stat = 0;
            csndIsPlaying(j, &stat);
            csndExecCmds(true);
            if (stat || j == prevchan) continue;
            cwav_->playingchanids[i ? rightChannel : leftChannel] = j;
            break;
        }
        if (cwav_->playingchanids[i ? rightChannel : leftChannel] == -1) {
            return false;
        }

        prevchan = cwav_->playingchanids[i ? rightChannel : leftChannel];

        u8* block0 = NULL;
        u8* block1 = NULL;
        u32 size;

        block0 = (u8*)((u32)cwav_->channelInfos[i ? rightChannel : leftChannel]->samples.offset + (u8*)(&(cwav_->cwavData->data)));
        size = (cwav_->cwavInfo->LoopEnd / 2);

        CSND_SetAdpcmState(cwav_->playingchanids[i ? rightChannel : leftChannel], 0, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.tableIndex);
        
        if (cwav_->cwavInfo->isLooped) {
            block1 = ((cwav_->cwavInfo->loopStart) / 2) + block0;
            CSND_SetAdpcmState(cwav_->playingchanids[i ? rightChannel : leftChannel], 1, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->loopContext.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->loopContext.tableIndex);
        }
        else {
            block1 = block0;
            CSND_SetAdpcmState(cwav_->playingchanids[i ? rightChannel : leftChannel], 1, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.data, cwav_->IMAADPCMInfos[i ? rightChannel : leftChannel]->context.tableIndex);
        }
        csndExecCmds(true);

        float pan = 0.f;
        if (stereo) {
            if (i == 0)
            {
                pan = -1.0f;
            }
            else 
            {
                pan = 1.0f;
            }
        }

        csndPlaySoundFixed(cwav_->playingchanids[i ? rightChannel : leftChannel], (cwav_->cwavInfo->isLooped ? SOUND_REPEAT : SOUND_ONE_SHOT) | SOUND_FORMAT_ADPCM, cwav_->cwavInfo->sampleRate, 1, pan, block0, block1, size);
        csndExecCmds(true);
    }
    return true;
}

void cwavStop(CWAV* cwav, int leftChannel, int rightChannel) {
    if (!cwav) return;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);

    if (leftChannel >= 0 && leftChannel < cwav_->channelcount) {
        if (cwav_->playingchanids[leftChannel] != -1) {
            CSND_SetPlayState(cwav_->playingchanids[leftChannel], 0);
            cwav_->playingchanids[leftChannel] = -1;
        }
    } else if (rightChannel >= 0 && rightChannel < cwav_->channelcount) {
        if (cwav_->playingchanids[rightChannel] != -1) {
            CSND_SetPlayState(cwav_->playingchanids[rightChannel], 0);
            cwav_->playingchanids[rightChannel] = -1;
        }
    }
    else {
        for (int i = 0; i < cwav_->channelcount; i++) {
            if (cwav_->playingchanids[i] != -1) {
                CSND_SetPlayState(cwav_->playingchanids[i], 0);
                cwav_->playingchanids[i] = -1;
            }
        }
    }
    csndExecCmds(true);
}

void cwavFree(CWAV* cwav) {
    if (!cwav) return;
    cwav_t* cwav_ = CWAVTOIMPL(cwav);

    if (cwav_->playingchanids) cwavStop(cwav, -1, -1);
    if (cwav_->fileBuf) linearFree(cwav_->fileBuf);
    if (cwav_->channelInfos) free(cwav_->channelInfos);
    if (cwav_->IMAADPCMInfos) free(cwav_->IMAADPCMInfos);
    free(cwav_);
    free(cwav);
}