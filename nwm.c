/* See LICENSE file for copyright and license details.
 *
 * It's like dwm (see https://dwm.suckless.org/)
 * but patched some amount of times and tweaked so it's kinda different
 *
 * As it says in original version of a file:
 * "To understand everything else, start reading main()"
 *
 * love,
 * Nick Friday <nfriday@ya.ru> 🗿
 */

#include "nwm.h"
#include "core.h"
#include "drw.h"
#include "util.h"
#include "variables.h"

/* -------------------------------- variables ------------------------------- */

Systray*   systray  = NULL;     // systray
const char broken[] = "broken"; // mark for broken clients
char       stext[1024];         // text in a statusbar
int        screen;              // screen
int        sw, sh;              // X display screen geometry
int        bh, blw = 0;         // bar geometry
int        th = 0;              // tab bar geometry
int        lrpad;               // sum of left and right padding for text
int (*xerrorxlib)(Display*, XErrorEvent*);

Atom     wmatom[WMLast];   // wm-specific atoms
Atom     netatom[NetLast]; // _NET atoms
Atom     xatom[XLast];     // x-specific atoms
int      running = 1;      // 0 means nwm stops
Cur*     cursor[CurLast];  // used cursors
Clr**    scheme;           // colorscheme
Display* dpy;              // display of X11 session
Drw*     drw;              // TODO: wtf is drawer (I guess it draws 🗿)
Monitor* mons;             // all the monitors
Monitor* selmon;           // selected monitor
Window   root;             // root window
Window   wmcheckwin;       // window for NetWMCheck

/* ------------------------ function implementations ------------------------ */

int main(int argc, char* argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die_happy("nwm v0.9.999.99999");
  else if (argc != 1)
    die("usage: nwm [-v]");

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);

  if (!(dpy = XOpenDisplay(NULL)))
    die("nwm: cannot open display");

  checkotherwm();
  setup();

  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);

  return EXIT_SUCCESS;
}
