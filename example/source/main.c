#include <switch.h>
#include <stdio.h>
#include "haze.h"

// called before main
void userAppInit(void) {
    appletLockExit(); // block exit until everything is cleaned up
}

// called after main has exit
void userAppExit(void) {
    appletUnlockExit(); // unblocks exit to cleanly exit
}

int main(int argc, char** argv) {
    hazeInitialize(); // init libhaze (creates thread)
    consoleInit(NULL); // consol to display to the screen

    // init controller
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    printf("libhaze example!\n\nPress (+) to exit\n");
    consoleUpdate(NULL);

    // loop until + button is pressed
    while (appletMainLoop()) {
        padUpdate(&pad);

        const u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        svcSleepThread(1000000);
    }

    consoleExit(NULL); // exit console display
    hazeExit(); // signals libhaze to exit, closes thread
}
