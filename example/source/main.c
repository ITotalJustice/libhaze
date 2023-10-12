#define NXLINK_LOG 0

#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if NXLINK_LOG
#include <unistd.h>
#endif
#include "haze.h"

static Mutex g_mutex;
static HazeCallbackData* g_callback_data = NULL;
static u32 g_num_events = 0;

// called before main
void userAppInit(void) {
    appletLockExit(); // block exit until everything is cleaned up
}

// called after main has exit
void userAppExit(void) {
    appletUnlockExit(); // unblocks exit to cleanly exit
}

static void callbackHandler(const HazeCallbackData* data) {
    mutexLock(&g_mutex);
        g_num_events++;
        g_callback_data = realloc(g_callback_data, g_num_events * sizeof(HazeCallbackData));
        memcpy(&g_callback_data[g_num_events-1], data, sizeof(*data));
    mutexUnlock(&g_mutex);
}

static void processEvents() {
    mutexLock(&g_mutex);
        // if we have no events, exit early
        if (!g_num_events) {
            mutexUnlock(&g_mutex);
            return;
        }
        // copy data over so that we don't block haze thread for too long
        u32 num_events = g_num_events;
        HazeCallbackData* data = malloc(g_num_events * sizeof(HazeCallbackData));
        memcpy(data, g_callback_data, g_num_events * sizeof(HazeCallbackData));
        // reset
        g_num_events = 0;
        free(g_callback_data);
        g_callback_data = NULL;
    mutexUnlock(&g_mutex);

    // log events
    for (u32 i = 0; i < num_events; i++) {
        switch (data[i].type) {
            case HazeCallbackType_OpenSession: printf("Opening Session\n"); break;
            case HazeCallbackType_CloseSession: printf("Closing Session\n"); break;

            case HazeCallbackType_CreateFile: printf("Creating File: %s\n", data[i].file.filename); break;
            case HazeCallbackType_DeleteFile: printf("Deleting File: %s\n", data[i].file.filename); break;

            case HazeCallbackType_RenameFile: printf("Rename File: %s -> %s\n", data[i].rename.filename, data[i].rename.newname); break;
            case HazeCallbackType_RenameFolder: printf("Rename Folder: %s -> %s\n", data[i].rename.filename, data[i].rename.newname); break;

            case HazeCallbackType_CreateFolder: printf("Creating Folder: %s\n", data[i].file.filename); break;
            case HazeCallbackType_DeleteFolder: printf("Deleting Folder: %s\n", data[i].file.filename); break;

            case HazeCallbackType_ReadBegin: printf("Reading File Begin: %s \r", data[i].file.filename); break;
            case HazeCallbackType_ReadProgress: printf("Reading File: offset: %lld size: %lld\r", data[i].progress.offset, data[i].progress.size); break;
            case HazeCallbackType_ReadEnd: printf("Reading File Finished: %s\n", data[i].file.filename); break;

            case HazeCallbackType_WriteBegin: printf("Writing File Begin: %s \r", data[i].file.filename); break;
            case HazeCallbackType_WriteProgress: printf("Writing File: offset: %lld size: %lld\r", data[i].progress.offset, data[i].progress.size); break;
            case HazeCallbackType_WriteEnd: printf("Writing File Finished: %s\n", data[i].file.filename); break;
        }
    }

    free(data);
    consoleUpdate(NULL);
}

int main(int argc, char** argv) {
    #if NXLINK_LOG
    socketInitializeDefault();
    int fd = nxlinkStdio();
    #endif

    mutexInit(&g_mutex);
    hazeInitialize(callbackHandler); // init libhaze (creates thread)
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

        processEvents();
        svcSleepThread(1000000);
    }

    #if NXLINK_LOG
    close(fd);
    socketExit();
    #endif
    consoleExit(NULL); // exit console display
    hazeExit(); // signals libhaze to exit, closes thread

    if (g_callback_data) {
        free(g_callback_data);
    }
}
