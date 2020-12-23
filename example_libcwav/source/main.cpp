#include <3ds.h>
#include <stdio.h>
#include "math.h"
#include <vector>
#include <string>
#include <tuple>

#include <cwav.h>

void updateScreens() {
		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
		//Wait for VBlank
		gspWaitForVBlank();
}

const char* fileList[] = {
	"romfs:/beep_ima_adpcm.bcwav",
	"romfs:/bwooh_ima_adpcm.bcwav",
	"romfs:/meow_ima_adpcm.bcwav",
	"romfs:/bell_stereo_ima_adpcm.bcwav",
	"romfs:/loop_ima_adpcm.bcwav",
};

std::vector<std::tuple<std::string, CWAV*>> cwavList;

void populateCwavList() {
	for (u32 i = 0; i < sizeof(fileList) / sizeof(char*); i++) {

		CWAV* cwav = cwavLoadFromFile(fileList[i]);

		if (cwav->loadStatus == CWAV_SUCCESS) {

			cwavList.push_back(std::make_tuple(fileList[i], cwav));

		} else {

			cwavFree(cwav);

		}
	}
}

void freeCwavList() {
	for (auto it = cwavList.begin(); it != cwavList.end(); it++)
		cwavFree(std::get<1>(*it));
}

int main(int argc, char **argv)
{
	const char* statusStr[] = {"II", "I>"};
	std::vector<int> cwavStatus;

	gfxInitDefault();
	csndInit();
	romfsInit();

	consoleInit(GFX_TOP, NULL);
	populateCwavList();
	
	int currsound = 0;
	bool changed = true;

	if (cwavList.size() != 0)
	{
		for (u32 i = 0; i < cwavList.size(); i++) 
			cwavStatus.push_back(0);
		
		// Main loop
		while (aptMainLoop())
		{
			svcSleepThread(1000000);
			if (changed) {
				consoleClear();
				printf("libcwav example.\n\n|%s| %s\n\nPress A to play the selected sound.\nPress UP/DOWN to change the file.\nPress B to stop the selected file.\n\nPress START to exit.", statusStr[cwavStatus[currsound]], std::get<0>(cwavList[currsound]).c_str());
				changed = false;
			}
			hidScanInput();
			u32 kdown = hidKeysDown();
			if (kdown & KEY_UP) {
				currsound++;
				if (currsound >= (int)cwavList.size()) currsound = 0;
				changed = true;
			}
			if (kdown & KEY_DOWN) {
				currsound--;
				if (currsound < 0) currsound = (int)cwavList.size() - 1;
				changed = true;
			}
			if (kdown & KEY_A) {
				CWAV* cwav = std::get<1>(cwavList[currsound]);
				if (cwav->numChannels == 2) {
					cwavPlay(cwav, 0, 1);
				} else {
					cwavPlay(cwav, 0, -1);
				}
				if (cwav->isLooped)
					cwavStatus[currsound] = 1;
				changed = true;
			}
			if (kdown & KEY_B) {
				CWAV* cwav = std::get<1>(cwavList[currsound]);
				if (cwav->numChannels == 2) {
					cwavStop(cwav, 0, 1);
				} else {
					cwavStop(cwav, 0, -1);
				}
				cwavStatus[currsound] = 0;
				changed = true;
			}
			if (kdown & KEY_START) {
				break;
			}
			updateScreens();
		}
	}
	freeCwavList();
	romfsExit();
	csndExit();
	gfxExit();
	return 0;
}
