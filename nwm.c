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

#include "config.h"

/* --------------------- config.h dependent structures ---------------------- */

struct Pertag {
  unsigned int  curtag, prevtag;             // current and previous tag
  int           nmasters[LENGTH(tags) + 1];  // number of windows in master area
  float         mfacts[LENGTH(tags) + 1];    // mfacts per tag
  unsigned int  sellts[LENGTH(tags) + 1];    // selected layouts
  const Layout* ltidxs[LENGTH(tags) + 1][2]; // matrix of tags and layouts indexes
  Bool          showbars[LENGTH(tags) + 1];  // display bar for the current tag
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
static int        th = 0;      // tab bar geometry
static int        lrpad;       // sum of left and right padding for text
static int (*xerrorxlib)(Display*, XErrorEvent*);
static unsigned int numlockmask = 0;

static Atom     wmatom[WMLast];   // wm-specific atoms
static Atom     netatom[NetLast]; // _NET atoms
static Atom     xatom[XLast];     // x-specific atoms
static int      running = 1;      // 0 means nwm stops
static Cur*     cursor[CurLast];  // used cursors
static Clr**    scheme;           // colorscheme
static Display* dpy;              // display of X11 session
static Drw*     drw;              // TODO: wtf is drawer (I guess it draws 🗿)
static Monitor* mons;             // all the monitors
static Monitor* selmon;           // selected monitor
static Window   root;             // root window
static Window   wmcheckwin;       // window for NetWMCheck

/* ------------------------ function implementations ------------------------ */

int main(int argc, char* argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("nwm");
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
/* ------------------------------ uninit stuff ------------------------------ */

// uninitialize wm
void cleanup(void) {
  Arg      a   = { .ui = ~0 };
  Layout   foo = { "", NULL };
  Monitor* m;
  size_t   i;

  // destroy, free and unmanage all the stuff
  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, False);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);
  if (showsystray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors) + 1; i++)
    free(scheme[i]);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

// unitialize monitor specific stuff
void cleanupmon(Monitor* mon) {
  Monitor* m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  XUnmapWindow(dpy, mon->tabwin);
  XDestroyWindow(dpy, mon->tabwin);
  free(mon);
}

/* -------------------------------- actions --------------------------------- */

// just quit from wm
void quit(const Arg* arg) {
  running = 0;
}

// set selected tags to arg.ui tagmask
void view(const Arg* arg) {
  int          i;
  unsigned int tmptag;

  // TODO: comment?
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    selmon->pertag->prevtag         = selmon->pertag->curtag;

    if (arg->ui == ~0)
      selmon->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag                  = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag  = tmptag;
  }

  selmon->nmaster               = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact                 = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt                 = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt]     = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);

  focus(NULL);
  arrange(selmon);
}

// toggle all view
void toggleall(const Arg* arg) {
  Arg tmp;
  if (TAGMASK != selmon->tagset[selmon->seltags]) {
    tmp.ui = TAGMASK;
  } else {
    tmp.ui = selmon->sel->tags;
  }
  view(&tmp);
}

// toggle selected tags with arg.ui tagmask
void toggleview(const Arg* arg) {
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
  int          i;

  // the first visible client should be the same after we add a new tag
  // we also want to be sure not to mutate the focus
  Client* const c = nexttiled(selmon->clients);
  if (c) {
    Client* const selected = selmon->sel;
    pop(c);
    focus(selected);
  }

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;

    if (newtagset == ~0) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      selmon->pertag->curtag  = 0;
    }

    /* test if the user did not select the same tag */
    if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i = 0; !(newtagset & 1 << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }

    /* apply settings for this view */
    selmon->nmaster               = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact                 = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt                 = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt]     = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
      togglebar(NULL);

    focus(NULL);
    arrange(selmon);
  }
}

// apply arg.ui tagmask for selected client
void tag(const Arg* arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

// toggle applied tagmask for selected client for arg.ui tagmask
void toggletag(const Arg* arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

// set selected tags to arg.ui tagmask for current monitor
void tagmon(const Arg* arg) {
  if (!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

// toggle floating state for selected client
void togglefloating(const Arg* arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y,
        selmon->sel->w, selmon->sel->h, 0);
  arrange(selmon);
}

// toggle sticky state for selected client
void togglesticky(const Arg* arg) {
  if (!selmon->sel)
    return;
  selmon->sel->issticky = !selmon->sel->issticky;
  arrange(selmon);
}

// toggle fullscreen state for selected client
void togglefullscr(const Arg* arg) {
  if (selmon->sel)
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

// focus urgent item and its tag
static void focusurgent(const Arg* arg) {
  Monitor* m;
  Client*  c;
  int      i;
  for (m = selmon; m; m = m->next)
    for (c = m->clients; c && !c->isurgent; c = c->next)
      ;
  if (c) {
    for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++)
      ;
    if (i < LENGTH(tags)) {
      const Arg a = { .ui = 1 << i };
      selmon      = c->mon;
      view(&a);
      focus(c);
    }
  }
}

/* move focused item in stack
  * arg.i - shift amount (e.g. +1) */
void movestack(const Arg* arg) {
  Client *c = NULL, *p = NULL, *pc = NULL, *i;

  if (arg->i > 0) {
    /* find the client after selmon->sel */
    for (c = selmon->sel->next; c && (!ISVISIBLE(c) || c->isfloating); c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && (!ISVISIBLE(c) || c->isfloating); c = c->next)
        ;

  } else {
    /* find the client before selmon->sel */
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i) && !i->isfloating)
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->isfloating)
          c = i;
  }
  /* find the client before selmon->sel and c */
  for (i = selmon->clients; i && (!p || !pc); i = i->next) {
    if (i->next == selmon->sel)
      p = i;
    if (i->next == c)
      pc = i;
  }

  /* swap c and selmon->sel selmon->clients in the selmon->clients list */
  if (c && c != selmon->sel) {
    Client* temp      = selmon->sel->next == c ? selmon->sel : selmon->sel->next;
    selmon->sel->next = c->next == selmon->sel ? c : c->next;
    c->next           = temp;

    if (p && p != c)
      p->next = c;
    if (pc && pc != selmon->sel)
      pc->next = selmon->sel;

    if (selmon->sel == selmon->clients)
      selmon->clients = c;
    else if (c == selmon->clients)
      selmon->clients = selmon->sel;

    arrange(selmon);
  }
}

