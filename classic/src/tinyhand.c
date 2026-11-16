/*
 * TinyHand for classic Mac OS. Minimum system is 7.1 on 68000 and PowerPC.
 * 1-bit QuickDraw, so a Mac Plus running System 7.1 can show it.
 *
 * Retro68's 68k int is 16-bit. Coordinates and counts are short or long.
 * The same file also compiles for a classic PowerPC Mac (RetroPPC).
 * Stroke storage is a static ring.
 */
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Events.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Devices.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#include <Sound.h>
#include <Gestalt.h>

#define kAppleMenu 128
#define kFileMenu 129
#define kHandMenu 130
#define kSoundMenu 131
#define kAboutAlert 128
#define kOldSystemAlert 129

/* gestaltSystemVersion is BCD. 7.0.1 is 0x0701, 7.1 is 0x0710, 7.1.2 is 0x0712. */
#define kMinSystemVersion 0x0710L

#define kAboutItem 1
#define kClearItem 1
#define kQuitItem 3
#define kBeepItem 1

#define kStyleOutline 1
#define kStyleSolid 2
#define kStyleRobot 3
#define kStyleLaser 4

#define kMaxSeg 256
#define kTapSlop 3
#define kPolyH 64
#define kPolyV 20

/* Palm height in the design coordinates: fingertip y=0 to the wrist,
   where the stick starts (the kHY/kRobot values of 82). The macOS app
   sizes that palm to pointerSize, default 0.13 of the screen height.
   gScale is 8.8 fixed point (256 = one design pixel), chosen once from
   qd.screenBits.bounds at launch. */
#define kWristV 82
#define kScaleOne 256
#define kHandFracNum 13
#define kHandFracDen 100
#define kScaleMin 64
#define kScaleMax 1024

typedef struct InkSeg {
    short x0;
    short y0;
    short x1;
    short y1;
    short arrow;
} InkSeg;

static const unsigned char kWinTitle[] = "\pTinyHand";

static WindowPtr gWin;
static Boolean gDone;
static Boolean gBeep;
static Boolean gCursorHidden;
static Boolean gDragging;
static Boolean gDragMoved;
static Boolean gMouseMoved;
static short gStyle;
static Point gHot;
static Point gAnchor;
static Point gLastInk;
static Point gLastGlobal;
static PolyHandle gHand;

static InkSeg gInk[kMaxSeg];
static short gInkCount;
static short gInkNext;
static short gScale = kScaleOne;

/* Fingertip at (0, 0). Index up on the left, other fingers folded,
   thumb further left. The stick hangs from the wrist, to the right of
   the index finger, so the hand sits on the top of the stick. */
static const short kHX[] = {
    0, 5, 5, 14, 20, 20, 12, 14, 22, 22, 14, 16, 20, 18, 24, 18,
    18, 10, 10, 0, -4, -8, -10, -28, -34, -32, -12, -6, -5, -5
};
static const short kHY[] = {
    0, 1, 12, 16, 18, 28, 32, 34, 38, 48, 52, 54, 64, 74, 82, 82,
    148, 148, 82, 82, 74, 66, 58, 62, 50, 40, 38, 26, 14, 1
};

typedef char kHandAxesMatch[(sizeof kHX == sizeof kHY) ? 1 : -1];

/* Same pose, separate rectangles so the robot reads as blocks. */
static const short kRobot[][4] = {
    {-5, 0, 6, 14},
    {-8, 12, 20, 78},
    {12, 14, 22, 30},
    {14, 34, 24, 50},
    {14, 54, 22, 70},
    {-34, 38, -10, 60},
    {-2, 72, 25, 86},
    {10, 80, 19, 148}
};

static short menu_bar_height(void)
{
    short h;

    h = GetMBarHeight();
    if (h < 20)
        h = 20;
    return h;
}

static short sab(short v)
{
    if (v < 0)
        return (short)-v;
    return v;
}

/* Map a design-coordinate delta onto the screen. Half rounds away from 0. */
static short sc(short v)
{
    long s;

    if (v >= 0)
        s = ((long)v * (long)gScale + 128L) / (long)kScaleOne;
    else
        s = ((long)v * (long)gScale - 128L) / (long)kScaleOne;
    return (short)s;
}

