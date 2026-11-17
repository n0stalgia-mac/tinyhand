/*
 * TinyHand for classic Mac OS. Minimum system is 7.1 on 68000 and PowerPC.
 * 1-bit QuickDraw, so a Mac Plus running System 7.1 can show it.
 * The hand is drawn on the Window Manager port. A plain window covers the
 * desktop only while drawing is on, so clicks hit this app. That window is
 * not erased, so other applications stay visible. Drawing off hides it.
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
#include <MacMemory.h>

#define kAppleMenu 128
#define kFileMenu 129
#define kHandMenu 130
#define kSoundMenu 131
#define kDrawMenu 132
#define kAboutAlert 128
#define kOldSystemAlert 129

/* gestaltSystemVersion is BCD. 7.0.1 is 0x0701, 7.1 is 0x0710, 7.1.2 is 0x0712. */
#define kMinSystemVersion 0x0710L

#define kAboutItem 1
#define kClearItem 1
#define kQuitItem 3
#define kBeepItem 1
#define kDrawItem 1

/* ANSI 'H' on the classic keyboard. ctrl-option-H matches the macOS app.
   Command-H is the menu equivalent, because a MENU resource can only
   show the command key. */
#define kKeyH 0x04

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

static Boolean gDone;
static Boolean gBeep;
static Boolean gDrawing;
static Boolean gCursorHidden;
static Boolean gDragging;
static Boolean gDragMoved;
static Boolean gMouseMoved;
static Boolean gDirty;
static Boolean gSuspended;
static Boolean gSaveValid;
static Boolean gClipOpen;
static Boolean gPresenting;
static Boolean gShieldShown;
static short gStyle;
static short gDepth;
static Point gHot;
static Point gAnchor;
static Point gLastInk;
static Point gLastGlobal;
static PolyHandle gHand;
static GrafPtr gOverlay;
static WindowPtr gShield;
static WindowRecord gShieldRec;
static RgnHandle gClipSave;
static RgnHandle gVisSave;
static Ptr gSaveBase;
static long gSaveSize;
static Rect gSaveRect;
static BitMap gSaveBits;
static PixMap gSavePix;

/* Full desktop under the click shield. ShowHide fills that window white,
   so this copy goes back on the screen immediately afterwards. */
static Ptr gDeskBase;
static long gDeskSize;
static Rect gDeskRect;
static BitMap gDeskBits;
static PixMap gDeskPix;
static Boolean gDeskValid;

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
    gDirty = true;
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

/* The Window Manager port is global. A color port stores a PixMapHandle
   where a GrafPort stores baseAddr, and a negative word where rowBytes
   would be. Both layouts put visRgn and clipRgn at the same offsets.
   Packed to 2 so the offsets match the Toolbox on 68k and PowerPC. */
#pragma pack(push, 2)
struct PortHead {
    short device;
    Ptr ref;
    short marker;
};
#pragma pack(pop)

static Boolean port_is_color(GrafPtr port)
{
    return ((struct PortHead *)port)->marker < 0;
}

static BitMap *screen_bits(GrafPtr port)
{
    PixMapHandle pm;

    if (!port_is_color(port))
        return &port->portBits;
    pm = (PixMapHandle)((struct PortHead *)port)->ref;
    if (pm == NULL || *pm == NULL)
        return &port->portBits;
    return (BitMap *)(*pm);
}

static short read_depth(GrafPtr port)
{
    PixMapHandle pm;
    short depth;

    if (!port_is_color(port))
        return 1;
    pm = (PixMapHandle)((struct PortHead *)port)->ref;
    if (pm == NULL || *pm == NULL)
        return 1;
    depth = (*pm)->pixelSize;
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16 && depth != 32)
        return 1;
    return depth;
}

static void content_rect(Rect *r)
{
    *r = qd.screenBits.bounds;
    r->top = (short)(r->top + menu_bar_height());
    if (r->top > r->bottom)
        r->top = r->bottom;
}

static void union_point(Rect *r, short x, short y)
{
    if (x < r->left)
        r->left = x;
    if ((short)(x + 1) > r->right)
        r->right = (short)(x + 1);
    if (y < r->top)
        r->top = y;
    if ((short)(y + 1) > r->bottom)
        r->bottom = (short)(y + 1);
}

/* Bounds of every pixel draw_hand and draw_ink can touch, in global
   coordinates, padded for the pen and the arrowhead. */
