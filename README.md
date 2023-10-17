# libhaze

libhaze is [haze](https://github.com/Atmosphere-NX/Atmosphere/tree/master/troposphere/haze) made into a easy to use library.

---

## how to use

Add the `source` and `include` folders to your makefile. This may look something like:
```mk
SOURCES     += src/libhaze/source
INCLUDES    += src/libhaze/include
```

Here is an example for your c/c++ project:

```c
#include <switch.h>
#include "haze.h"

int main(int argc, char** argv) {
    appletLockExit(); // block exit until everything is cleaned up
    hazeInitialize(NULL); // init libhaze without callback (creates thread)

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    // loop until + button is pressed
    while (appletMainLoop()) {
        padUpdate(&pad);

        const u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        svcSleepThread(1000000);
    }

    hazeExit(); // signals libhaze to exit, closes thread
    appletUnlockExit(); // unblocks exit to cleanly exit
}
```

---

## changes

some changes to haze were made:

- instead of [allocating the entire heap](https://github.com/Atmosphere-NX/Atmosphere/blob/8b88351cb46afab3907e395f40733854fd8de0cf/troposphere/haze/source/ptp_object_heap.cpp#L45), libhaze allocates 2x 20Mib blocks.
- console_main_loop.hpp was changed to accept a stop_token to allow signalling for exit from another thread.
- console_main_loop.hpp was changed to remove console gfx code.
- `SuspendAndWaitForFocus()` loop now sleeps thread instead of spinlooping until focus state changes.
- added event callback for when files are created, deleted, written and read .

...and that's it! The rest of haze is unchanged :)

---

## Todo

in no particular order:

- replace vapours
- get connection info
- workflow
- examples

---

## Credits

All credit for libhaze goes to [liamwhite](https://github.com/liamwhite) and the [Atmosphere team](https://github.com/Atmosphere-NX/Atmosphere).
