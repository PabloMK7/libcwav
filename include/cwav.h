/**
 * @file cwav.h
 * @brief libcwav - Library to play (b)cwav files on the 3DS.
*/
#pragma once
#include "3ds.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Posible status values.
typedef enum
{
    CWAV_NOT_ALLOCATED = 0, ///< CWAV is not allocated and cannot be used.
    CWAV_SUCCESS = 1, ///< CWAV loaded properly and is ready to play.
    CWAV_INVALID_ARGUMENT = 2, ///< An invalid argument was passed to the function call.
    CWAV_FILE_OPEN_FAILED = 3, ///< Failed to open the specified file.
    CWAV_FILE_TOO_LARGE = 4, ///< The file is too large to fit in the available memory.
    CWAV_UNKNOWN_FILE_FORMAT = 5, ///< The specified file is not a valid CWAV file.
    CWAV_INVAID_INFO_BLOCK = 6, ///< The INFO block in the CWAV file is invalid or not supported.
    CWAV_INVAID_DATA_BLOCK = 7, ///< The DATA block in the CWAV file is invalid or not supported.
    CWAV_UNSUPPORTED_AUDIO_ENCODING = 8, ///< The audio encoding is not supported.
    
} cwavLoadStatus_t;

/// Possible environments
typedef enum
{
    CWAV_ENV_DSP = 0, // DSP Service, only available for applications.
    CWAV_ENV_CSND = 1 // CSND Service, only available for applets and 3GX plugins.
} cwavEnvMode_t;

/// CWAV structure, some values can be read [R] or written [W] to.
typedef struct CWAV_s
{
    void* cwav; ///< Pointer to internal cwav data, should not be used.
    cwavLoadStatus_t loadStatus; ///< [R] Value from the cwavLoadStatus_t enum. Set when the CWAV is loaded. 
    float monoPan; ///< [RW] Value in the range [-1.0, 1.0]. -1.0 for left ear and 1.0 for right ear. Only used if played in mono. Default: 0.0
    float volume; ///< [RW] Value in the range [0.0, 1.0]. 0.0 muted and 1.0 full volume. Default: 1.0
    u8 numChannels; ///< [R] Number of CWAV channels stored in the file.
    u8 isLooped; ///< [R] Whether the file is looped or not.
} CWAV;

/// vAddr to pAddr conversion callback definition.
typedef u32(*vaToPaCallback_t)(const void*);

/**
 * @brief Sets the environment that libcwav will use.
 * 
 * This call does not initialize the desired service. ndspInit/csndInit should be called before.
 * By default, DSP is used. Modifying the enviroment after loading the first CWAV causes undefined behaviour.
*/
void cwavUseEnvironment(cwavEnvMode_t envMode);

/**
 * @brief Hooks to libctru APT implementation to recieve apt events. (Required for CSND)
 * 
 * Needs to be used in environments supported by libctru, should not be used with applets nor 3GX game plugins.
 * For unsupported environments, use cwavNotifyAptEvent directly to notify individual events.
 * Either cwavDoAptHook or cwavNotifyAptEvent MUST be used, otherwise it will cause undefined behaviour.
*/
void cwavDoAptHook();

/**
 * @brief Notifies the lib of an incoming apt event. (Required for CSND)
 * @param event The apt event to notify.
 * 
 * Should be used in environments not supported by libctru, such as applets and 3GX game plugins.
 * For supported environments, use cwavDoAptHook so the the library automatically handles the events.
 * Either cwavDoAptHook or cwavNotifyAptEvent MUST be used, otherwise it will cause undefined behaviour.
*/
void cwavNotifyAptEvent(APT_HookType event);

/**
 * @brief Sets a custom virtual address to physical address conversion callback. (Only used for CSND)
 * @param callback Function callback to use.
 * 
 * The function callback must take the virtual address as a const void* argument and return the physical address as a u32.
 * If not set, the default conversion is used. Use NULL to reset to default.
*/
void cwavSetVAToPACallback(vaToPaCallback_t callback);

/**
 * @brief Loads a CWAV from a buffer in linear memory.
 * @param bcwavFileBuffer Pointer to the buffer in linear memory (e.g.: linearAlloc()) containing the CWAV file.
 * @param maxSPlays Amount of times this CWAV can be played simultaneously (should be >0).
 * @return A pointer to a CWAV struct.
 * 
 * Use the loadStatus struct member to determine if the load was successful.
 * Wether the load was successful or not, cwavFree must be always called to clean up and free the memory.
 * The bcwavFileBuffer must be manually freed by the user after calling cwavFree (e.g.: linearFree()).
 */
void cwavLoad(CWAV* out, const void* bcwavFileBuffer, u8 maxSPlays);

/**
 * @brief Frees the CWAV.
 * @param cwav The CWAV to free.
 * 
 * Must be called even if the CWAV load fails.
 * The bcwavFileBuffer used in cwavLoad must be manually freed afterwards (e.g.: linearFree()).
 * The CWAV* struct itself must be freed manually if it has been allocated.
*/
void cwavFree(CWAV* cwav);

//#define DIRECT_SOUND_IMPLEMENTED
#ifdef DIRECT_SOUND_IMPLEMENTED
/**
 * @brief Plays the CWAV channels as a direct sound (only available if using CSND).
 * @param cwav The CWAV to play.
 * @param leftChannel The CWAV channel to play on the left ear.
 * @param rigtChannel The CWAV channel to play on the right ear.
 * @param directSoundChannel The direct sound channel to play the sound on. Range [0-3].
 * @param directSoundPrioriy The direct sound priority to use if the specified channel is in use (smaller value -> higher priority). Range [0-31].
 * @param soundModifiers CSND direct sound modifiers to apply to the sound.
 * 
 * To play a single channel in mono for both ears, set rightChannel to -1.
 * The individual channel volumes are multiplied by the CWAV volume.
*/
bool cwavPlayAsDirectSound(CWAV* cwav, int leftChannel, int rightChannel, u32 directSoundChannel, u32 directSoundPriority, CSND_DirectSoundModifiers* soundModifiers);
#endif

/**
 * @brief Plays the specified channels in the bcwav file.
 * @param cwav The CWAV to play.
 * @param leftChannel The CWAV channel to play on the left ear.
 * @param rigtChannel The CWAV channel to play on the right ear.
 * @return True if both channels started playing, false otherwise.
 * 
 * To play a single channel in mono for both ears, set rightChannel to -1.
*/
bool cwavPlay(CWAV* cwav, int leftChannel, int rightChannel);

/**
 * @brief Stops the specified channels in the bcwav file.
 * @param cwav The CWAV to play.
 * @param leftChannel The CWAV channel to stop (left ear).
 * @param rigtChannel The CWAV channel to stop (right ear).
 * 
 * Setting both channels to -1 stops all the CWAV channels.
*/
void cwavStop(CWAV* cwav, int leftChannel, int rightChannel);

/**
 * @brief Checks whether the cwav is currently playing or not.
 * @return Boolean representing the playing state.
*/
bool cwavIsPlaying(CWAV* cwav);

/**
 * @brief Gets a bitmap representing the playing state of the channels for the selected environment.
 * @return Bitmap of the playing channels. First channel is the LSB.
*/
u32 cwavGetEnvironmentPlayingChannels();

#ifdef __cplusplus
}
#endif