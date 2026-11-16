#include "Types.r"

/*
 * Appended only to the PowerPC link, after TinyHand.r.
 * That file's SIZE is 128K/64K. This resource overrides it and
 * RetroPPCAPPL.r. Finder will not launch a PEF unless SIZE is 32-bit
 * clean. The 1 MB partition is the RetroPPC crt default.
 */
resource 'SIZE' (-1) {
    dontSaveScreen, acceptSuspendResumeEvents, enableOptionSwitch,
    canBackground, multiFinderAware, backgroundAndForeground,
    dontGetFrontClicks, ignoreChildDiedEvents, is32BitCompatible,
    reserved, reserved, reserved, reserved, reserved, reserved, reserved,
    1024 * 1024,
    1024 * 1024
};