// cycle through layouts
void cyclelayout(const Arg* arg) {
  Layout* l;
  for (l = (Layout*)layouts; l != selmon->lt[selmon->sellt]; l++)
    ;
  if (arg->i > 0) {
    if (l->symbol && (l + 1)->symbol)
      setlayout(&((Arg) { .v = (l + 1) }));
    else
      setlayout(&((Arg) { .v = layouts }));
  } else {
    if (l != layouts && (l - 1)->symbol)
      setlayout(&((Arg) { .v = (l - 1) }));
    else
      setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
  }
}

// resize window by mouse
void resizemouse(const Arg* arg) {
  int      ocx, ocy, nw, nh;
  Client*  c;
  Monitor* m;
  XEvent   ev;
  Time     lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) // don't support resizing fullscreen windows by mouse
    return;
  // TODO: comment
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
          None, cursor[CurResize]->cursor, CurrentTime)
      != GrabSuccess)
    return;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
          && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
            && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

// move client by mouse
void movemouse(const Arg* arg) {
  int      x, y, ocx, ocy, nx, ny;
  Client*  c;
  Monitor* m;
  XEvent   ev;
  Time     lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
          None, cursor[CurMove]->cursor, CurrentTime)
      != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if ((selmon->wx + selmon->ww) - (nx + WIDTH(c)) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if ((selmon->wy + selmon->wh) - (ny + HEIGHT(c)) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
          && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

// switch bar on/off
void togglebar(const Arg* arg) {
  selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
  updatebarpos(selmon);
  resizebarwin(selmon);
  if (showsystray) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = -bh;
    else if (selmon->showbar) {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - bh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

// cycle through clients' tab modes
void tabmode(const Arg* arg) {
  if (arg && arg->i >= 0)
    selmon->showtab = arg->ui % showtab_nmodes;
  else
    selmon->showtab = (selmon->showtab + 1) % showtab_nmodes;
  arrange(selmon);
}

// switch first client in stack (sel<->first)
void zoom(const Arg* arg) {
  Client* c = selmon->sel;

  if (!selmon->lt[selmon->sellt]->arrange
      || (selmon->sel && selmon->sel->isfloating))
    return;
  if (c == nexttiled(selmon->clients))
    if (!c || !(c = nexttiled(c->next)))
      return;
  pop(c);
}

// set current layout to arg.v
void setlayout(const Arg* arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout*)arg->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

// set current ratio factor (arg.f > 1.0 will set it absolutely)
void setmfact(const Arg* arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

// shift tagmask to next/prev tag from the edgest active (arg.i > 0 is next)
void shiftviewclients(const Arg* arg) {
  Arg          shifted;
  Client*      c;
  unsigned int tagmask = 0;

  for (c = selmon->clients; c; c = c->next)
    tagmask = tagmask | c->tags;

  shifted.ui = selmon->tagset[selmon->seltags];

  if (arg->i > 0) // left circular shift
    do {
      // use only the most left tag
      if (n_ones(shifted.ui) > 1)
        shifted.ui = 1 << (31 - __builtin_clz(shifted.ui));

      shifted.ui = (shifted.ui << arg->i)
                 | (shifted.ui >> (LENGTH(tags) - arg->i));
    } while (tagmask && !(shifted.ui & tagmask));
  else // right circular shift
    do {
      // use only the most right tag
      if (n_ones(shifted.ui) > 1)
        shifted.ui = 1 << __builtin_ctz(shifted.ui);

      shifted.ui = (shifted.ui >> (-arg->i)
                    | shifted.ui << (LENGTH(tags) + arg->i));
    } while (tagmask && !(shifted.ui & tagmask));

  view(&shifted);
}

// change amount of masters (+arg.i)
void incnmaster(const Arg* arg) {
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

// kill selected client
void killclient(const Arg* arg) {
  if (!selmon->sel)
    return;
  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

// focus a window
void focuswin(const Arg* arg) {
  int     iwin = arg->i;
  Client* c    = NULL;
  for (c = selmon->clients; c && (iwin || !ISVISIBLE(c)); c = c->next) {
    if (ISVISIBLE(c))
      --iwin;
  };
  if (c) {
    focus(c);
    restack(selmon);
  }
}

// focus monitor (arg.i > 0 is next)
void focusmon(const Arg* arg) {
  Monitor* m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

// move focus in stack (arg.i > 0 is forward)
void focusstack(const Arg* arg) {
  Client *c = NULL, *i;

  if (!selmon->sel || selmon->sel->isfullscreen)
    return;
  if (arg->i > 0) {
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(c);
    restack(selmon);
  }
}

/* ---------------------------- statusbar stuff ----------------------------- */

// draw the bar I suppose...
void drawbar(Monitor* m) {
  int          x, w, tw = 0, stw = 0;
  int          boxs = drw->fonts->h / 9;
  int          boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client*      c;

  if (showsystray && m == systraytomon(m) && !systrayonleft)
    stw = getsystraywidth();

  // draw status first so it can be overdrawn by tags later
  if (m == selmon || 1) { // draw status on every monitor
    tw = m->ww - drawstatusbar(m, bh, stext, stw);
  }

  // TODO: comment below
  resizebarwin(m);
  for (c = m->clients; c; c = c->next) {
    occ |= c->tags == 255 ? 0 : c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    // do not draw vacant tags
    if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
      continue;

    w = TEXTW(tags[i]);

    drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : m == selmon ? SchemeNorm
                                                                                       : SchemeDark]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
    x += w;
  }
  w = blw = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[m == selmon ? SchemeNorm : SchemeDark]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - tw - x) > bh) {
    if (m->sel) {
      drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeInv]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
      if (m->sel->issticky)
        drw_polygon(drw, x + boxs, m->sel->isfloating ? boxs * 2 + boxw : boxs, stickyiconbb.x, stickyiconbb.y, boxw, boxw * stickyiconbb.y / stickyiconbb.x, stickyicon, LENGTH(stickyicon), Nonconvex, m->sel->tags & m->tagset[m->seltags]);
    } else {
      drw_setscheme(drw, scheme[m == selmon ? SchemeNorm : SchemeInv]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

// draw all the bars on different monitors
void drawbars(void) {
  Monitor* m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

// update all the bars
void updatebars(void) {
  unsigned int         w;
  Monitor*             m;
  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixmap = ParentRelative,
    .event_mask        = ButtonPressMask | ExposureMask
  };
  XClassHint ch = { "nwm", "nwm" };
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    w = m->ww;
    if (showsystray && m == systraytomon(m))
      w -= getsystraywidth();
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && m == systraytomon(m))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    m->tabwin = XCreateWindow(dpy, root, m->wx, m->ty, m->ww, th, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, m->tabwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, m->tabwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

// draw all the tabs on different monitors
void drawtabs(void) {
  Monitor* m;

  for (m = mons; m; m = m->next)
    drawtab(m);
}

// compare int (idk really, it is needed for drawtab)
static int cmpint(const void* p1, const void* p2) {
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */
  return *((int*)p1) > *(int*)p2;
}

void drawtab(Monitor* m) {
  Client*      c;
  unsigned int i;
  int          itag = -1;
  char         view_info[50];
  int          view_info_w = 0;
  int          sorted_label_widths[MAXTABS];
  int          tot_width;
  int          maxsize = bh;
  int          x       = 0;
  int          w       = 0;

  //view_info: indicate the tag which is displayed in the view
  for (i = 0; i < LENGTH(tags); ++i) {
    if ((selmon->tagset[selmon->seltags] >> i) & 1) {
      if (itag >= 0) { //more than one tag selected
        itag = -1;
        break;
      }
      itag = i;
    }
  }
  /* if (0 <= itag && itag < LENGTH(tags)) { */
  /*   snprintf(view_info, sizeof view_info, "[%s]", tags[itag]); */
  /* } else { */
  /*   strncpy(view_info, "[...]", sizeof view_info); */
  /* } */
  view_info[sizeof(view_info) - 1] = 0;
  view_info_w                      = TEXTW(view_info);
  tot_width                        = view_info_w;

  /* Calculates number of labels and their width */
  m->ntabs = 0;
  for (c = m->clients; c; c = c->next) {
    if (!ISVISIBLE(c))
      continue;
    m->tab_widths[m->ntabs] = TEXTW(c->name);
    tot_width += m->tab_widths[m->ntabs];
    ++m->ntabs;
    if (m->ntabs >= MAXTABS)
      break;
  }

  if (tot_width > m->ww) { //not enough space to display the labels, they need to be truncated
    memcpy(sorted_label_widths, m->tab_widths, sizeof(int) * m->ntabs);
    qsort(sorted_label_widths, m->ntabs, sizeof(int), cmpint);
    tot_width = view_info_w;
    for (i = 0; i < m->ntabs; ++i) {
      if (tot_width + (m->ntabs - i) * sorted_label_widths[i] > m->ww)
        break;
      tot_width += sorted_label_widths[i];
    }
    maxsize = (m->ww - tot_width) / (m->ntabs - i);
  } else {
    maxsize = m->ww;
  }
  i = 0;
  for (c = m->clients; c; c = c->next) {
    if (!ISVISIBLE(c))
      continue;
    if (i >= m->ntabs)
      break;
    if (m->tab_widths[i] > maxsize)
      m->tab_widths[i] = maxsize;
    w = m->tab_widths[i];
    drw_setscheme(drw, (c == m->sel) ? scheme[SchemeSel] : scheme[SchemeNorm]);
    drw_text(drw, x, 0, w, th, lrpad / 2, c->name, 0);
    x += w;
    ++i;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);

  /* cleans interspace between window names and current viewed tag label */
  w = m->ww - view_info_w - x;
  drw_text(drw, x, 0, w, th, lrpad / 2, "", 0);

  /* view info */
  x += w;
  w = view_info_w;
  drw_text(drw, x, 0, w, th, lrpad / 2, view_info, 0);

  drw_map(drw, m->tabwin, 0, 0, m->ww, th);
}

// update position of the bar
void updatebarpos(Monitor* m) {
  Client* c;
  int     nvis = 0;

  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    if (m->topbar)
      m->wy += bh;
  } else {
    m->by = -bh;
  }

  for (c = m->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      ++nvis;
  }

  if (m->showtab == showtab_always
      || ((m->showtab == showtab_auto) && (nvis > 1) && (m->lt[m->sellt]->arrange == monocle))) {
    m->wh -= th;
    m->ty = m->toptab ? m->wy : m->wy + m->wh;
    if (m->toptab)
      m->wy += th;
  } else {
    m->ty = -th;
  }
}

// resize bar window
void resizebarwin(Monitor* m) {
  unsigned int w = m->ww;
  if (showsystray && m == systraytomon(m) && !systrayonleft)
    w -= getsystraywidth();
  XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
}

// apply systray to selected monitor
Monitor* systraytomon(Monitor* m) {
  Monitor* t;
  int      i, n;
  if (!systraypinning) {
    if (!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for (n = 1, t = mons; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next)
    ;
  if (systraypinningfailfirst && n < systraypinning)
    return mons;
  return t;
}

// get width of systray
unsigned int getsystraywidth() {
  unsigned int w = 0;
  Client*      i;
  if (showsystray)
    for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next)
      ;
  return w ? w + systrayspacing : 1;
}

// draw status bar (with pretty colors ^-^)
int drawstatusbar(Monitor* m, int bh, char* stext, int stw) {
  int   ret, i, w, x, len;
  short isCode = 0;
  char* text;
  char* p;
  Clr   oldbg, oldfg;

  // TODO: comment this stuff, refactor maybe

  len = strlen(stext) + 1;
  if (!(text = (char*)malloc(sizeof(char) * len)))
    die("malloc");
  p = text;
  memcpy(text, stext, len);

  /* compute width of the status text */
  w = 0;
  i = -1;
  while (text[++i]) {
    if (text[i] == '^') {
      if (!isCode) {
        isCode  = 1;
        text[i] = '\0';
        w += TEXTW(text) - lrpad;
        text[i] = '^';
        if (text[++i] == 'f')
          w += atoi(text + ++i);
      } else {
        isCode = 0;
        text   = text + i + 1;
        i      = -1;
      }
    }
  }
  if (!isCode)
    w += TEXTW(text) - lrpad;
  else
    isCode = 0;
  text = p;

  w += 2; /* 1px padding on both sides */
  ret = x = m->ww - w - stw;

  drw_setscheme(drw, scheme[LENGTH(colors)]);
  Clr* scheme_ = scheme[m == selmon ? SchemeNorm : SchemeDark];

  drw->scheme[ColFg] = scheme_[ColFg];
  drw->scheme[ColBg] = scheme_[ColBg];
  drw_rect(drw, x, 0, w, bh, 1, 1);
  x++;

  /* process status text */
  i = -1;
  while (text[++i]) {
    if (text[i] == '^' && !isCode) {
      isCode = 1;

      text[i] = '\0';
      w       = TEXTW(text) - lrpad;
      drw_text(drw, x, 0, w, bh, 0, text, 0);

      x += w;

      /* process code */
      while (text[++i] != '^') {
        if (text[i] == 'c') {
          char buf[8];
          memcpy(buf, (char*)text + i + 1, 7);
          buf[7] = '\0';
          drw_clr_create(drw, &drw->scheme[ColFg], buf);
          i += 7;
        } else if (text[i] == 'b') {
          char buf[8];
          memcpy(buf, (char*)text + i + 1, 7);
          buf[7] = '\0';
          drw_clr_create(drw, &drw->scheme[ColBg], buf);
          i += 7;
        } else if (text[i] == 'd') {
          drw->scheme[ColFg] = scheme_[ColFg];
          drw->scheme[ColBg] = scheme_[ColBg];
        } else if (text[i] == 'w') {
          Clr swp;
          swp                = drw->scheme[ColFg];
          drw->scheme[ColFg] = drw->scheme[ColBg];
          drw->scheme[ColBg] = swp;
        } else if (text[i] == 'v') {
          oldfg = drw->scheme[ColFg];
          oldbg = drw->scheme[ColBg];
        } else if (text[i] == 't') {
          drw->scheme[ColFg] = oldfg;
          drw->scheme[ColBg] = oldbg;
        } else if (text[i] == 'r') {
          int rx = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int ry = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int rw = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int rh = atoi(text + ++i);

          drw_rect(drw, rx + x, ry, rw, rh, 1, 0);
        } else if (text[i] == 'f') {
          x += atoi(text + ++i);
        }
      }

      text   = text + i + 1;
      i      = -1;
      isCode = 0;
    }
  }

  if (!isCode) {
    w = TEXTW(text) - lrpad;
    drw_text(drw, x, 0, w, bh, 0, text, 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  free(p);

  return ret;
}

// update status in bar
void updatestatus(void) {
  Monitor* m;
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "nwm");
  for (m = mons; m; m = m->next)
    drawbar(m);
  updatesystray();
}

// update geometry of systray icons
void updatesystrayicongeom(Client* i, int w, int h) {
  if (i) {
    i->h = bh;
    if (w == h)
      i->w = bh;
    else if (h == bh)
      i->w = w;
    else
      i->w = (int)((float)bh * ((float)w / (float)h));
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    // force icons into the systray dimensions if they don't want to
    if (i->h > bh) {
      if (i->w == i->h)
        i->w = bh;
      else
        i->w = (int)((float)bh * ((float)i->w / (float)i->h));
      i->h = bh;
    }
  }
}

// update states of systray icons
void updatesystrayiconstate(Client* i, XPropertyEvent* ev) {
  long flags;
  int  code = 0;

  // TODO: comment
  if (!showsystray || !i || ev->atom != xatom[XembedInfo] || !(flags = getatomprop(i, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code    = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  } else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code    = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  } else
    return;
  sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
      systray->win, XEMBED_EMBEDDED_VERSION);
}

// update systray
void updatesystray(void) {
  XSetWindowAttributes wa;
  XWindowChanges       wc;
  Client*              i;
  Monitor*             m  = systraytomon(NULL);
  unsigned int         x  = m->mx + m->mw;
  unsigned int         sw = TEXTW(stext) - lrpad + systrayspacing;
  unsigned int         w  = 1;

  // TODO: comment
  if (!showsystray)
    return;
  if (systrayonleft)
    x -= sw;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray*)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win         = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
    wa.event_mask        = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char*)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(dpy, systray->win, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
    XMapRaised(dpy, systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
      XSync(dpy, False);
    } else {
      fprintf(stderr, "nwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
  wc.x          = x;
  wc.y          = m->by;
  wc.width      = w;
  wc.height     = bh;
  wc.stack_mode = Above;
  wc.sibling    = m->barwin;
  XConfigureWindow(dpy, systray->win, CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode, &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  /* redraw background */
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
  XSync(dpy, False);
}

// gets a systray icon client from its window
Client* wintosystrayicon(Window w) {
  Client* i = NULL;

  if (!showsystray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next)
    ;
  return i;
}

// remove system tray icon
void removesystrayicon(Client* i) {
  Client** ii;

  if (!showsystray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next)
    ;
  if (ii)
    *ii = i->next;
  free(i);
}

/* ------------------------ things to manage things ------------------------- */

// configure client by XConfigureEvent
void configure(Client* c) {
  XConfigureEvent ce;

  ce.type              = ConfigureNotify;
  ce.display           = dpy;
  ce.event             = c->win;
  ce.window            = c->win;
  ce.x                 = c->x;
  ce.y                 = c->y;
  ce.width             = c->w;
  ce.height            = c->h;
  ce.border_width      = c->bw;
  ce.above             = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent*)&ce);
}

// runtime error handler
int xerror(Display* dpy, XErrorEvent* ee) {
  /* There's no way to check accesses to destroyed windows, thus those cases are
  ** ignored (especially on UnmapNotify's). Other types of errors call Xlibs
  ** default error handler, which may call exit. */
  if (ee->error_code == BadWindow
      || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
      || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
      || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
      || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
      || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
      || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "nwm: fatal error: request code=%d, error code=%d\n",
      ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

// dummy error handler (for cleanup)
int xerrordummy(Display* dpy, XErrorEvent* ee) {
  return 0;
}

// startup error handler to check if another window manager is already running
int xerrorstart(Display* dpy, XErrorEvent* ee) {
  die("nwm: another window manager is already running");
  return -1;
}

// watch buttons presses
void grabbuttons(Client* c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
          BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button,
              buttons[i].mask | modifiers[j],
              c->win, False, BUTTONMASK,
              GrabModeAsync, GrabModeSync, None, None);
  }
}

// watch keys presses
void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
    KeyCode      code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
              True, GrabModeAsync, GrabModeAsync);
  }
}

// set right mask for numlock key (idk)
void updatenumlockmask(void) {
  unsigned int     i, j;
  XModifierKeymap* modmap;

  numlockmask = 0;
  modmap      = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j]
          == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

// TODO: figure it out
// return next monitor by direction (dir > 0 means next)
Monitor* dirtomon(int dir) {
  Monitor* m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

// get prop of an atom for client
Atom getatomprop(Client* c, Atom prop) {
  int            di;
  unsigned long  dl;
  unsigned char* p = NULL;
  Atom           da, atom = None;
  // TODO: getatomprop should return the number of items and a pointer to the stored data instead of this workaround
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
          &da, &di, &dl, &dl, &p)
          == Success
      && p) {
    atom = *(Atom*)p;
    if (da == xatom[XembedInfo] && dl == 2)
      atom = ((Atom*)p)[1];
    XFree(p);
  }
  return atom;
}

// get pointer to root window
int getrootptr(int* x, int* y) {
  int          di;
  unsigned int dui;
  Window       dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

// property event handler
void propertynotify(XEvent* e) {
  Client*         c;
  Window          trans;
  XPropertyEvent* ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
    } else
      updatesystrayiconstate(c, ev);
    resizebarwin(selmon);
    updatesystray();
  }
  if ((ev->window == root) && (ev->atom == XA_WM_NAME))
    updatestatus();
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) && (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      updatesizehints(c);
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      drawtabs();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
      drawtab(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

// get text property (??)
int gettextprop(Window w, Atom atom, char* text, unsigned int size) {
  char**        list = NULL;
  int           n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING)
    strncpy(text, (char*)name.value, size - 1);
  else {
    if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif // XINERAMA

// clean up any zombies
void sigchld(int unused) {
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
}

/* ------------------------ client/monitor managing ------------------------- */

// update list of registered clients
void updateclientlist() {
  Client*  c;
  Monitor* m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList],
          XA_WINDOW, 32, PropModeAppend,
          (unsigned char*)&(c->win), 1);
}

// update geometry
int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int                 i, j, n, nn;
    Client*             c;
    Monitor*            m;
    XineramaScreenInfo* info   = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo* unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    if (n <= nn) { /* new monitors available */
      for (i = 0; i < (nn - n); i++) {
        for (m = mons; m && m->next; m = m->next)
          ;
        if (m)
          m->next = createmon();
        else
          mons = createmon();
      }
      for (i = 0, m = mons; i < nn && m; m = m->next, i++)
        if (i >= n
            || unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw || unique[i].height != m->mh) {
          dirty  = 1;
          m->num = i;
          m->mx = m->wx = unique[i].x_org;
          m->my = m->wy = unique[i].y_org;
          m->mw = m->ww = unique[i].width;
          m->mh = m->wh = unique[i].height;
          updatebarpos(m);
        }
    } else { /* less monitors available nn < n */
      for (i = nn; i < n; i++) {
        for (m = mons; m && m->next; m = m->next)
          ;
        while ((c = m->clients)) {
          dirty      = 1;
          m->clients = c->next;
          detachstack(c);
          c->mon = mons;
          attach(c);
          attachstack(c);
        }
        if (m == selmon)
          selmon = mons;
        cleanupmon(m);
      }
    }
    free(unique);
  } else
#endif // XINERAMA
  {    /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty    = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

// update client's title
void updatetitle(Client* c) {
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

// update client's window type
void updatewindowtype(Client* c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

// update client's wm hints
void updatewmhints(Client* c) {
  XWMHints* wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

// make a client from window
Client* wintoclient(Window w) {
  Client*  c;
  Monitor* m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

// make a monitor from window
Monitor* wintomon(Window w) {
  int      x, y;
  Client*  c;
  Monitor* m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin || w == m->tabwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

// throws an error if some other window manager is running
void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

// initialize wm
void setup(void) {
  int                  i;
  XSetWindowAttributes wa;
  Atom                 utf8string;

  sigchld(0); // clean up any zombies immediately

  // init screen
  screen = DefaultScreen(dpy);
  sw     = DisplayWidth(dpy, screen);
  sh     = DisplayHeight(dpy, screen);
  root   = RootWindow(dpy, screen);
  drw    = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h * barspacing_font + 2 * barspacing;
  bh    = drw->fonts->h + 2 * barmargins; // TODO: add setting to set paddings
  th    = bh;
  updategeom();

  // init atoms
  utf8string                            = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols]                   = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete]                      = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState]                       = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus]                   = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow]              = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported]                 = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray]                = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP]              = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation]     = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName]                    = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState]                   = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck]                   = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen]              = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType]              = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog]        = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList]                = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  xatom[Manager]                        = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed]                         = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo]                     = XInternAtom(dpy, "_XEMBED_INFO", False);

  // init cursors
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove]   = drw_cur_create(drw, XC_fleur);

  // init appearance
  scheme                 = ecalloc(LENGTH(colors) + 1, sizeof(Clr*));
  scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], 3);
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);

  // init system tray
  updatesystray();
  // init bars
  updatebars();
  updatestatus();

  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
      PropModeReplace, (unsigned char*)"nwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*)&wmcheckwin, 1);

  // EWMH support per view
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
      PropModeReplace, (unsigned char*)netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);

  // select events
  wa.cursor     = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
                | ButtonPressMask | PointerMotionMask | EnterWindowMask
                | LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

