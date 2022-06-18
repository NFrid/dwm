#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "layouts.h"
#include "util.h"
#include "variables.h"
#include "xevents.h"

void drawbar(Monitor* mon) {
  unsigned i;

  int      x = 0; // drawing cursor position
  unsigned w = 0; // width of the thing

  unsigned status_w = 0; // statusbar width
  unsigned systr_w  = 0; // systray width

  unsigned boxs = drw->fonts->h / 9;     // box size
  unsigned boxw = drw->fonts->h / 6 + 2; // box width

  unsigned occ = 0; // occupied tags mask
  unsigned urg = 0; // urgent tags mask

  if (showsystray && mon == systraytomon(mon) && !systrayonleft)
    systr_w = getsystraywidth();

  // draw status first so it can be overdrawn by tags later
  if (mon == selmon || 1) { // NOTE: draw status on every monitor
    status_w = mon->ww - drawstatusbar(mon, barh, stext, systr_w);
  }

  resizebarwin(mon);

  // get tag state masks
  for (Client* c = mon->clients; c; c = c->next) {
    occ |= c->tags == 255 ? 0 : c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }

  // draw tags
  for (i = 0; i < TAGS_N; i++) {
    // do not draw vacant tags
    if (!(occ & 1 << i || mon->tagset[mon->seltags] & 1 << i))
      continue;

    w = TEXTW(tags[i]);

    drw_setscheme(drw, scheme[mon->tagset[mon->seltags] & 1 << i
                                  ? SchemeSel
                              : mon == selmon
                                  ? SchemeNorm
                                  : SchemeDark]);
    drw_text(drw, x, 0, w, barh, lrpad / 2, tags[i], urg & 1 << i);
    x += w;
  }

  // draw layout symbol
  w = barw = TEXTW(mon->ltsymbol);
  drw_setscheme(drw, scheme[mon == selmon ? SchemeNorm : SchemeDark]);
  x = drw_text(drw, x, 0, w, barh, lrpad / 2, mon->ltsymbol, 0);

  if (debuginfo[0]) {
    // draw debug info
    x = drw_text(drw, x, 0, TEXTW(debuginfo), barh, lrpad / 2, debuginfo, 0);
  }

  // draw selected client
  if ((w = mon->ww - status_w - x) > barh) {
    if (mon->sel) {
      drw_setscheme(drw, scheme[mon == selmon ? SchemeSel : SchemeInv]);
      drw_text(drw, x, 0, w, barh, lrpad / 2, mon->sel->name, 0);

      // draw its status icons
      if (mon->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, mon->sel->isfixed, 0);
      if (mon->sel->issticky)
        drw_polygon(drw,
            x + boxs, mon->sel->isfloating ? boxs * 2 + boxw : boxs,
            stickyiconbb.x, stickyiconbb.y,
            boxw, boxw * stickyiconbb.y / stickyiconbb.x,
            stickyicon, stickyicon_len,
            Nonconvex, mon->sel->tags & mon->tagset[mon->seltags]);

    } else { // when no clients selected
      drw_setscheme(drw, scheme[mon == selmon ? SchemeNorm : SchemeInv]);
      drw_rect(drw, x, 0, w, barh, 1, 1);
    }
  }

  // draw it finally
  drw_map(drw, mon->barwin, 0, 0, mon->ww, barh);
}

void drawbars(void) {
  for (Monitor* mon = mons; mon; mon = mon->next)
    drawbar(mon);
}

void updatebars(void) {
  unsigned w;

  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixmap = ParentRelative,
    .event_mask        = ButtonPressMask | ExposureMask
  };
  XClassHint ch = { "nwm", "nwm" };

  for (Monitor* mon = mons; mon; mon = mon->next) {
    if (mon->barwin)
      continue;

    w = mon->ww;

    if (showsystray && mon == systraytomon(mon))
      w -= getsystraywidth();

    mon->barwin = XCreateWindow(dpy, root,
        mon->wx, mon->bary, w, barh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, mon->barwin, cursor[CurNormal]->cursor);
    if (showsystray && mon == systraytomon(mon))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, mon->barwin);

    mon->tabwin = XCreateWindow(dpy, root,
        mon->wx, mon->taby, mon->ww, tabh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, mon->tabwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, mon->tabwin);

    XSetClassHint(dpy, mon->barwin, &ch);
  }
}

void drawtabs(void) {
  for (Monitor* mon = mons; mon; mon = mon->next)
    drawtab(mon);
}

