#include "monitor.h"
#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "stack.h"
#include "util.h"
#include "variables.h"

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

/* ------------------------ client/monitor managing ------------------------- */

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
  m->lt[1]                    = &layouts[1 % layouts_len];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  m->pertag         = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= TAGS_N; i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i]   = m->mfact;

    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i]    = m->sellt;

    m->pertag->showbars[i] = m->showbar;
  }

  // apply pertag rules
  for (i = 0; i < pertagrules_len; i++) {
    if (pertagrules[i].mfact > 0) {
      m->pertag->mfacts[pertagrules[i].tag + 1] = pertagrules[i].mfact;
    }
    m->pertag->ltidxs[pertagrules[i].tag + 1][0]
        = &layouts[pertagrules[i].layout];
  }

  return m;
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
  XMoveResizeWindow(dpy, m->tabwin, m->wx, m->taby, m->ww, tabh);
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

// get window's wm state
long getstate(Window w) {
  int            format;
  long           result = -1;
  unsigned char* p      = NULL;
  unsigned long  n, extra;
  Atom           real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
          wmatom[WMState], &real, &format, &n, &extra, (unsigned char**)&p)
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
  c->y = MAX(
      c->y, ((c->mon->bary == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
                && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww))
                ? barh
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
  XSelectInput(dpy, w,
      EnterWindowMask | FocusChangeMask | PropertyChangeMask
          | StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
      PropModeAppend, (unsigned char*)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * screenw, c->y, c->w,
      c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocus(selmon->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  focus(NULL);
}

// make monitor from rectangle
Monitor* recttomon(int x, int y, int w, int h) {
  Monitor *m, *r   = selmon;
  int      a, area = 0;
  int      deltawy = showbar * selmon->showbar * barh;

  for (m = mons; m; m = m->next)
    if ((a = intersect(
             x, y, w, h, m->wx, m->wy - deltawy, m->ww, m->wh + deltawy))
        > area) {
      area = a;
      r    = m;
    }
  return r;
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
