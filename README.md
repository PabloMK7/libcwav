# libcwav
A library for playing **(b)cwav** files on the **3DS**.

# Description
The goal of this library is to provide an interface for playing **(b)cwav** files in 3ds homebrew sofware. The way it is designed allows to play these files in non-application environments, such as *3GX game plugins* or *applets*, as it provides support for the **CSND** system service.

Unlike *(b)cstm* files which are streamed in chunks from their storage media, **(b)cwav** files are fully loaded into the linear RAM. Therefore, **(b)cwav** files are only meant for small sound effects. This library provides support for the **ADPCM** encodings, which heavily reduce the required memory to play the file. 

# Supported Features
## Supported CWAV Audio Encodings
The following audio encodings are supported.

### PCM8/PCM16
Uncompressed **8/16 bit PCM**. Useful if memory usage is not a problem.

### DSP ADPCM
Lossy compression format, useful if the available memory is limited. Can only be played with **DSP**.

### IMA ADPCM
Lossy compression format, similar to **DSP ADPCM**. Can only be played with **CSND**.

## Supported System Services
The following system services used to play the audio are supported.

### DSP
This system service is used by normal applications. It is recommended to use this system service, as it properly supports suspending applications and sleep mode.

### CSND
This system service is used by *applets* to play audio. It has the advantage of playing audio on top of running/suspended applications, whitout causing any interferences.
Use this system service if you want to play audio in *applets* or *3GX game plugins*. Make sure to use *`cwavDoAptHook()`* or *`cwavNotifyAptEvent()`* to handle apt events (app suspend, sleep or exit)!

# Installation and Usage
1. Make sure you have [devkitpro](https://devkitpro.org/wiki/Getting_Started) installed and working.
2. Clone or download the repo and open a command prompt.
3. Run `make install` and confirm there aren't any errors.
4. In your project makefile, add the following to the `LIBDIRS` line (or similar): `$(DEVKITPRO)/libcwav`
5. In your project makefile, add the following to the `LIBS` line (or similar): `-lcwav` if the already listed libraries start with `-l` or just `cwav` if they don't.
6. Add `#include "cwav.h"` in your source files to use the library.

You can check all the available function calls in the documentation provided in [cwav.h](include/cwav.h).

# Credits
- [libctru](https://github.com/devkitPro/libctru): **CSND** and **DSP** implementation.
- [3dbrew.org](https://www.3dbrew.org/wiki/BCWAV): **(b)cwav** file specification.

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