void drawtab(Monitor* mon) {
  // TODO: comment
  Client*  c;
  unsigned i;

  int      itag = -1;
  char     view_info[50];
  unsigned view_info_w = 0;
  unsigned sorted_label_widths[MAXTABS];
  unsigned tot_width;
  unsigned maxsize = barh;

  int      x = 0; // drawing cursor position
  unsigned w = 0; // width of the thing

  // view_info: indicate the tag which is displayed in the view
  for (i = 0; i < TAGS_N; ++i) {
    if ((selmon->tagset[selmon->seltags] >> i) & 1) {
      if (itag >= 0) { // more than one tag selected
        itag = -1;
        break;
      }
      itag = i;
    }
  }

  view_info[sizeof(view_info) - 1] = 0;
  view_info_w                      = TEXTW(view_info);
  tot_width                        = view_info_w;

  /* Calculates number of labels and their width */
  mon->ntabs = 0;
  for (c = mon->clients; c; c = c->next) {
    if (!ISVISIBLE(c))
      continue;
    mon->tab_widths[mon->ntabs] = TEXTW(c->name);
    tot_width += mon->tab_widths[mon->ntabs];
    ++mon->ntabs;
    if (mon->ntabs >= MAXTABS)
      break;
  }

  if (tot_width > mon->ww) { // not enough space to display the labels, they need to be truncated
    memcpy(sorted_label_widths, mon->tab_widths, sizeof(int) * mon->ntabs);
    qsort(sorted_label_widths, mon->ntabs, sizeof(int), cmpint);
    tot_width = view_info_w;
    for (i = 0; i < mon->ntabs; ++i) {
      if (tot_width + (int)(mon->ntabs - i) * sorted_label_widths[i] > mon->ww)
        break;
      tot_width += sorted_label_widths[i];
    }
    maxsize = (mon->ww - tot_width) / (mon->ntabs - i);
  } else {
    maxsize = mon->ww;
  }
  i = 0;
  for (c = mon->clients; c; c = c->next) {
    if (!ISVISIBLE(c))
      continue;
    if (i >= mon->ntabs)
      break;
    if (mon->tab_widths[i] > maxsize)
      mon->tab_widths[i] = maxsize;
    w = mon->tab_widths[i];
    drw_setscheme(drw, (c == mon->sel) ? scheme[SchemeSel] : scheme[SchemeNorm]);
    drw_text(drw, x, 0, w, tabh, lrpad / 2, c->name, 0);
    x += w;
    ++i;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);

  /* cleans interspace between window names and current viewed tag label */
  w = mon->ww - view_info_w - x;
  drw_text(drw, x, 0, w, tabh, lrpad / 2, "", 0);

  /* view info */
  x += w;
  w = view_info_w;
  drw_text(drw, x, 0, w, tabh, lrpad / 2, view_info, 0);

  drw_map(drw, mon->tabwin, 0, 0, mon->ww, tabh);
}

void updatebarpos(Monitor* mon) {
  mon->wy = mon->my;
  mon->wh = mon->mh;
  if (mon->showbar) {
    mon->wh -= barh;
    mon->bary = mon->topbar ? mon->wy : mon->wy + mon->wh;

    if (mon->topbar)
      mon->wy += barh;

  } else {
    mon->bary = -barh;
  }

  updatetabspos(mon);
}

void updatetabspos(Monitor* mon) {
  unsigned nvis = 0; // count of invisible clients

  for (Client* c = mon->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      ++nvis;
  }

  if (mon->showtab == showtab_always
      || ((mon->showtab == showtab_auto)
          && (nvis > 1)
          && (mon->lt[mon->sellt]->arrange == monocle))) {
    mon->wh -= tabh;
    mon->taby = mon->toptab ? mon->wy : mon->wy + mon->wh;

    if (mon->toptab)
      mon->wy += tabh;

  } else {
    mon->taby = -tabh;
  }
}

void resizebarwin(Monitor* mon) {
  unsigned w = mon->ww;

  if (showsystray && mon == systraytomon(mon) && !systrayonleft)
    w -= getsystraywidth();

  XMoveResizeWindow(dpy, mon->barwin, mon->wx, mon->bary, w, barh);
}

Monitor* systraytomon(Monitor* mon) {
  Monitor* res;
  unsigned i, n;

  if (!systraypinning) {
    if (!mon)
      return selmon;

    return mon == selmon ? mon : NULL;
  }

  for (n = 1, res = mons;
       res && res->next;
       n++, res = res->next)
    ;
  for (i = 1, res = mons;
       res && res->next && i < systraypinning;
       i++, res = res->next)
    ;

  if (systraypinningfailfirst && n < systraypinning)
    return mons;

  return res;
}

unsigned getsystraywidth() {
  int     w = 0;
  Client* icon;

  if (showsystray)
    for (icon = systray->icons;
         icon; w += icon->w + systrayspacing, icon = icon->next)
      ;

  return w ? w + systrayspacing : 1;
}