static void content_bounds(Rect *r)
{
    Rect limit;
    short n;
    short i;
    short idx;
    short pad;
    short rad;

    n = (short)(sizeof kHX / sizeof kHX[0]);
    r->left = gHot.h;
    r->right = (short)(gHot.h + 1);
    r->top = gHot.v;
    r->bottom = (short)(gHot.v + 1);
    for (i = 0; i < n; i++)
        union_point(r, (short)(gHot.h + sc(kHX[i])), (short)(gHot.v + sc(kHY[i])));
    union_point(r, (short)(gHot.h + sc(-34)), (short)(gHot.v + sc(0)));
    union_point(r, (short)(gHot.h + sc(25)), (short)(gHot.v + sc(148)));
    if (gStyle == kStyleLaser) {
        rad = sc(5);
        if (rad < 1)
            rad = 1;
        union_point(r, (short)(gHot.h - rad), (short)(gHot.v - rad));
        union_point(r, (short)(gHot.h + rad + 1), (short)(gHot.v + rad + 1));
    }
    n = gInkCount;
    if (n > 0) {
        if (n < kMaxSeg)
            idx = 0;
        else
            idx = gInkNext;
        for (i = 0; i < n; i++) {
            union_point(r, gInk[idx].x0, gInk[idx].y0);
            union_point(r, gInk[idx].x1, gInk[idx].y1);
            idx = (short)((idx + 1) & (kMaxSeg - 1));
        }
    }
    pad = 16;
    r->left = (short)(r->left - pad);
    r->top = (short)(r->top - pad);
    r->right = (short)(r->right + pad);
    r->bottom = (short)(r->bottom + pad);
    content_rect(&limit);
    if (!SectRect(r, &limit, r))
        SetRect(r, 0, 0, 0, 0);
}

static void screen_begin(void)
{
    Rect r;

    if (gOverlay == NULL || gClipSave == NULL || gVisSave == NULL)
        return;
    SetPort(gOverlay);
    if (gClipOpen)
        return;
    CopyRgn(gOverlay->clipRgn, gClipSave);
    CopyRgn(gOverlay->visRgn, gVisSave);
    content_rect(&r);
    RectRgn(gOverlay->clipRgn, &r);
    RectRgn(gOverlay->visRgn, &r);
    gClipOpen = true;
    PenNormal();
}

static void screen_end(void)
{
    if (!gClipOpen || gOverlay == NULL)
        return;
    SetPort(gOverlay);
    CopyRgn(gClipSave, gOverlay->clipRgn);
    CopyRgn(gVisSave, gOverlay->visRgn);
    gClipOpen = false;
}

static short row_bytes_for(short width, short depth)
{
    long bits;
    long rb;

    if (width < 1)
        width = 1;
    if (depth < 1)
        depth = 1;
    bits = (long)width * (long)depth;
    rb = (bits + 15L) >> 4;
    rb <<= 1;
    if (rb < 2L)
        rb = 2L;
    return (short)rb;
}

static BitMap *save_bitmap(void)
{
    if (gDepth == 1)
        return &gSaveBits;
    return (BitMap *)&gSavePix;
}

static void prepare_offscreen(const Rect *r)
{
    short w;
    short h;
    short rb;
    long bytes;
    BitMap *src;

    w = (short)(r->right - r->left);
    h = (short)(r->bottom - r->top);
    if (w < 1 || h < 1) {
        gSaveValid = false;
        return;
    }
    rb = row_bytes_for(w, gDepth);
    bytes = (long)rb * (long)h;
    if (gSaveBase == NULL || bytes != gSaveSize) {
        if (gSaveBase != NULL)
            DisposePtr(gSaveBase);
        gSaveBase = NewPtr(bytes);
        gSaveSize = 0;
        gSaveValid = false;
        if (gSaveBase == NULL)
            return;
        gSaveSize = bytes;
    }
    if (gDepth == 1) {
        gSaveBits.baseAddr = gSaveBase;
        gSaveBits.rowBytes = rb;
        gSaveBits.bounds = *r;
        return;
    }
    src = screen_bits(gOverlay);
    gSavePix = *(PixMap *)src;
    gSavePix.baseAddr = gSaveBase;
    gSavePix.bounds = *r;
    gSavePix.rowBytes = (short)(rb | (short)0x8000);
}

static void copy_under(Boolean toSave)
{
    BitMap *screen;
    BitMap *save;
    Rect r;

    if (gSaveBase == NULL || gOverlay == NULL)
        return;
    screen = screen_bits(gOverlay);
    save = save_bitmap();
    r = gSaveRect;
    if (toSave)
        CopyBits(screen, save, &r, &r, srcCopy, NULL);
    else
        CopyBits(save, screen, &r, &r, srcCopy, NULL);
}

