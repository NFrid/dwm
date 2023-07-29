#include "xevents.h"
#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "monitor.h"
#include "util.h"
#include "variables.h"

// ConfigureNotify event handler
void configurenotify(XEvent* e) {
  Monitor*         m;
  XConfigureEvent* ev = &e->xconfigure;
  int              dirty;

  // TODO: updategeom handling sucks, needs to be simplified
  if (ev->window == root) {
    dirty   = (screenw != ev->width || screenh != ev->height);
    screenw = ev->width;
    screenh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, screenw, barh);
      updatebars();
      for (m = mons; m; m = m->next) {
        XMoveResizeWindow(dpy, m->barwin, m->wx, m->bary, m->ww, barh);
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
      if (!c->nocenter) {
        if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
          c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
        if ((c->y + c->h) > m->my + m->mh && c->isfloating)
          c->y
              = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      }
      if ((ev->value_mask & (CWX | CWY))
          && !(ev->value_mask & (CWWidth | CWHeight)))
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
  keysym = XkbKeycodeToKeysym(dpy, ev->keycode, 0, 0);
  for (i = 0; keys[i].func != NULL; i++)
    if (keysym == keys[i].keysym
        && cleanmask(keys[i].mod) == cleanmask(ev->state) && keys[i].func)
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
    } while (ev->x >= x && ++i < TAGS_N);
    if (i < TAGS_N) {
      click  = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + barw)
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
  for (i = 0; buttons[i].func != NULL; i++)
    if (click == buttons[i].click && buttons[i].func
        && buttons[i].button == ev->button
        && cleanmask(buttons[i].mask) == cleanmask(ev->state))
      buttons[i].func(
          ((click == ClkTagBar || click == ClkTabBar) && buttons[i].arg.i == 0)
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
  if (showsystray && cme->window == systray->win
      && cme->message_type == netatom[NetSystemTrayOP]) {
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
        wa.width        = barh;
        wa.height       = barh;
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
      XSelectInput(dpy, c->win,
          StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      // use parents background color
      swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
          XEMBED_EMBEDDED_NOTIFY, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      // FIXME: not sure if I have to send these events, too
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
          XEMBED_FOCUS_IN, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
          XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
          XEMBED_MODALITY_ON, 0, systray->win, XEMBED_EMBEDDED_VERSION);
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

  if (ignorenotify) {
    ignorenotify = False;
    return;
  }

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior)
      && ev->window != root)
    return;

  c = wintoclient(ev->window);

  // to prevent shitty wine apps' popups from refocusing back to main window
  // if (selmon->sel && selmon->sel->isfloating && c->isfloating)
  //   return;

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
void focusin(XEvent* e) { // TODO: there are some broken focus acquiring clients
                          // needing extra handling
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
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
        XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
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
int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2,
    long d3, long d4) {
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

// handle XEvent to its function
void (*handler[LASTEvent])(XEvent*) = { [ButtonPress] = buttonpress,
  [ClientMessage]                                     = clientmessage,
  [ConfigureRequest]                                  = configurerequest,
  [ConfigureNotify]                                   = configurenotify,
  [DestroyNotify]                                     = destroynotify,
  [EnterNotify]                                       = enternotify,
  [Expose]                                            = expose,
  [FocusIn]                                           = focusin,
  [KeyPress]                                          = keypress,
  [MappingNotify]                                     = mappingnotify,
  [MapRequest]                                        = maprequest,
  [MotionNotify]                                      = motionnotify,
  [PropertyNotify]                                    = propertynotify,
  [ResizeRequest]                                     = resizerequest,
  [UnmapNotify]                                       = unmapnotify };
