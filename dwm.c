/* See LICENSE file for copyright and license details.
 *
 * It's like original dwm (see https://dwm.suckless.org/)
 * but patched some amount of times and tweaked so it's kinda different
 * 
 * There are some .c files in chunks/ that have parts of this file.
 * Why? It's easier for me.
 *
 * As it says in original version of a file:
 * "To understand everything else, start reading main()"
 * with the proviso that not "everything else" but "anything".
 * 
 * I made an attempt to figure out as many as I can and write some comments.
 * Hope it'll help in some ways.
 *
 * love,
 * Nick Friday <nfriday@ya.ru> 🗿
 */
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif // XINERAMA

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
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

#include "drw.h"
#include "util.h"

#include "dwm.h"

#include "config.h"

/* ----------------------- config.h related structures ---------------------- */

struct Pertag {
  unsigned int  curtag, prevtag;             // current and previous tag
  int           nmasters[LENGTH(tags) + 1];  // number of windows in master area
  float         mfacts[LENGTH(tags) + 1];    // mfacts per tag
  unsigned int  sellts[LENGTH(tags) + 1];    // selected layouts
  const Layout* ltidxs[LENGTH(tags) + 1][2]; // matrix of tags and layouts indexes
  int           showbars[LENGTH(tags) + 1];  // display bar for the current tag
};

// compile-time check if all tags fit into an unsigned int bit array.
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* -------------------------------- variables ------------------------------- */

static Systray*   systray  = NULL;
static const char broken[] = "broken"; // mark for broken clients
static char       stext[1024];         // text in a statusbar
static int        screen;
static int        sw, sh;      // X display screen geometry
static int        bh, blw = 0; // bar geometry
static int        lrpad;       // sum of left and right padding for text
static int (*xerrorxlib)(Display*, XErrorEvent*);
static unsigned int numlockmask = 0;

static Atom     wmatom[WMLast];   // wm-specific atoms
static Atom     netatom[NetLast]; // _NET atoms
static Atom     xatom[XLast];     // x-specific atoms
static int      running = 1;      // 0 means dwm stops
static Cur*     cursor[CurLast];  // used cursors
static Clr**    scheme;           // colorscheme
static Display* dpy;              // display of X11 session
static Drw*     drw;              // TODO: wtf is drawer (I guess it draws 🗿)
static Monitor* mons;             // all the monitors
static Monitor* selmon;           // selected monitor
static Window   root;             // root window
static Window   wmcheckwin;       // window for NetWMCheck

/* ------------------------ function implementations ------------------------ */

#include "chunks/actions.c" // all the actions
#include "chunks/bar.c"     // bar related
#include "chunks/core.c"    // core wm stuff
#include "chunks/init.c"    // init wm
#include "chunks/layouts.c" // layout functions
#include "chunks/manage.c"  // manage clients, windows and monitors
#include "chunks/uninit.c"  // uninit wm
#include "chunks/xevents.c" // xevent handlers

int main(int argc, char* argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-nf (6.2 based)");
  else if (argc != 1)
    die("usage: dwm [-v]");

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);

  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");

  checkotherwm();
  setup();

#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif // __OpenBSD__

  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);

  return EXIT_SUCCESS;
}
