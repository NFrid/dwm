#include "layouts.h"
#include "client.h"
#include "config.h"
#include "stack.h"
#include "util.h"
#include "variables.h"

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
  for (i = mx = 0, tx = m->wx, c = nexttiled(m->clients); c; c =
nexttiled(c->next), i++) { if (i < m->nmaster) { w = (m->ww - mx) / (MIN(n,
m->nmaster) - i); resize(c, m->wx + mx, m->wy, w - (2 * c->bw), mh - (2 *
c->bw), 0); mx += WIDTH(c); } else { h = m->wh - mh; resize(c, tx, ty, tw - (2 *
c->bw), h - (2 * c->bw), 0); if (tw != m->ww) tx += WIDTH(c);
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
  for (i = my = ty = 0, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw),
          h - (2 * c->bw), 0);
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