static void choose_scale(void)
{
    Rect b;
    short sh;
    long den;
    long num;

    b = qd.screenBits.bounds;
    sh = (short)(b.bottom - b.top);
    if (sh < 1)
        sh = 342;
    den = (long)kHandFracDen * (long)kWristV;
    num = (256L * (long)kHandFracNum * (long)sh + den / 2) / den;
    if (num < (long)kScaleMin)
        num = (long)kScaleMin;
    if (num > (long)kScaleMax)
        num = (long)kScaleMax;
    gScale = (short)num;
}

static void show_arrow(void)
{
    if (!gCursorHidden)
        return;
    InitCursor();
    gCursorHidden = false;
}

static void hide_arrow(void)
{
    if (gCursorHidden)
        return;
    HideCursor();
    gCursorHidden = true;
}

static void ink_clear(void)
{
    gInkCount = 0;
    gInkNext = 0;
    if (gWin != NULL)
        InvalRect(&gWin->portRect);
}

static void ink_add(short x0, short y0, short x1, short y1)
{
    gInk[gInkNext].x0 = x0;
    gInk[gInkNext].y0 = y0;
    gInk[gInkNext].x1 = x1;
    gInk[gInkNext].y1 = y1;
    gInk[gInkNext].arrow = 0;
    gInkNext = (short)((gInkNext + 1) & (kMaxSeg - 1));
    if (gInkCount < kMaxSeg)
        gInkCount = (short)(gInkCount + 1);
}

static void ink_set_last_arrow(void)
{
    short idx;

    if (gInkCount <= 0)
        return;
    idx = (short)((gInkNext - 1) & (kMaxSeg - 1));
    gInk[idx].arrow = 1;
}