// main event loop, active when running != false
void run(void) {
  XEvent ev;
  XSync(dpy, False); // all calls should be executed before continue

  // blocks when is no events in queue, continues by one event, removing it from queue
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); // handle an event to its function if there is one
}

Monitor* createmon(void) {
  Monitor*     m;
  unsigned int i;

  m            = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact                    = mfact;
  m->nmaster                  = nmaster;
  m->showbar                  = showbar;
  m->showtab                  = showtab;
  m->topbar                   = topbar;
  m->toptab                   = toptab;
  m->ntabs                    = 0;
  m->lt[0]                    = &layouts[0];
  m->lt[1]                    = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  m->pertag         = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i]   = m->mfact;

    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i]    = m->sellt;

    m->pertag->showbars[i] = m->showbar;
  }

  // apply pertag rules
  for (i = 0; i < LENGTH(pertagrules); i++) {
    if (pertagrules[i].mfact > 0) {
      m->pertag->mfacts[pertagrules[i].tag + 1] = pertagrules[i].mfact;
    }
    m->pertag->ltidxs[pertagrules[i].tag + 1][0] = &layouts[pertagrules[i].layout];
  }

  return m;
}

// XXX: not there
// check if current setup is valid and ready
void scan(void) {
  unsigned int      i, num;
  Window            d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa)
          || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1)
          && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

