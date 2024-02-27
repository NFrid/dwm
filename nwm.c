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

#include "core.h"
#include "drw.h"
#include "nwm.h"
#include "util.h"
#include "variables.h"

Systray*   systray  = NULL;
const char broken[] = "broken";
char       stext[1024];
int        screen;
unsigned   screenw, screenh;
unsigned   barh, barw = 0;
unsigned   tabh = 0;
int        lrpad;
int (*xerrorxlib)(Display*, XErrorEvent*);

Atom     wmatom[WMLast];
Atom     netatom[NetLast];
Atom     xatom[XLast];
Atom     motifatom;
Bool     running = 1;
Cur*     cursor[CurLast];
Clr**    scheme;
Display* dpy;
Drw*     drw;
Monitor* mons;
Monitor* selmon;
Window   root;
Window   wmcheckwin;
Bool     ignorenotify = False;

char debuginfo[36];

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
