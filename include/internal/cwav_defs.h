#ifndef CWAVDEFS_H
#define CWAVDEFS_H
#include "3ds/types.h"

// Thanks https://www.3dbrew.org/wiki/BCWAV

typedef enum
{
    DSP_ADPCM_INFO = 0x300,
    IMA_ADPCM_INFO = 0x301,
    SAMPLE_DATA = 0x1F00,
    INFO_BLOCK = 0x7000,
    DATA_BLOCK = 0x7001,
    CHANNEL_INFO = 0x7100
} cwavReferenceType_t;

typedef enum
{
    PCM8,
    PCM16,
    DSP_ADPCM,
    IMA_ADPCM
} cwavEncoding_t;

typedef struct cwavReference_s
{
    u16 refType;
    u16 padding;
    u32 offset;
} cwavReference_t;

typedef struct cwavSizedReference_s
{
    cwavReference_t ref;
    u32 size;
} cwavSizedReference_t;

typedef struct cwavBlockHeader_s
{
    u32 magic;
    u32 size;
} cwavBlockHeader_t;

typedef struct cwavReferenceTable_s
{
    u32 count;
    cwavReference_t references[];
} cwavReferenceTable_t;

typedef struct cwavchannelInfo_s
{
    cwavReference_t samples;
    cwavReference_t ADPCMInfo;
    u32 reserved;
} cwavchannelInfo_t;

typedef struct cwavDSPADPCMParam_s
{
    u16 coefs[16];
} cwavDSPADPCMParam_t;

typedef struct cwavDSPADPCMContext_s
{
    u16 predScale;
    u16 prevSample;
    u16 secondPrevSample;
} cwavDSPADPCMContext_t;

typedef struct cwavDSPADPCMInfo_s
{
    cwavDSPADPCMParam_t param;
    cwavDSPADPCMContext_t context;
    cwavDSPADPCMContext_t loopContext;
    u16 padding;
} cwavDSPADPCMInfo_t;

typedef struct cwavIMAADPCMContext_s
{
    u16 data;
    u8 tableIndex;
    u8 padding;
} cwavIMAADPCMContext_t;

typedef struct cwavIMAADPCMInfo_s
{
    cwavIMAADPCMContext_t context;
    cwavIMAADPCMContext_t loopContext;
} cwavIMAADPCMInfo_t;

typedef struct cwavInfoBlock_s
{
    cwavBlockHeader_t header;
    u8 encoding;
    bool isLooped;
    u16 padding;
    u32 sampleRate;
    u32 loopStart;
    u32 LoopEnd;
    u32 reserved;
    cwavReferenceTable_t channelInfoRefs;
} cwavInfoBlock_t;

typedef struct cwavDataBlock_s
{
    cwavBlockHeader_t header;
    void* data;
} cwavDataBlock_t;

typedef struct  cwavHeader_s
{
    u32 magic;
    u16 endian;
    u16 headerS;
    u32 version;
    u32 fileSize;
    u16 blockCount;
    u16 reserved;
    cwavSizedReference_t info_blck;
    cwavSizedReference_t data_blck;
} cwavHeader_t;

typedef struct cwav_s
{
    void* fileBuf;
    cwavHeader_t* cwavHeader;
    cwavInfoBlock_t* cwavInfo;
    cwavDataBlock_t* cwavData;
    cwavchannelInfo_t** channelInfos;
    cwavIMAADPCMInfo_t** IMAADPCMInfos;
    cwavDSPADPCMInfo_t** DSPADPCMInfos;
    int** playingChanIds;
    u8 channelcount;
    u8 totalMultiplePlay;
    u8 currMultiplePlay;
} cwav_t;
#endif