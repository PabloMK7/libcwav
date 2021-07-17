#include <3ds.h>
#include <stdio.h>
#include "math.h"
#include <vector>
#include <string>
#include <tuple>

#include <cwav.h>
#include <ncsnd.h>

void updateScreens()
{
		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
		//Wait for VBlank
		gspWaitForVBlank();
}

const char* fileList[] =
{
	"romfs:/beep_dsp_adpcm.bcwav",
	"romfs:/bwooh_ima_adpcm.bcwav",
	"romfs:/meow_pcm8.bcwav",
	"romfs:/bell_stereo_ima_adpcm.bcwav",
	"romfs:/bell_stereo_dsp_adpcm.bcwav",
	"romfs:/loop_pcm16.bcwav",
	"romfs:/loop_ima_adpcm.bcwav",
	"romfs:/loop_dsp_adpcm.bcwav",
};

const u8 maxSPlayList[] =
{
	3,
	3,
	3,
	4,
	4,
	1,
	1,
	1
};

const char* bit_str[] = 
{
    "0000", "0001", "0010", "0011",
    "0100", "0101", "0110", "0111",
    "1000", "1001", "1010", "1011",
    "1100", "1101", "1110", "1111"
};

void print_u32_binary(u32 val)
{
	for (u32 i = 0; i < 4; i++)
	{
		u32 toprint = val >> ((3 - i) * 8);
		printf("%s%s", bit_str[(toprint >> 4) & 0xF], bit_str[toprint & 0x0F]);
	}
}

std::vector<std::tuple<std::string, CWAV*>> cwavList;

void populateCwavList()
{
	
	for (u32 i = 0; i < sizeof(fileList) / sizeof(char*); i++)
	{

		CWAV* cwav = (CWAV*)malloc(sizeof(CWAV));

		FILE* file = fopen(fileList[i], "rb");
		if (!file)
		{
			cwavFree(cwav);
			free(cwav);
			continue;
		}

		fseek(file, 0, SEEK_END);
		u32 fileSize = ftell(file);
		void* buffer = linearAlloc(fileSize);
		if (!buffer) // This should never happen (unless we load a file too big to fit)
			svcBreak(USERBREAK_PANIC);

		fseek(file, 0, SEEK_SET); 
		fread(buffer, 1, fileSize, file);
		fclose(file);

		// Since we manually allocated the buffer, we use cwavLoad directly...
		cwavLoad(cwav, buffer, maxSPlayList[i]);
		cwav->dataBuffer = buffer; // (We store the buffer pointer here for convinience, but it's not required.)
		// or, we could have let the library handle it...
		// cwavFileLoad(cwav, fileList[i], maxSPlayList[i]);

		if (cwav->loadStatus == CWAV_SUCCESS)
		{
			cwavList.push_back(std::make_tuple(std::string(fileList[i]), cwav));
		}
		else
		{
			// Manually de-allocating the buffer
			cwavFree(cwav);
			linearFree(cwav->dataBuffer);
			// or, if we used cwavFileLoad, let the library handle it.
			// cwavFileFree(cwav);

			free(cwav);
		}
	}
}

void freeCwavList()
{
	for (auto it = cwavList.begin(); it != cwavList.end(); it++)
	{
		// Manually de-allocating the buffer
		cwavFree(std::get<1>(*it));
		void* buffer = std::get<1>(*it)->dataBuffer;
		if (buffer)
			linearFree(buffer);
		// or, if we used cwavFileLoad, let the library handle it.
		// cwavFileFree(std::get<1>(*it));
		free(std::get<1>(*it));
	}
}

