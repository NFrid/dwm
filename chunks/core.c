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
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
      ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

// dummy error handler (for cleanup)
int xerrordummy(Display* dpy, XErrorEvent* ee) {
  return 0;
}

// startup error handler to check if another window manager is already running
int xerrorstart(Display* dpy, XErrorEvent* ee) {
  die("dwm: another window manager is already running");
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
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
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
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}
