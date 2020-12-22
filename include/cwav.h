#ifndef CWAV_H
#define CWAV_H
#include "3ds.h"

/// Posible status values.
typedef enum {
    CWAV_SUCCESS = 0, ///< CWAV loaded properly and is ready to play.
    CWAV_FILE_OPEN_FAILED = 1, ///< Failed to open the specified file.
    CWAV_FILE_TOO_LARGE = 2, ///< The file is too large to fit in the available memory.
    CWAV_UNKNOWN_FILE_FORMAT = 3, ///< The specified file is not a valid CWAV file.
    CWAV_INVAID_INFO_BLOCK = 4, ///< The INFO block in the CWAV file is invalid or not supported.
    CWAV_INVAID_DATA_BLOCK = 5, ///< The DATA block in the CWAV file is invalid or not supported.
    CWAV_UNSUPPORTED_AUDIO_ENCODING = 6, ///< The audio encoding is not supported.
} cwavLoadStatus_t;

/// CWAV structure
typedef struct CWAV_s {
    void* cwav; ///< Pointer to internal cwav data, must not be used.
    u32 loadStatus; ///< Value from the cwavLoadStatus_t enum. Set when the CWAV is loaded.
} CWAV;

/// vAddr to pAddr conversion callback definition.
typedef u32(*vaToPaCallback_t)(const void*); 

/**
 * @brief Sets a custom virtual address to physical address conversion callback.
 * @param callback Function callback to use.
 * 
 * The function callback must take the virtual address as a const void* argument and return the physical address as a u32.
 * If not set, the default conversion is used. Use NULL to reset to default.
*/
void cwavSetVAToPACallback(vaToPaCallback_t callback);

/**
 * @brief Loads a CWAV from a file.
 * @param filename Path to the CWAV file.
 * @return A pointer to a CWAV struct.
 * 
 * Use the loadStatus struct member to determine if the load was successful.
 * Wether the load was successful or not, cwavFree must be always called to clean up and free the memory.
 */
CWAV* cwavLoadFromFile(char* filename);

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
 * @brief Frees the CWAV.
 * @param cwav The CWAV to free.
 * 
 * Must be called even if the CWAV load fails.
*/
void cwavFree(CWAV* cwav);

#endif