static void restore_under(void)
{
    if (!gSaveValid)
        return;
    screen_begin();
    copy_under(false);
    gSaveValid = false;
    screen_end();
}

static BitMap *desk_bitmap(void)
{
    if (gDepth == 1)
        return &gDeskBits;
    return (BitMap *)&gDeskPix;
}

/* Copy the content area, including pixels owned by other applications.
   screen_begin opens the Window Manager clip so the Finder window is included. */
static void snapshot_desktop(void)
{
    Rect r;
    short w;
    short h;
    short rb;
    long bytes;
    BitMap *src;
    BitMap *dst;

    gDeskValid = false;
    if (gOverlay == NULL || gClipSave == NULL)
        return;
    content_rect(&r);
    w = (short)(r.right - r.left);
    h = (short)(r.bottom - r.top);
    if (w < 1 || h < 1)
        return;
    rb = row_bytes_for(w, gDepth);
    bytes = (long)rb * (long)h;
    if (gDeskBase == NULL || bytes != gDeskSize) {
        if (gDeskBase != NULL)
            DisposePtr(gDeskBase);
        gDeskBase = NewPtr(bytes);
        gDeskSize = 0;
        if (gDeskBase == NULL)
            return;
        gDeskSize = bytes;
    }
    if (gDepth == 1) {
        gDeskBits.baseAddr = gDeskBase;
        gDeskBits.rowBytes = rb;
        gDeskBits.bounds = r;
    } else {
        src = screen_bits(gOverlay);
        gDeskPix = *(PixMap *)src;
        gDeskPix.baseAddr = gDeskBase;
        gDeskPix.bounds = r;
        gDeskPix.rowBytes = (short)(rb | (short)0x8000);
    }
    gDeskRect = r;
    src = screen_bits(gOverlay);
    dst = desk_bitmap();
    screen_begin();
    CopyBits(src, dst, &r, &r, srcCopy, NULL);
    screen_end();
    gDeskValid = true;
}

static void restore_desktop(void)
{
    BitMap *src;
    BitMap *dst;
    Rect r;

    if (!gDeskValid || gDeskBase == NULL || gOverlay == NULL)
        return;
    src = desk_bitmap();
    dst = screen_bits(gOverlay);
    r = gDeskRect;
    screen_begin();
    CopyBits(src, dst, &r, &r, srcCopy, NULL);
    screen_end();
}

/* Draw only the hand, the stick, and the ink. The pixels underneath are
   copied aside first and put back when the drawing moves, so the desktop
   and other applications stay visible. There is no full-screen window. */
static void present(void)
{
    Rect bounds;

    if (gOverlay == NULL || gPresenting || !gDrawing || gSuspended)
        return;
    gPresenting = true;
    gDirty = false;
    screen_begin();
    if (gSaveValid)
        copy_under(false);
    gSaveValid = false;
    content_bounds(&bounds);
    if (!EmptyRect(&bounds)) {
        gSaveRect = bounds;
        prepare_offscreen(&bounds);
        if (gSaveBase != NULL) {
            copy_under(true);
            gSaveValid = true;
        }
        draw_ink();
        draw_hand(gHot.h, gHot.v);
    }
    screen_end();
    gPresenting = false;
}

static void build_hand(void)
{
    short n;
    short i;

    if (gOverlay == NULL)
        return;
    /* Widen the port or the template polygon is clipped to the desktop. */
    screen_begin();
    gHand = OpenPoly();
    if (gHand == NULL) {
        screen_end();
        return;
    }
    n = (short)(sizeof kHX / sizeof kHX[0]);
    MoveTo((short)(kPolyH + sc(kHX[0])), (short)(kPolyV + sc(kHY[0])));
    for (i = 1; i < n; i++)
        LineTo((short)(kPolyH + sc(kHX[i])), (short)(kPolyV + sc(kHY[i])));
    LineTo((short)(kPolyH + sc(kHX[0])), (short)(kPolyV + sc(kHY[0])));
    ClosePoly();
    screen_end();
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
    gDirty = true;
}

static void toggle_beep(void)
{
    MenuHandle menu;

    gBeep = (Boolean)!gBeep;
    menu = GetMenuHandle(kSoundMenu);
    if (menu != NULL)
        CheckItem(menu, kBeepItem, gBeep);
}