int main(int argc, char **argv)
{
	const char* statusStr[] = {"II", "I>"};
	std::vector<int> cwavStatus;

	gfxInitDefault();
	romfsInit();

	consoleInit(GFX_TOP, NULL);

	int currsound = 0;
	cwavEnvMode_t mode = CWAV_ENV_DSP;
	bool changed = true;
	bool exit = true;

	while (aptMainLoop())
	{
		svcSleepThread(1000000);
		if (changed)
		{
			consoleClear();
			printf("libcwav example.\n\nPress X to use: CSND.\nPress Y to use: DSP.\n\nPress START to exit.");
			changed = false;
		}
		hidScanInput();
		u32 kdown = hidKeysDown();
		if (kdown & KEY_X)
		{
			mode = CWAV_ENV_CSND;
			exit = false;
			break;
		}
		if (kdown & KEY_Y)
		{
			mode = CWAV_ENV_DSP;
			exit = false;
			break;
		}
		if (kdown & KEY_START)
		{
			break;
		}
	}

	changed = true;
	if (!exit)
		cwavUseEnvironment(mode);
	if (!exit && mode == CWAV_ENV_CSND)
	{
		ncsndInit(true);
	}
	else if (!exit && mode == CWAV_ENV_DSP)
	{
		ndspInit();
	}

	if (!exit)
		populateCwavList();

	if (!exit && cwavList.size() != 0)
	{
		for (u32 i = 0; i < cwavList.size(); i++) 
			cwavStatus.push_back(0);
		
		// Main loop
		while (aptMainLoop())
		{
			svcSleepThread(1000000);
			if (changed)
			{
				consoleClear();
				printf("libcwav example.\n\n|%s| %s\n\nPress A to play the selected sound.\nPress UP/DOWN to change the file.\nPress B to stop the selected file.%s\n\nPress START to exit.\n\nPlaying channels:\n", statusStr[cwavStatus[currsound]], std::get<0>(cwavList[currsound]).c_str(), mode == CWAV_ENV_CSND ? "\nPress Y to play as direct sound." : "");
				print_u32_binary(cwavGetEnvironmentPlayingChannels());
				changed = false;
			}
			else
			{
				printf("\r");
				print_u32_binary(cwavGetEnvironmentPlayingChannels());
				fflush(stdout);
			}
			hidScanInput();
			u32 kdown = hidKeysDown();
			if (kdown & KEY_UP)
			{
				currsound++;
				if (currsound >= (int)cwavList.size())
					currsound = 0;
				changed = true;
			}
			if (kdown & KEY_DOWN)
			{
				currsound--;
				if (currsound < 0)
					currsound = (int)cwavList.size() - 1;
				changed = true;
			}
			if (kdown & KEY_A)
			{
				CWAV* cwav = std::get<1>(cwavList[currsound]);
				if (cwav->numChannels == 2)
				{
					cwavPlay(cwav, 0, 1);
				}
				else
				{
					cwavPlay(cwav, 0, -1);
				}
				if (cwav->isLooped)
					cwavStatus[currsound] = 1;
				changed = true;
			}
			if (mode == CWAV_ENV_CSND && (kdown & KEY_Y))
			{
				ncsndDirectSound dirSound;
				ncsndInitializeDirectSound(&dirSound);

				dirSound.soundModifiers.forceSpeakerOutput = 1;
				dirSound.soundModifiers.ignoreVolumeSlider = 1;
				//dirSound.soundModifiers.playOnSleep = 1;

				CWAV* cwav = std::get<1>(cwavList[currsound]);
				if (cwav->numChannels == 2)
				{
					cwavPlayAsDirectSound(cwav, 0, 1, 0, 0, &dirSound.soundModifiers);
				}
				else
				{
					cwavPlayAsDirectSound(cwav, 0, -1, 0, 0, &dirSound.soundModifiers);
				}
			}
			if (kdown & KEY_B)
			{
				CWAV* cwav = std::get<1>(cwavList[currsound]);
				if (cwav->numChannels == 2)
				{
					cwavStop(cwav, 0, 1);
				}
				else
				{
					cwavStop(cwav, 0, -1);
				}
				cwavStatus[currsound] = 0;
				changed = true;
			}
			if (kdown & KEY_START)
			{
				break;
			}
			updateScreens();
		}
	}
	freeCwavList();
	romfsExit();
	if (!exit && mode == CWAV_ENV_CSND)
		ncsndExit();
	else if (!exit && mode == CWAV_ENV_DSP)
		ndspExit();
	gfxExit();
	return 0;
}