static long isqrt_long(long v)
{
    long bit;
    long r;

    if (v <= 0)
        return 0;
    r = 0;
    bit = 1L << 30;
    while (bit > v)
        bit >>= 2;
    while (bit != 0) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static void draw_arrow(short x0, short y0, short x1, short y1)
{
    long dx;
    long dy;
    long len;
    long bx;
    long by;
    long px;
    long py;

    dx = (long)x1 - (long)x0;
    dy = (long)y1 - (long)y0;
    len = isqrt_long(dx * dx + dy * dy);
    if (len < 1)
        return;
    bx = -(dx * 10) / len;
    by = -(dy * 10) / len;
    px = -(dy * 4) / len;
    py = (dx * 4) / len;
    MoveTo(x1, y1);
    LineTo((short)(x1 + bx + px), (short)(y1 + by + py));
    MoveTo(x1, y1);
    LineTo((short)(x1 + bx - px), (short)(y1 + by - py));
}

static void draw_ink(void)
{
    short i;
    short idx;
    short n;

    n = gInkCount;
    if (n <= 0)
        return;
    if (n < kMaxSeg)
        idx = 0;
    else
        idx = gInkNext;
    PenNormal();
    for (i = 0; i < n; i++) {
        MoveTo(gInk[idx].x0, gInk[idx].y0);
        LineTo(gInk[idx].x1, gInk[idx].y1);
        if (gInk[idx].arrow)
            draw_arrow(gInk[idx].x0, gInk[idx].y0, gInk[idx].x1, gInk[idx].y1);
        idx = (short)((idx + 1) & (kMaxSeg - 1));
    }
}

static void frame_hand_lines(short hx, short hy)
{
    short n;
    short i;

    n = (short)(sizeof kHX / sizeof kHX[0]);
    PenNormal();
    MoveTo((short)(hx + sc(kHX[0])), (short)(hy + sc(kHY[0])));
    for (i = 1; i < n; i++)
        LineTo((short)(hx + sc(kHX[i])), (short)(hy + sc(kHY[i])));
    LineTo((short)(hx + sc(kHX[0])), (short)(hy + sc(kHY[0])));
}

static void draw_hand_poly(short hx, short hy, Boolean fill)
{
    short dh;
    short dv;

    if (gHand == NULL) {
        frame_hand_lines(hx, hy);
        return;
    }
    dh = (short)(hx - kPolyH);
    dv = (short)(hy - kPolyV);
    OffsetPoly(gHand, dh, dv);
    PenNormal();
    if (fill)
        PaintPoly(gHand);
    else
        FramePoly(gHand);
    OffsetPoly(gHand, (short)-dh, (short)-dv);
}

static void draw_robot(short hx, short hy)
{
    short i;
    short n;
    short left;
    short top;
    short right;
    short bottom;

    n = (short)(sizeof kRobot / sizeof kRobot[0]);
    PenNormal();
    for (i = 0; i < n; i++) {
        Rect q;

        /* Trap calls reject a nested call in an argument on this compiler. */
        left = (short)(hx + sc(kRobot[i][0]));
        top = (short)(hy + sc(kRobot[i][1]));
        right = (short)(hx + sc(kRobot[i][2]));
        bottom = (short)(hy + sc(kRobot[i][3]));
        SetRect(&q, left, top, right, bottom);
        PaintRect(&q);
    }
}

static void draw_laser(short hx, short hy)
{
    Rect o;
    short r;

    r = sc(5);
    if (r < 1)
        r = 1;
    PenNormal();
    SetRect(&o, (short)(hx - r), (short)(hy - r), (short)(hx + r + 1), (short)(hy + r + 1));
    PaintOval(&o);
}

static void draw_hand(short hx, short hy)
{
    if (gStyle == kStyleSolid)
        draw_hand_poly(hx, hy, true);
    else if (gStyle == kStyleRobot)
        draw_robot(hx, hy);
    else if (gStyle == kStyleLaser)
        draw_laser(hx, hy);
    else
        draw_hand_poly(hx, hy, false);
}

static void draw_all(void)
{
    if (gWin == NULL)
        return;
    SetPort(gWin);
    EraseRect(&gWin->portRect);
    draw_ink();
    draw_hand(gHot.h, gHot.v);
}

static void build_hand(void)
{
    short n;
    short i;

    if (gWin == NULL)
        return;
    SetPort(gWin);
    gHand = OpenPoly();
    if (gHand == NULL)
        return;
    n = (short)(sizeof kHX / sizeof kHX[0]);
    MoveTo((short)(kPolyH + sc(kHX[0])), (short)(kPolyV + sc(kHY[0])));
    for (i = 1; i < n; i++)
        LineTo((short)(kPolyH + sc(kHX[i])), (short)(kPolyV + sc(kHY[i])));
    LineTo((short)(kPolyH + sc(kHX[0])), (short)(kPolyV + sc(kHY[0])));
    ClosePoly();
}

static void sync_style_checks(void)
{
    MenuHandle menu;
    short item;

    menu = GetMenuHandle(kHandMenu);
    if (menu == NULL)
        return;
    for (item = kStyleOutline; item <= kStyleLaser; item++)
        CheckItem(menu, item, (Boolean)(item == gStyle));
}

static void set_style(short style)
{
    gStyle = style;
    sync_style_checks();
    if (gWin != NULL)
        InvalRect(&gWin->portRect);
}

static void toggle_beep(void)
{
    MenuHandle menu;

    gBeep = (Boolean)!gBeep;
    menu = GetMenuHandle(kSoundMenu);
    if (menu != NULL)
        CheckItem(menu, kBeepItem, gBeep);
}

static void do_menu(long result)
{
    short menu;
    short item;
    Str255 name;

    menu = HiWord(result);
    item = LoWord(result);
    switch (menu) {
    case kAppleMenu:
        if (item == kAboutItem)
            (void)Alert(kAboutAlert, NULL);
        else if (item > 1) {
            GetMenuItemText(GetMenuHandle(kAppleMenu), item, name);
            (void)OpenDeskAcc(name);
        }
        break;
    case kFileMenu:
        if (item == kClearItem)
            ink_clear();
        else if (item == kQuitItem)
            gDone = true;
        break;
    case kHandMenu:
        if (item >= kStyleOutline && item <= kStyleLaser)
            set_style(item);
        break;
    case kSoundMenu:
        if (item == kBeepItem)
            toggle_beep();
        break;
    default:
        break;
    }
    HiliteMenu(0);
}

static void place_hand(Point local)
{
    if (local.h == gHot.h && local.v == gHot.v)
        return;
    gHot = local;
    if (gWin != NULL)
        InvalRect(&gWin->portRect);
}

static void extend_ink(Point cur)
{
    short dx;
    short dy;

    if (!gDragMoved) {
        dx = sab((short)(cur.h - gAnchor.h));
        dy = sab((short)(cur.v - gAnchor.v));
        if (dx <= kTapSlop && dy <= kTapSlop)
            return;
        ink_add(gAnchor.h, gAnchor.v, cur.h, cur.v);
        gDragMoved = true;
        gLastInk = cur;
        if (gWin != NULL)
            InvalRect(&gWin->portRect);
        return;
    }
    if (cur.h == gLastInk.h && cur.v == gLastInk.v)
        return;
    ink_add(gLastInk.h, gLastInk.v, cur.h, cur.v);
    gLastInk = cur;
    if (gWin != NULL)
        InvalRect(&gWin->portRect);
}

static void end_drag(const EventRecord *e)
{
    Point cur;

    if (!gDragging || gWin == NULL)
        return;
    SetPort(gWin);
    GetMouse(&cur);
    gMouseMoved = true;
    place_hand(cur);
    extend_ink(cur);
    if (gDragMoved) {
        if (e->modifiers & cmdKey)
            ink_set_last_arrow();
    } else if (gBeep) {
        SysBeep(30);
    }
    gDragging = false;
    InvalRect(&gWin->portRect);
}

static Boolean point_in_content(Point local)
{
    if (gWin == NULL)
        return false;
    if (local.h < gWin->portRect.left || local.h >= gWin->portRect.right)
        return false;
    if (local.v < gWin->portRect.top || local.v >= gWin->portRect.bottom)
        return false;
    return true;
}

static void track_mouse(void)
{
    Point local;
    Point global;
    short mbar;
    Boolean moved;
    Boolean inContent;

    if (gWin == NULL)
        return;
    if (FrontWindow() != gWin) {
        show_arrow();
        return;
    }
    SetPort(gWin);
    GetMouse(&local);
    global = local;
    LocalToGlobal(&global);
    moved = (global.h != gLastGlobal.h || global.v != gLastGlobal.v);
    gLastGlobal = global;
    if (moved)
        gMouseMoved = true;

    mbar = menu_bar_height();
    inContent = point_in_content(local);
    if (global.v < mbar)
        show_arrow();
    else if (inContent)
        hide_arrow();

    if (gDragging) {
        place_hand(local);
        extend_ink(local);
        return;
    }
    if (gMouseMoved && global.v >= mbar && inContent)
        place_hand(local);
}

static void handle_mouse_down(EventRecord *e)
{
    WindowPtr wp;
    short part;
    Point local;
    Rect limit;

    part = FindWindow(e->where, &wp);
    if (part == inMenuBar) {
        InitCursor();
        gCursorHidden = false;
        do_menu(MenuSelect(e->where));
        return;
    }
    if (part == inSysWindow) {
        SystemClick(e, wp);
        return;
    }
    if (wp != gWin)
        return;
    if (part == inGoAway) {
        if (TrackGoAway(gWin, e->where))
            gDone = true;
        return;
    }
    if (part == inDrag) {
        limit = qd.screenBits.bounds;
        limit.top = (short)(limit.top + menu_bar_height());
        DragWindow(gWin, e->where, &limit);
        return;
    }
    if (part != inContent)
        return;

    if (FrontWindow() != gWin)
        SelectWindow(gWin);
    SetPort(gWin);
    local = e->where;
    GlobalToLocal(&local);
    gDragging = true;
    gDragMoved = false;
    gMouseMoved = true;
    gAnchor = local;
    gLastInk = local;
    place_hand(local);
}

static void handle_key(EventRecord *e)
{
    char ch;
    short code;
    long menuResult;

    ch = (char)(e->message & charCodeMask);
    code = (short)((e->message >> 8) & 0xFF);
    if (e->modifiers & cmdKey) {
        if (ch == '.' || code == 0x2F) {
            gDone = true;
            return;
        }
        if (ch == 'x' || ch == 'X') {
            ink_clear();
            HiliteMenu(0);
            return;
        }
        menuResult = MenuKey(ch);
        if (menuResult == 0 && (ch == 'q' || ch == 'Q'))
            gDone = true;
        else if (menuResult == 0 && (ch == 'c' || ch == 'C'))
            ink_clear();
        else
            do_menu(menuResult);
        return;
    }
    if (e->what == keyDown && ch == 0x1B) {
        if (gInkCount > 0)
            ink_clear();
        else
            gDone = true;
    }
}

static void handle_event(EventRecord *e)
{
    switch (e->what) {
    case mouseDown:
        handle_mouse_down(e);
        break;
    case mouseUp:
        if (gDragging)
            end_drag(e);
        break;
    case keyDown:
    case autoKey:
        handle_key(e);
        break;
    case updateEvt:
        if ((WindowPtr)e->message == gWin) {
            BeginUpdate(gWin);
            draw_all();
            EndUpdate(gWin);
        }
        break;
    case activateEvt:
        if ((WindowPtr)e->message == gWin) {
            if (e->modifiers & activeFlag)
                InvalRect(&gWin->portRect);
            else
                show_arrow();
        }
        break;
    case osEvt:
        if (((e->message >> 24) & 0xFF) == suspendResumeMessage) {
            if (e->message & resumeFlag)
                InvalRect(&gWin->portRect);
            else
                show_arrow();
        }
        break;
    default:
        break;
    }
}

/* True when Gestalt reports System 7.1 or newer. A missing selector or any
   other error is treated as too old: the caller shows the alert and quits. */
static Boolean system_is_supported(void)
{
    long version;
    OSErr err;

    version = 0;
    err = Gestalt(gestaltSystemVersion, &version);
    if (err != noErr)
        return false;
    if (version < kMinSystemVersion)
        return false;
    return true;
}

static void init_toolbox(void)
{
    MaxApplZone();
    MoreMasters();
    MoreMasters();
    MoreMasters();
    MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
}

static void setup_menus(void)
{
    Handle mbar;
    MenuHandle sound;

    mbar = GetNewMBar(128);
    if (mbar == NULL)
        return;
    SetMenuBar(mbar);
    AppendResMenu(GetMenuHandle(kAppleMenu), 'DRVR');
    DrawMenuBar();
    gStyle = kStyleOutline;
    gBeep = true;
    sync_style_checks();
    sound = GetMenuHandle(kSoundMenu);
    if (sound != NULL)
        CheckItem(sound, kBeepItem, true);
}

static void create_window(void)
{
    Rect r;
    short w;
    short h;

    r = qd.screenBits.bounds;
    r.top = (short)(r.top + menu_bar_height());
    gWin = NewWindow(NULL, &r, kWinTitle, true, plainDBox,
                     (WindowPtr)-1, false, 0);
    if (gWin == NULL)
        return;
    SetPort(gWin);
    w = (short)(gWin->portRect.right - gWin->portRect.left);
    h = (short)(gWin->portRect.bottom - gWin->portRect.top);
    gHot.h = (short)(gWin->portRect.left + w / 2);
    gHot.v = (short)(gWin->portRect.top + h / 2);
}

static void note_launch_mouse(void)
{
    Point local;

    if (gWin == NULL)
        return;
    SetPort(gWin);
    GetMouse(&local);
    gLastGlobal = local;
    LocalToGlobal(&gLastGlobal);
    gMouseMoved = false;
}

int main(void)
{
    EventRecord e;

    gDone = false;
    gCursorHidden = false;
    gDragging = false;
    gInkCount = 0;
    gInkNext = 0;
    gStyle = kStyleOutline;
    gBeep = true;

    init_toolbox();
    if (!system_is_supported()) {
        (void)Alert(kOldSystemAlert, NULL);
        return 0;
    }
    setup_menus();
    choose_scale();
    create_window();
    if (gWin == NULL)
        return 1;
    build_hand();
    note_launch_mouse();
    /* Paint before the first event so a fresh launch already shows the hand. */
    draw_all();

    while (!gDone) {
        if (GetNextEvent(everyEvent, &e))
            handle_event(&e);
        track_mouse();
    }

    show_arrow();
    if (gHand != NULL)
        KillPoly(gHand);
    if (gWin != NULL)
        DisposeWindow(gWin);
    return 0;
}
