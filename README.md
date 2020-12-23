# libcwav
A library for playing (b)cwav files on the 3DS.

# Description
The goal of this library is to provide an interface for playing (b)cwav files in 3ds homebrew sofware. The way it is designed allows to play cwav files in non-application environments, such as 3GX game plugins or applets, as it provides support for the CSND system service.

Unlike (b)cstm files which are streamed in chunks from their storage media, (b)cwav files are fully loaded into the linear RAM. Therefore, (b)cwav files are only meant for small sound effects. This library provides support for the IMA ADPCM encoding, which heavily reduces the required memory to play the file. 

# Supported Features
## Supported CWAV Audio Encodings
Currently, the following audio encodings are supported (with looping).

### IMA ADPCM
Lossy compression format, useful if the available memory is limited.

## Supported System Services
Currently, the following system services used to play the audio are supported.

### CSND
This system service is used by applets to play audio. It has the advantage of playing audio on top of running/suspended applications, whitout causing any interferences.
Use this system service if you want to play audio in applets or 3GX game plugins. Unlike DSP, it's the user's responsability to stop the currently playing (b)cwav files when the application is suspended or closed.

# Planned Features
## Planned CWAV Audio Encodings
The following audio encodings are planned (with looping).

### PCM8/PCM16
Uncompressed 8/16 bit PCM samples. Useful if memory usage is not a problem.

### DSP ADPCM
Lossy compression format, similar to IMA ADPCM.

## Planned System Services
The following system services used to play the audio are planned.

### DSP
This system service is used by normal applications, implemented as NDSP in libctru. It is recommended to use this system service, as it properly supports application suspension and closing.

# Installation and Usage
1. Make sure you have devkitpro installed and working.
2. Clone or download the repo and open a command prompt.
3. Run `make install` and confirm there aren't any errors.
4. In your project makefile, add the following to the `LIBDIRS` line (or similar): `$(DEVKITPRO)/libcwav`
5. Add `#include "cwav.h"` in your source files to use the library.

You can check all the available function calls in the documentation provided in [cwav.h](include/cwav.h).

# Credits
- [libctru](https://github.com/devkitPro/libctru): CSND and NDSP implementation.
- [3dbrew.org](https://www.3dbrew.org/wiki/BCWAV): (B)cwav file specification.

# License
This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1.  The origin of this software must not be misrepresented; you must not claim
    that you wrote the original software. If you use this software in a product,
    an acknowledgment in the product documentation would be appreciated but is
    not required.

2.  Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

3.  This notice may not be removed or altered from any source distribution.
