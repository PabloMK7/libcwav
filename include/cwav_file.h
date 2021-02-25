/**
 * @file cwav_file.h
 * @brief libcwav - Helper file to load (b)cwav from filesystem.
*/
#pragma once
#include "3ds.h"
#include "cwav.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a CWAV from the file system.
 * @param bcwavFileName Path to the (b)CWAV file in the filesystem.
 * @param maxSPlays Amount of times this CWAV can be played simultaneously (should be >0).
 * 
 * Use the loadStatus struct member to determine if the load was successful.
 * Wether the load was successful or not, cwavFileFree must be always called to clean up and free the memory.
 * Do not use cwavFree, as it will not properly free the bcwav buffer.
 */
inline void cwavFileLoad(CWAV* out, const char* bcwavFileName, u8 maxSPlays)
{
    FILE* file = NULL;
    size_t fileSize = 0;
    void* buffer = NULL;

    if (!out)
        goto exit;

    file = fopen(bcwavFileName, "rb");
    if (!file)
        goto exit;

    if (fseek(file, 0, SEEK_END))
        goto exitClose;
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = linearAlloc(fileSize);
    if (!buffer)
        goto exitClose;

    if (fread(buffer, 1, fileSize, file) != fileSize)
        goto exitClose;

    cwavLoad(out, buffer, maxSPlays);
    out->dataBuffer = buffer;

exitClose:
    fclose(file);
exit:
    return;
}

/**
 * @brief Frees a CWAV loaded from the filesystem.
 * @param cwav The CWAV to free.
 * 
 * Must be called even if the CWAV file load fails.
 * Use this if you have used cwavFileLoad, otherwise use cwavFree.
 * The CWAV* struct itself must be freed manually if it has been allocated.
*/
inline void cwavFileFree(CWAV* cwav)
{
    if (!cwav)
        return;
    
    cwavFree(cwav);

    if (cwav->dataBuffer)
        linearFree(cwav->dataBuffer);
}

#ifdef __cplusplus
}
#endif