/* The item names the action. Drawing on shows "Draw Off". */
static void sync_draw_item(void)
{
    MenuHandle menu;

    menu = GetMenuHandle(kDrawMenu);
    if (menu == NULL)
        return;
    if (gDrawing)
        SetMenuItemText(menu, kDrawItem, "\pDraw Off");
    else
        SetMenuItemText(menu, kDrawItem, "\pDraw On");
}

/* plainDBox frames its bounds with one pixel and insets the hit area.
   Shift that frame into the menu bar and past the screen edges. */
static void shield_bounds(Rect *r)
{
    Rect screen;
    short mbar;

    screen = qd.screenBits.bounds;
    mbar = menu_bar_height();
    r->left = (short)(screen.left - 1);
    r->top = (short)(mbar - 1);
    r->right = (short)(screen.right + 1);
    r->bottom = (short)(screen.bottom + 1);
}

static void create_shield(void)
{
    Rect r;

    if (gShield != NULL)
        return;
    shield_bounds(&r);
    gShield = NewWindow(&gShieldRec, &r, "\pTinyHand", false, plainDBox,
                        (WindowPtr)-1, false, 0);
}

static void snapshot_desktop(void);
static void restore_desktop(void);

/* ShowHide fills a new window white. Copy the screen aside first and put
   it back after the window is in front, so other applications stay visible.
   FindWindow then reports clicks here as inContent. */
static void show_shield(void)
{
    if (gShield == NULL || gShieldShown)
        return;
    snapshot_desktop();
    ShowHide(gShield, true);
    SelectWindow(gShield);
    gShieldShown = true;
    restore_desktop();
}

static void hide_shield(void)
{
    if (gShield == NULL || !gShieldShown)
        return;
    ShowHide(gShield, false);
    gShieldShown = false;
}

static Boolean point_in_content(Point global);

static void set_drawing(Boolean on)
{
    Point global;

    if (on == gDrawing)
        return;
    if (!on) {
        gDragging = false;
        restore_under();
        show_arrow();
        hide_shield();
        gDrawing = false;
        gDirty = false;
        sync_draw_item();
        return;
    }
    gDrawing = true;
    sync_draw_item();
    show_shield();
    if (gOverlay != NULL && !gSuspended) {
        SetPort(gOverlay);
        GetMouse(&global);
        gLastGlobal = global;
        if (point_in_content(global))
            gHot = global;
    }
    gDirty = true;
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
    case kDrawMenu:
        if (item == kDrawItem)
            set_drawing((Boolean)!gDrawing);
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
    gDirty = true;
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
        gDirty = true;
        return;
    }
    if (cur.h == gLastInk.h && cur.v == gLastInk.v)
        return;
    ink_add(gLastInk.h, gLastInk.v, cur.h, cur.v);
    gLastInk = cur;
    gDirty = true;
}

static void end_drag(const EventRecord *e)
{
    Point cur;

    if (!gDragging || gOverlay == NULL)
        return;
    SetPort(gOverlay);
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
    gDirty = true;
}

static Boolean point_in_content(Point global)
{
    Rect r;

    content_rect(&r);
    return PtInRect(global, &r);
}

static void track_mouse(void)
{
    Point global;
    short mbar;
    Boolean moved;
    Boolean inContent;

    if (gOverlay == NULL || gSuspended || !gDrawing)
        return;
    SetPort(gOverlay);
    GetMouse(&global);
    moved = (global.h != gLastGlobal.h || global.v != gLastGlobal.v);
    gLastGlobal = global;
    if (moved)
        gMouseMoved = true;

    mbar = menu_bar_height();
    inContent = point_in_content(global);
    if (global.v < mbar)
        show_arrow();
    else if (inContent)
        hide_arrow();

    if (gDragging) {
        place_hand(global);
        extend_ink(global);
        return;
    }
    if (gMouseMoved && global.v >= mbar && inContent)
        place_hand(global);
}

static void handle_mouse_down(EventRecord *e)
{
    WindowPtr wp;
    short part;
    Point global;

    part = FindWindow(e->where, &wp);
    if (part == inMenuBar) {
        InitCursor();
        gCursorHidden = false;
        do_menu(MenuSelect(e->where));
        if (gDrawing && !gSuspended)
            gDirty = true;
        return;
    }
    if (part == inSysWindow) {
        SystemClick(e, wp);
        return;
    }
    /* The shield window owns clicks below the menu bar while drawing is
       on. An inDesk click belongs to another application; leaving it
       unhandled lets the Process Manager deliver it. */
    if (!gDrawing || gSuspended)
        return;
    if (part != inContent || wp != gShield)
        return;
    global = e->where;
    if (!point_in_content(global))
        return;
    gDragging = true;
    gDragMoved = false;
    gMouseMoved = true;
    gAnchor = global;
    gLastInk = global;
    place_hand(global);
}

