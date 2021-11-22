#ifndef NWM_H_
#define NWM_H_

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif // XINERAMA

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* --------------------------------- macros --------------------------------- */

#define BUTTONMASK               (ButtonPressMask | ButtonReleaseMask)
#define INTERSECT(x, y, w, h, m) (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) \
                                  * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define LENGTH(X)    (sizeof X / sizeof X[0])
#define MOUSEMASK    (BUTTONMASK | PointerMotionMask)
#define WIDTH(X)     ((X)->w + 2 * (X)->bw + gappx)
#define HEIGHT(X)    ((X)->h + 2 * (X)->bw + gappx)
#define TAGMASK      ((1 << TAGS_N) - 1)
#define TEXTW(X)     (drw_fontset_getwidth(drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK 0

/* ----------------------------- XEMBED messages ---------------------------- */

#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_IN        4
#define XEMBED_MODALITY_ON     10

#define XEMBED_MAPPED            (1 << 0)
#define XEMBED_WINDOW_ACTIVATE   1
#define XEMBED_WINDOW_DEACTIVATE 2

#define VERSION_MAJOR           0
#define VERSION_MINOR           0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* ---------------------------------- enums --------------------------------- */

enum { CurNormal,
  CurResize,
  CurMove,
  CurLast }; /* cursor */

enum { SchemeNorm,
  SchemeDark,
  SchemeInv,
  SchemeSel }; /* color schemes */

enum { NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetSystemTray,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetSystemTrayOrientationHorz,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast }; /* EWMH atoms */

enum { Manager,
  Xembed,
  XembedInfo,
  XLast }; /* Xembed atoms */

enum { WMProtocols,
  WMDelete,
  WMState,
  WMTakeFocus,
  WMLast }; /* default atoms */

enum { ClkTagBar,
  ClkTabBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  ClkLast }; /* clicks */

/* ---------------------------------- types --------------------------------- */

// action arguments
typedef union {
  int          i;
  unsigned int ui;
  float        f;
  const void*  v;
} Arg;

// button
typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg* arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client  Client;
struct Client {
  char         name[256];                          // client's name
  float        mina, maxa;                         // aspect ratio
  int          x, y, w, h;                         // geometry
  int          oldx, oldy, oldw, oldh;             // prev geometry
  int          basew, baseh, incw, inch;           // based geometry
  int          maxw, maxh, minw, minh;             // based geometry
  int          bw, oldbw;                          // border width
  unsigned int tags;                               // assigned tags
  int          isfixed, isfloating, isurgent;      // client state
  int          neverfocus, oldstate, isfullscreen; // client state
  int          issticky;                           // client state
  Client*      next;                               // next client link
  Client*      snext;                              // next stack
  Monitor*     mon;                                // assigned monitor
  Window       win;                                // client's window
};

typedef struct {
  unsigned int mod;
  KeySym       keysym;
  void (*func)(const Arg*);
  const Arg arg;
} Key;

typedef struct {
  const char* symbol;
  void (*arrange)(Monitor*);
} Layout;

#define MAXTABS 50

typedef struct Pertag Pertag; // Pertag

struct Monitor {
  char          ltsymbol[16];        // symbolic representation of layout
  float         mfact;               // ratio factor of layout
  int           nmaster;             // number of masters
  int           num;                 // number (id)
  int           by;                  // bar geometry
  int           ty;                  // tab bar geometry
  int           mx, my, mw, mh;      // screen size
  int           wx, wy, ww, wh;      // window area
  unsigned int  seltags;             // selected tags
  unsigned int  sellt;               // selected layout
  unsigned int  tagset[2];           // set of tags for the monitor
  int           showbar;             // 0 means no bar
  int           showtab;             // 0 means no tab bar
  int           topbar;              // 0 means bottom bar
  int           toptab;              // 0 means bottom tab bar
  Client*       clients;             // clients on this monitor
  Client*       sel;                 // selected client
  Client*       stack;               // current stack of clients
  Monitor*      next;                // pointer to next monitor
  Window        barwin;              // window of the native bar
  Window        tabwin;              // window of the tab bar
  unsigned int  ntabs;               // number of tab bars
  int           tab_widths[MAXTABS]; // width of tab bars
  const Layout* lt[2];               // layouts
  Pertag*       pertag;              // separated parameters (e.g. layout) per tag
};

typedef struct {
  const char* class;
  const char*  instance;
  const char*  title;
  unsigned int tags;
  int          isfloating;
  int          monitor;
} Rule;

typedef struct {
  unsigned int tag;    // index of tag in array
  unsigned int layout; // index of layout in array
  float        mfact;  // ratio, -1 for default
} PertagRule;

typedef struct Systray Systray;
struct Systray {
  Window  win;
  Client* icons;
};

// TODO: get rid of pertag scum, write your own that works better

#define TAGS_N 10

struct Pertag {
  unsigned int  curtag, prevtag;       // current and previous tag
  int           nmasters[TAGS_N + 1];  // number of windows in master area
  float         mfacts[TAGS_N + 1];    // mfacts per tag
  unsigned int  sellts[TAGS_N + 1];    // selected layouts
  const Layout* ltidxs[TAGS_N + 1][2]; // matrix of tags and layouts indexes
  Bool          showbars[TAGS_N + 1];  // display bar for the current tag
};

// compile-time check if all tags fit into an unsigned int bit array.
struct NumTags {
  char limitexceeded[TAGS_N > 31 ? -1 : 1];
};

#endif // NWM_H_
