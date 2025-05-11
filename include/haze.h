#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HazeCallbackType_OpenSession, // data = none
    HazeCallbackType_CloseSession, // data = none
    HazeCallbackType_CreateFile, // data = file
    HazeCallbackType_DeleteFile, // data = file
    HazeCallbackType_RenameFile, // data = rename
    HazeCallbackType_RenameFolder, // data = rename
    HazeCallbackType_CreateFolder, // data = file
    HazeCallbackType_DeleteFolder, // data = file
    HazeCallbackType_ReadBegin, // data = file
    HazeCallbackType_ReadProgress, // data = progress
    HazeCallbackType_ReadEnd, // data = file
    HazeCallbackType_WriteBegin, // data = file
    HazeCallbackType_WriteProgress, // data = progress
    HazeCallbackType_WriteEnd, // data = file
} HazeCallbackType;

typedef struct {
    char filename[0x301];
} HazeCallbackDataFile;

typedef struct {
    char filename[0x301];
    char newname[0x301];
} HazeCallbackDataRename;

typedef struct {
    long long offset;
    long long size;
} HazeCallbackDataProgress;


typedef struct {
    HazeCallbackType type;
    union {
        HazeCallbackDataFile file;
        HazeCallbackDataRename rename;
        HazeCallbackDataProgress progress;
    };
} HazeCallbackData;

typedef void(*HazeCallback)(const HazeCallbackData* data);

/* Callback is optional */
bool hazeInitialize(HazeCallback callback, int cpuid, int prio);
void hazeExit();

#ifdef __cplusplus
}
#endif