static void handle_key(EventRecord *e)
{
    char ch;
    short code;
    long menuResult;

    ch = (char)(e->message & charCodeMask);
    code = (short)((e->message >> 8) & 0xFF);
    if ((e->modifiers & (controlKey | optionKey)) == (controlKey | optionKey)
        && (e->modifiers & cmdKey) == 0
        && code == kKeyH) {
        set_drawing((Boolean)!gDrawing);
        return;
    }
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
    case osEvt:
        if (((e->message >> 24) & 0xFF) == suspendResumeMessage) {
            if (e->message & resumeFlag) {
                gSuspended = false;
                if (gDrawing) {
                    show_shield();
                    gDirty = true;
                }
            } else {
                gSuspended = true;
                gDragging = false;
                restore_under();
                show_arrow();
                hide_shield();
            }
        }
        break;
    case updateEvt:
        /* BeginUpdate can erase the shield. Put the desktop back, then the
           hand. ValidRect stops that CopyBits from queuing another update,
           which would wipe the hand again before the next present. */
        if (gShield != NULL && (WindowPtr)e->message == gShield && !gClipOpen) {
            BeginUpdate(gShield);
            EndUpdate(gShield);
            if (gDrawing && !gSuspended) {
                restore_desktop();
                present();
                SetPort(gShield);
                ValidRect(&gShield->portRect);
            }
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
    sync_draw_item();
    sound = GetMenuHandle(kSoundMenu);
    if (sound != NULL)
        CheckItem(sound, kBeepItem, true);
}

static void init_overlay(void)
{
    Rect r;

    GetWMgrPort(&gOverlay);
    if (gOverlay == NULL)
        return;
    gClipSave = NewRgn();
    gVisSave = NewRgn();
    gDepth = read_depth(gOverlay);
    create_shield();
    content_rect(&r);
    gHot.h = (short)(r.left + (short)((r.right - r.left) / 2));
    gHot.v = (short)(r.top + (short)((r.bottom - r.top) / 2));
}

static void note_launch_mouse(void)
{
    if (gOverlay == NULL)
        return;
    SetPort(gOverlay);
    GetMouse(&gLastGlobal);
    gMouseMoved = false;
}

int main(void)
{
    EventRecord e;

    gDone = false;
    gDrawing = true;
    gCursorHidden = false;
    gDragging = false;
    gInkCount = 0;
    gInkNext = 0;
    gStyle = kStyleOutline;
    gBeep = true;
    gDepth = 1;

    init_toolbox();
    if (!system_is_supported()) {
        (void)Alert(kOldSystemAlert, NULL);
        return 0;
    }
    setup_menus();
    choose_scale();
    init_overlay();
    if (gOverlay == NULL)
        return 1;
    build_hand();
    note_launch_mouse();
    /* ShowHide fills the shield white. show_shield puts the desktop back.
       The update event's BeginUpdate can erase it again, so paint the hand
       after that update has been handled. */
    show_shield();
    {
        EventRecord u;
        short n;

        for (n = 0; n < 8; n++) {
            if (!GetNextEvent(updateMask, &u))
                break;
            if (gShield != NULL && (WindowPtr)u.message == gShield)
                handle_event(&u);
        }
    }
    if (gDrawing) {
        Point global;

        SetPort(gOverlay);
        GetMouse(&global);
        gLastGlobal = global;
        if (point_in_content(global))
            gHot = global;
        hide_arrow();
        gDirty = true;
        present();
    }

    while (!gDone) {
        if (gDrawing && !gSuspended) {
            if (GetNextEvent(everyEvent, &e))
                handle_event(&e);
            track_mouse();
            if (gDirty && gDrawing && !gSuspended)
                present();
        } else if (WaitNextEvent(everyEvent, &e, 12, NULL)) {
            handle_event(&e);
            if (gDirty && gDrawing && !gSuspended)
                present();
        }
    }

    restore_under();
    show_arrow();
    if (gHand != NULL)
        KillPoly(gHand);
    if (gSaveBase != NULL)
        DisposePtr(gSaveBase);
    if (gDeskBase != NULL)
        DisposePtr(gDeskBase);
    if (gClipSave != NULL)
        DisposeRgn(gClipSave);
    if (gVisSave != NULL)
        DisposeRgn(gVisSave);
    return 0;
}
