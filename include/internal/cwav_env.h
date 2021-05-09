#ifndef CWAVENV_H
#define CWAVENV_H
#include "cwav.h"
#include "internal/cwav_defs.h"

void cwavEnvUseEnvironment(cwavEnvMode_t envMode);
cwavEnvMode_t cwavEnvGetEnvironment();

void cwavEnvInitialize();
void cwavEnvFinalize();

bool cwavEnvCompatibleEncoding(cwavEncoding_t encoding);

u32 cwavEnvGetChannelAmount();
bool cwavEnvIsChannelAvailable(u32 channel);

void cwavEnvSetADPCMState(cwav_t* cwav, u32 cwavChannel);

void cwavEnvPlay(u32 channel, bool isLooped, cwavEncoding_t encoding, u32 sampleRate, float volume, float pan, float pitch, void* block0, void* block1, u32 loopStart, u32 loopEnd, u32 totalSize);
bool cwavEnvChannelIsPlaying(u32 channel);
void cwavEnvStop(u32 channel);

#endif