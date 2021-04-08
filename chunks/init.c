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
      PropModeReplace, (unsigned char*)"dwm", 3);
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
  m->topbar                   = topbar;
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
