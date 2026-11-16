#include "Types.r"

resource 'MENU' (128) {
    128, textMenuProc,
    allEnabled, enabled,
    apple,
    {
        "About TinyHand...", noIcon, noKey, noMark, plain,
        "-", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (129) {
    129, textMenuProc,
    allEnabled, enabled,
    "File",
    {
        "Clear", noIcon, "C", noMark, plain,
        "-", noIcon, noKey, noMark, plain,
        "Quit", noIcon, "Q", noMark, plain
    }
};

resource 'MENU' (130) {
    130, textMenuProc,
    allEnabled, enabled,
    "Hand",
    {
        "Outline", noIcon, noKey, noMark, plain,
        "Solid", noIcon, noKey, noMark, plain,
        "Robot", noIcon, noKey, noMark, plain,
        "Laser", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (131) {
    131, textMenuProc,
    allEnabled, enabled,
    "Sound",
    {
        "Beep", noIcon, noKey, noMark, plain
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130, 131 }
};

resource 'ALRT' (128) {
    {40, 40, 230, 430},
    128,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    noAutoCenter
};

resource 'DITL' (128) {
    {
        {155, 300, 175, 370},
        Button { enabled, "OK" },

        {12, 16, 145, 370},
        StaticText {
            disabled,
            "TinyHand\n"
            "\n"
            "A hand on a stick. Needs System 7.1.\n"
            "The fingertip follows the pointer.\n"
            "Drag in the window to draw ink.\n"
            "Command-drag adds an arrowhead.\n"
            "Clear is Command-C. Quit is Command-Q."
        }
    }
};

resource 'ALRT' (129) {
    {60, 40, 200, 430},
    129,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    noAutoCenter
};

resource 'DITL' (129) {
    {
        {110, 300, 130, 370},
        Button { enabled, "OK" },

        {10, 16, 100, 370},
        StaticText {
            disabled,
            "TinyHand requires System 7.1 or later."
        }
    }
};

resource 'ICN#' (128) {
    {
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"007F F800 007F FFC0"
        $"007F FFC0 3FFF FFC0"
        $"3FFF FFC0 3FFF F800"
        $"3FFF FFE0 3FFF FFE0"
        $"3FFF FFE0 007F FFE0"
        $"007F FF80 007F FF80"
        $"007F FF80 000F E000"
        $"000F E000 0003 8000"
        $"0003 8000 0003 8000"
        $"0003 8000 0003 8000"
        $"0003 8000 0000 0000",
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"0003 E000 0003 E000"
        $"007F F800 007F FFC0"
        $"007F FFC0 3FFF FFC0"
        $"3FFF FFC0 3FFF F800"
        $"3FFF FFE0 3FFF FFE0"
        $"3FFF FFE0 007F FFE0"
        $"007F FF80 007F FF80"
        $"007F FF80 000F E000"
        $"000F E000 0003 8000"
        $"0003 8000 0003 8000"
        $"0003 8000 0003 8000"
        $"0003 8000 0000 0000"
    }
};

resource 'BNDL' (128) {
    'tHnd', 0,
    { 'FREF', { 0, 128 }, 'ICN#', { 0, 128 } }
};

resource 'FREF' (128) {
    'APPL', 0, ""
};

data 'tHnd' (0) {
    $"5469 6E79 4861 6E64"
};

resource 'SIZE' (-1) {
    dontSaveScreen, acceptSuspendResumeEvents, enableOptionSwitch,
    canBackground, multiFinderAware, backgroundAndForeground,
    dontGetFrontClicks, ignoreChildDiedEvents, is32BitCompatible,
    reserved, reserved, reserved, reserved, reserved, reserved, reserved,
    128 * 1024,
    64 * 1024
};

resource 'vers' (1) {
    0x01, 0x00, release, 0x00, verUS,
    "1.0",
    "1.0, TinyHand classic"
};
