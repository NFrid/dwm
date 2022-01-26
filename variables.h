#ifndef VARIABLES_H_
#define VARIABLES_H_

#include "drw.h"
#include "nwm.h"

extern Systray*   systray;          // systray
extern const char broken[];         // mark for broken clients
extern char       stext[1024];      // text in a statusbar
extern int        screen;           // screen
extern unsigned   screenw, screenh; // X display screen geometry
extern unsigned   barh, barw;       // bar geometry
extern unsigned   tabh;             // tab bar geometry
extern int        lrpad;            // sum of left and right padding for text
extern int (*xerrorxlib)(Display*, XErrorEvent*);

extern Atom     wmatom[WMLast];   // wm-specific atoms
extern Atom     netatom[NetLast]; // _NET atoms
extern Atom     xatom[XLast];     // x-specific atoms
extern Bool     running;          // 0 means nwm stops
extern Cur*     cursor[CurLast];  // used cursors
extern Clr**    scheme;           // colorscheme
extern Display* dpy;              // display of X11 session
extern Drw*     drw;              // drawer that draws
extern Monitor* mons;             // all the monitors
extern Monitor* selmon;           // selected monitor
extern Window   root;             // root window
extern Window   wmcheckwin;       // window for NetWMCheck

extern void (*handler[LASTEvent])(XEvent*);

#endif // VARIABLES_H_