// apply rules based on config
void applyrules(Client* c) {
  const char* class, *instance;
  unsigned int i;
  const Rule*  r;
  Monitor*     m;
  XClassHint   ch = { NULL, NULL };

  c->isfloating = 0;
  c->tags       = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  // rule matching & applying
  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title))
        && (!r->class || strstr(class, r->class))
        && (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }

  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);

  // apply tagmask if can
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

// resize client using size hints (sticky stuff)
int applysizehints(Client* c, int* x, int* y, int* w, int* h, int interact) {
  int      baseismin;
  Monitor* m = c->mon;

  // set minimum possible
  *w = MAX(1, *w);
  *h = MAX(1, *h);

  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

// arrange everything
void arrange(Monitor* m) {
  if (m)
    showhide(m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(m);
}

// arrange monitor
void arrangemon(Monitor* m) {
  updatebarpos(m);
  XMoveResizeWindow(dpy, m->tabwin, m->wx, m->ty, m->ww, th);
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

// attach client to stack
void attach(Client* c) {
  c->next         = c->mon->clients;
  c->mon->clients = c;
}

// attach stack to stack
void attachstack(Client* c) {
  c->snext      = c->mon->stack;
  c->mon->stack = c;
}

// detach client from stack
void detach(Client* c) {
  Client** tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

// detack stack from stacks stack (idk)
void detachstack(Client* c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

// focus some client
void focus(Client* c) {
  // TODO: comment
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
  drawtabs();
}

// get client's wm state
long getstate(Window w) {
  int            format;
  long           result = -1;
  unsigned char* p      = NULL;
  unsigned long  n, extra;
  Atom           real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
          &real, &format, &n, &extra, (unsigned char**)&p)
      != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

// manage a window (big ol' chunky procedure)
void manage(Window w, XWindowAttributes* wa) {
  Client *       c, *t = NULL;
  Window         trans = None;
  XWindowChanges wc;

  c      = ecalloc(1, sizeof(Client));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw       = wa->border_width;

  updatetitle(c);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon  = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
  }

  if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
    c->x = c->mon->mx + c->mon->mw - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
    c->y = c->mon->my + c->mon->mh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->mx);
  /* only fix client y-offset, if the client center might cover the bar */
  c->y  = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
                       && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww))
                        ? bh
                        : c->mon->my);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
  c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
  XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
      (unsigned char*)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocus(selmon->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  focus(NULL);
}

// get next tiled client in stack
Client* nexttiled(Client* c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

// pop a client in the stack
void pop(Client* c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

// make monitor from rectangle
Monitor* recttomon(int x, int y, int w, int h) {
  Monitor *m, *r   = selmon;
  int      a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r    = m;
    }
  return r;
}

// resize client (wrapper w/ sizehints)
void resize(Client* c, int x, int y, int w, int h, int interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

// actual resize client
void resizeclient(Client* c, int x, int y, int w, int h) {
  XWindowChanges wc;
  unsigned int   n;
  unsigned int   gapoffset;
  unsigned int   gapincr;
  Client*        nbc;

  wc.border_width = c->bw;

  /* Get number of clients for the client's monitor */
  for (n = 0, nbc = nexttiled(c->mon->clients); nbc; nbc = nexttiled(nbc->next), n++)
    ;

  /* Do nothing if layout is floating */
  if (c->isfloating || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    gapincr = gapoffset = 0;
  } else {
    /* Remove border and gap if layout is monocle or only one client */
    if (c->mon->lt[c->mon->sellt]->arrange == monocle || n == 1) {
      gapoffset       = 0;
      gapincr         = -2 * borderpx;
      wc.border_width = 0;
    } else {
      gapoffset = gappx;
      gapincr   = 2 * gappx;
    }
  }

  c->oldx = c->x;
  c->x = wc.x = x + gapoffset;
  c->oldy     = c->y;
  c->y = wc.y = y + gapoffset;
  c->oldw     = c->w;
  c->w = wc.width = w - gapincr;
  c->oldh         = c->h;
  c->h = wc.height = h - gapincr;

  XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

// restack things on a monitor
void restack(Monitor* m) {
  Client*        c;
  XEvent         ev;
  XWindowChanges wc;

  drawbar(m);
  drawtab(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling    = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

// send client to monitor
void sendmon(Client* c, Monitor* m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon  = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

// set state of a client
void setclientstate(Client* c, long state) {
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
      PropModeReplace, (unsigned char*)data, 2);
}

// set client as focused
void setfocus(Client* c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char*)&(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

// set client as fullscreen
void setfullscreen(Client* c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate     = c->isfloating;
    c->oldbw        = c->bw;
    c->bw           = 0;
    c->isfloating   = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = 0;
    c->isfloating   = c->oldstate;
    c->bw           = c->oldbw;
    c->x            = c->oldx;
    c->y            = c->oldy;
    c->w            = c->oldw;
    c->h            = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

// set client as urgent
void seturgent(Client* c, int urg) {
  XWMHints* wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

// show/hide a client
void showhide(Client* c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

// unfocus a client
void unfocus(Client* c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

// unmanage a client
void unmanage(Client* c, int destroyed) {
  Monitor*       m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

// update size hints 🗿
void updatesizehints(Client* c) {
  long       msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

/* -------------------------------- layouts --------------------------------- */

/*
// bottom stack layout
static void bstack(Monitor* m) {
  int          w, h, mh, mx, tx, ty, tw;
  unsigned int i, n;
  Client*      c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;
  if (n > m->nmaster) {
    mh = m->nmaster ? m->mfact * m->wh : 0;
    tw = m->ww / (n - m->nmaster);
    ty = m->wy + mh;
  } else {
    mh = m->wh;
    tw = m->ww;
    ty = m->wy;
  }
  for (i = mx = 0, tx = m->wx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
    if (i < m->nmaster) {
      w = (m->ww - mx) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx + mx, m->wy, w - (2 * c->bw), mh - (2 * c->bw), 0);
      mx += WIDTH(c);
    } else {
      h = m->wh - mh;
      resize(c, tx, ty, tw - (2 * c->bw), h - (2 * c->bw), 0);
      if (tw != m->ww)
        tx += WIDTH(c);
    }
  }
}
*/

// tile layout (canonical)
void tile(Monitor* m) {
  unsigned int i, n, h, mw, my, ty;
  Client*      c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c);
    }
}

// monocle layout
void monocle(Monitor* m) {
  unsigned int n = 0;
  Client*      c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) // override layout symbol with clients count
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

/* -------------------------------- xevents --------------------------------- */

// ConfigureNotify event handler
void configurenotify(XEvent* e) {
  Monitor*         m;
  XConfigureEvent* ev = &e->xconfigure;
  int              dirty;

  // TODO: updategeom handling sucks, needs to be simplified
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw    = ev->width;
    sh    = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
      }
      resizebarwin(m);
    }
    focus(NULL);
    arrange(NULL);
  }
}

// DestroyNotify event handler
void destroynotify(XEvent* e) {
  Client*              c;
  XDestroyWindowEvent* ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  else if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    resizebarwin(selmon);
    updatesystray();
  }
}

// ConfigureRequest event handler
void configurerequest(XEvent* e) {
  Client*                 c;
  Monitor*                m;
  XConfigureRequestEvent* ev = &e->xconfigurerequest;
  XWindowChanges          wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x    = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y    = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w    = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h    = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x            = ev->x;
    wc.y            = ev->y;
    wc.width        = ev->width;
    wc.height       = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling      = ev->above;
    wc.stack_mode   = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

// handle key press event
void keypress(XEvent* e) {
  unsigned int i;
  KeySym       keysym;
  XKeyEvent*   ev;

  ev     = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

// handle button press event
void buttonpress(XEvent* e) {
  unsigned int         i, x, click, occ = 0;
  Arg                  arg = { 0 };
  Client*              c;
  Monitor*             m;
  XButtonPressedEvent* ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  if (ev->window == selmon->barwin) {
    i = x = 0;
    for (c = m->clients; c; c = c->next)
      occ |= c->tags == 255 ? 0 : c->tags;
    do {
      /* do not reserve space for vacant tags */
      if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
        continue;
      x += TEXTW(tags[i]);
    } while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click  = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + blw)
      click = ClkLtSymbol;
    else if (ev->x > selmon->ww - (int)TEXTW(stext) - getsystraywidth())
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  }
  if (ev->window == selmon->tabwin) {
    i = 0;
    x = 0;
    for (c = selmon->clients; c; c = c->next) {
      if (!ISVISIBLE(c))
        continue;
      x += selmon->tab_widths[i];
      if (ev->x > x)
        ++i;
      else
        break;
      if (i >= m->ntabs)
        break;
    }
    if (c) {
      click  = ClkTabBar;
      arg.ui = i;
    }
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
        && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(((click == ClkTagBar || click == ClkTabBar)
                          && buttons[i].arg.i == 0)
                          ? &arg
                          : &buttons[i].arg);
}

// handle client messages
void clientmessage(XEvent* e) {
  XWindowAttributes    wa;
  XSetWindowAttributes swa;
  XClientMessageEvent* cme = &e->xclient;
  Client*              c   = wintoclient(cme->window);

  // handle systray icon clients
  if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(c = (Client*)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(c->win = cme->data.l[2])) {
        free(c);
        return;
      }
      // add to tray stack
      c->mon         = selmon;
      c->next        = systray->icons;
      systray->icons = c;
      if (!XGetWindowAttributes(dpy, c->win, &wa)) {
        // use sane defaults
        wa.width        = bh;
        wa.height       = bh;
        wa.border_width = 0;
      }
      // set props
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw       = wa.border_width;
      c->bw          = 0;
      c->isfloating  = True;
      // reuse tags field as mapped status
      c->tags = 1;
      // make it brrrrr
      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      // use parents background color
      swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      // FIXME: not sure if I have to send these events, too
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      // make it brrrrr: fin
      XSync(dpy, False);
      resizebarwin(selmon);
      updatesystray();
      setclientstate(c, NormalState);
    }
    return;
  }
  // no client - no problem
  if (!c)
    return;
  // handle state
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen]
        || cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1        /* _NET_WM_STATE_ADD    */
                           || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */
                               && !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent) {
      seturgent(c, 1);
      drawbar(c->mon);
    }
  }
}

// EnterNotify event handler
void enternotify(XEvent* e) {
  Client*         c;
  Monitor*        m;
  XCrossingEvent* ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel)
    return;
  focus(c);
}

// expose bar event handler
void expose(XEvent* e) {
  Monitor*      m;
  XExposeEvent* ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    drawtab(m);
    if (m == selmon)
      updatesystray();
  }
}

// FocusIn event handler
void focusin(XEvent* e) { // TODO: there are some broken focus acquiring clients needing extra handling
  XFocusChangeEvent* ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

// keyboard mapping event handler
void mappingnotify(XEvent* e) {
  XMappingEvent* ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

// map request event handler
void maprequest(XEvent* e) {
  static XWindowAttributes wa;
  XMapRequestEvent*        ev = &e->xmaprequest;
  Client*                  i;
  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    resizebarwin(selmon);
    updatesystray();
  }

  if (!XGetWindowAttributes(dpy, ev->window, &wa))
    return;
  if (wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

// motion event handler
void motionnotify(XEvent* e) {
  static Monitor* mon = NULL;
  Monitor*        m;
  XMotionEvent*   ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, True);
    selmon = m;
    focus(NULL);
  }
  mon = m;
}

// resize request event handler
void resizerequest(XEvent* e) {
  XResizeRequestEvent* ev = &e->xresizerequest;
  Client*              i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    resizebarwin(selmon);
    updatesystray();
  }
}

// unmapping event handler
void unmapnotify(XEvent* e) {
  Client*      c;
  XUnmapEvent* ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  } else if ((c = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(dpy, c->win);
    updatesystray();
  }
}

// send x event
int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4) {
  int    n;
  Atom * protocols, mt;
  int    exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt     = proto;
  }
  if (exists) {
    ev.type                 = ClientMessage;
    ev.xclient.window       = w;
    ev.xclient.message_type = mt;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = d0;
    ev.xclient.data.l[1]    = d1;
    ev.xclient.data.l[2]    = d2;
    ev.xclient.data.l[3]    = d3;
    ev.xclient.data.l[4]    = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}
