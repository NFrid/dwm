#include "actions.h"
#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "monitor.h"
#include "stack.h"
#include "util.h"
#include "variables.h"
#include "xevents.h"

static unsigned int numlockmask = 0;

// runtime error handler
int xerror(Display* dpy, XErrorEvent* ee) {
  /* There's no way to check accesses to destroyed windows, thus those cases are
  ** ignored (especially on UnmapNotify's). Other types of errors call Xlibs
  ** default error handler, which may call exit. */
  if (ee->error_code == BadWindow
      || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
      || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolyFillRectangle
          && ee->error_code == BadDrawable)
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
int xerrordummy(Display* dpy, XErrorEvent* ee) { return 0; }

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
    unsigned int modifiers[]
        = { 0, LockMask, numlockmask, numlockmask | LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
          GrabModeSync, GrabModeSync, None, None);
    for (i = 0; buttons[i].func != NULL; i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j],
              c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None,
              None);
  }
}

// watch keys presses
void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[]
        = { 0, LockMask, numlockmask, numlockmask | LockMask };
    KeyCode code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; keys[i].func != NULL; i++)
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True,
              GrabModeAsync, GrabModeAsync);
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

// get prop of an atom for client
Atom getatomprop(Client* c, Atom prop) {
  int            di;
  unsigned long  dl;
  unsigned char* p = NULL;
  Atom           da, atom = None;
  // TODO: getatomprop should return the number of items and a pointer to the
  // stored data instead of this workaround
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req, &da,
          &di, &dl, &dl, &p)
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
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans))
          && (c->isfloating = (wintoclient(trans)) != NULL))
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
    if (ev->atom == motifatom)
      updatemotifhints(c);
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
    if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0
        && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

// clean up any zombies
void sigchld(int unused) {
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
}

// update list of registered clients
void updateclientlist() {
  Client*  c;
  Monitor* m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
          PropModeAppend, (unsigned char*)&(c->win), 1);
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
        if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my
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
    if (mons->mw != screenw || mons->mh != screenh) {
      dirty    = 1;
      mons->mw = mons->ww = screenw;
      mons->mh = mons->wh = screenh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
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

// return unmasked keypress
unsigned int cleanmask(unsigned int masked) {
  return masked & ~(numlockmask | LockMask)
       & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask
           | Mod5Mask);
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
  screen  = DefaultScreen(dpy);
  screenw = DisplayWidth(dpy, screen);
  screenh = DisplayHeight(dpy, screen);
  root    = RootWindow(dpy, screen);
  drw     = drw_create(dpy, screen, root, screenw, screenh);
  if (!drw_fontset_create(drw, fonts, fonts_len))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h * barspacing_font + 2 * barspacing;
  barh  = drw->fonts->h + 2 * barmargins; // TODO: add setting to set paddings
  tabh  = barh;
  updategeom();

  // init atoms
  utf8string               = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols]      = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete]         = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState]          = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus]      = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported]    = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray]   = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation]
      = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz]
      = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName]  = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen]
      = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog]
      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetWMWindowTypeDesktop]
      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  motifatom              = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
  xatom[Manager]         = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed]          = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo]      = XInternAtom(dpy, "_XEMBED_INFO", False);

  // init cursors
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove]   = drw_cur_create(drw, XC_fleur);

  // init appearance
  scheme             = ecalloc(colors_len + 1, sizeof(Clr*));
  scheme[colors_len] = drw_scm_create(drw, colors[0], 3);
  for (i = 0; i < colors_len; i++)
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

// check if current setup is valid and ready
void scan(void) {
  unsigned int      i, num;
  Window            d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect
          || XGetTransientForHint(dpy, wins[i], &d1))
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

// main event loop, active when running != false
void run(void) {
  XEvent ev;
  XSync(dpy, False); // all calls should be executed before continue

  // blocks when is no events in queue, continues by one event, removing it from
  // queue
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); // handle an event to its function if there is one
}

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
  for (i = 0; i < colors_len + 1; i++)
    free(scheme[i]);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

#ifdef XINERAMA
int isuniquegeom(
    XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif // XINERAMA