int drawstatusbar(Monitor* mon, unsigned barh, char* stext, unsigned statusw) {
  int   res, i;
  int   w;
  int   x, len;
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
  res = x = mon->ww - w - statusw;

  drw_setscheme(drw, scheme[colors_len]);
  Clr* scheme_ = scheme[mon == selmon ? SchemeNorm : SchemeDark];

  drw->scheme[ColFg] = scheme_[ColFg];
  drw->scheme[ColBg] = scheme_[ColBg];
  drw_rect(drw, x, 0, w, barh, 1, 1);
  x++;

  /* process status text */
  i = -1;
  while (text[++i]) {
    if (text[i] == '^' && !isCode) {
      isCode = 1;

      text[i] = '\0';
      w       = TEXTW(text) - lrpad;
      drw_text(drw, x, 0, w, barh, 0, text, 0);

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
    drw_text(drw, x, 0, w, barh, 0, text, 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  free(p);

  return res;
}

void updatestatus(void) {
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "nwm");

  for (Monitor* mon = mons; mon; mon = mon->next)
    drawbar(mon);

  updatesystray();
}

void updatesystrayicongeom(Client* icon, unsigned w, unsigned h) {
  if (!icon)
    return;

  icon->h = barh;
  if (w == h) {
    icon->w = barh;
  } else if (h == barh) {
    icon->w = w;
  } else {
    icon->w = (unsigned)((float)barh * ((float)w / (float)h));
  }

  applysizehints(icon, &(icon->x), &(icon->y), &(icon->w), &(icon->h), False);

  // force icons into the systray dimensions if they don't want to
  if (icon->h > barh) {
    if (icon->w == icon->h) {
      icon->w = barh;
    } else {
      icon->w = (unsigned)((float)barh * ((float)icon->w / (float)icon->h));
    }

    icon->h = barh;
  }
}

void updatesystrayiconstate(Client* icon, XPropertyEvent* ev) {
  long flags;
  int  code = 0;

  if (!showsystray || !icon
      || ev->atom != xatom[XembedInfo]
      || !(flags = getatomprop(icon, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !icon->tags) {
    icon->tags = 1;
    code       = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, icon->win);
    setclientstate(icon, NormalState);

  } else if (!(flags & XEMBED_MAPPED) && icon->tags) {
    icon->tags = 0;
    code       = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, icon->win);
    setclientstate(icon, WithdrawnState);

  } else
    return;

  sendevent(icon->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
      systray->win, XEMBED_EMBEDDED_VERSION);
}

void updatesystray(void) {
  XSetWindowAttributes wa;
  XWindowChanges       wc;

  Client*  icon;
  Monitor* mon = systraytomon(NULL);

  int      x  = mon->mx + mon->mw;
  unsigned sw = TEXTW(stext) - lrpad + systrayspacing;
  unsigned w  = 1;

  // TODO: comment
  if (!showsystray)
    return;
  if (systrayonleft)
    x -= sw;
  if (!systray) { // init systray
    if (!(systray = (Systray*)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));

    systray->win = XCreateSimpleWindow(dpy, root, x, mon->bary, w, barh, 0, 0, scheme[SchemeSel][ColBg].pixel);

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

  for (w = 0, icon = systray->icons; icon; icon = icon->next) {
    // make sure the background color stays the same
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;

    XChangeWindowAttributes(dpy, icon->win, CWBackPixel, &wa);
    XMapRaised(dpy, icon->win);

    w += systrayspacing;
    icon->x = w;
    XMoveResizeWindow(dpy, icon->win, icon->x, 0, icon->w, icon->h);
    w += icon->w;

    if (icon->mon != mon)
      icon->mon = mon;
  }

  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, mon->bary, w, barh);

  wc.x          = x;
  wc.y          = mon->bary;
  wc.width      = w;
  wc.height     = barh;
  wc.stack_mode = Above;
  wc.sibling    = mon->barwin;

  XConfigureWindow(dpy, systray->win, CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode, &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);

  // redraw background
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, barh);
  XSync(dpy, False);
}

Client* wintosystrayicon(Window win) {
  Client* icon = NULL;

  if (!showsystray || !win)
    return icon;

  for (icon = systray->icons;
       icon && icon->win != win;
       icon = icon->next)
    ;
  return icon;
}

void removesystrayicon(Client* icon) {
  Client** icons;

  if (!showsystray || !icon)
    return;

  for (icons = &systray->icons; *icons && *icons != icon; icons = &(*icons)->next)
    ;
  if (icons)
    *icons = icon->next;

  free(icon);
}
