/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx = 2;  /* border pixel of windows */
static const unsigned int gappx    = 5;  /* gap pixel between windows */
static const unsigned int snap     = 32; /* snap pixel */

static const unsigned int systraypinning          = 0; /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft           = 0; /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing          = 2; /* systray spacing */
static const int          systraypinningfailfirst = 1; /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/

static const int showsystray = 1; /* 0 means no systray */
static const int showbar     = 1; /* 0 means no bar */
static const int topbar      = 1; /* 0 means bottom bar */

static const char* fonts[]     = { "Iosevka nf:size=12", "Noto Color Emoji" };
static const char  col_gray1[] = "#f8f8f2";
static const char  col_gray2[] = "#282a36";
static const char  col_gray3[] = "#44475a";
static const char  col_pink[]  = "#ff79c6";
static const char* colors[][3] = {
  /*               fg         bg         border   */
  [SchemeNorm] = { col_gray1, col_gray2, col_gray2 },
  [SchemeSel]  = { col_gray1, col_gray3, col_pink },
};

/* tagging */
static const char* tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };

static const Rule rules[] = {
  /* xprop(1):
    *  WM_CLASS(STRING) = instance, class
    *  WM_NAME(STRING) = title
    */
  // clang-format off
  /* class              instance  title   tags mask   isfloating    monitor */
  { "mpv",              NULL,     NULL,   1,          0,            -1 },

  { "discord",          NULL,     NULL,   1 << 2,     0,            -1 },
  { "TelegramDesktop",  NULL,     NULL,   1 << 2,     0,            -1 },
  { "VK",               NULL,     NULL,   1 << 2,     0,            -1 },

  { "sxiv",             NULL,     NULL,   0,          0,            -1 },
  { "Zathura",          NULL,     NULL,   0,          0,            -1 },

  { "Crow Translate",   NULL,     NULL,   0,          1,            -1 },
  // clang-format on
};

/* layout(s) */
static const float mfact       = 0.55; /* factor of master area size [0.05..0.95] */
static const int   nmaster     = 1;    /* number of clients in master area */
static const int   resizehints = 1;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
  // clang-format off
  /* symbol   arrange function */
  { "[]=",    tile },
  // {"><>",     NULL},
  { "[M]",    monocle },
  { "TTT",    bstack },
  { NULL,     NULL },
  // clang-format on
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                         \
  { MODKEY, KEY, view, { .ui = 1 << TAG } },                      \
      { MODKEY | Mod1Mask, KEY, toggleview, { .ui = 1 << TAG } }, \
      { MODKEY | ShiftMask, KEY, tag, { .ui = 1 << TAG } },       \
  {                                                               \
    MODKEY | ControlMask, KEY, toggletag, { .ui = 1 << TAG }      \
  }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                      \
  {                                                     \
    .v = (const char*[]) { "/bin/sh", "-c", cmd, NULL } \
  }

#include "movestack.c"
#include "focusurgent.c"
static Key keys[] = {
  /* modifier            key         function        argument */
  { MODKEY, XK_v, togglebar, { 0 } },

  { MODKEY, XK_j, focusstack, { .i = +1 } },
  { MODKEY, XK_k, focusstack, { .i = -1 } },
  { MODKEY, XK_l, shiftviewclients, { .i = +1 } },
  { MODKEY, XK_h, shiftviewclients, { .i = -1 } },

  { MODKEY, XK_i, incnmaster, { .i = +1 } },
  { MODKEY | ShiftMask, XK_i, incnmaster, { .i = -1 } },
  { MODKEY | ShiftMask, XK_h, setmfact, { .f = -0.05 } },
  { MODKEY | ShiftMask, XK_l, setmfact, { .f = +0.05 } },
  { MODKEY | ShiftMask, XK_j, movestack, { .i = +1 } },
  { MODKEY | ShiftMask, XK_k, movestack, { .i = -1 } },

  { MODKEY, XK_x, zoom, { 0 } },
  { MODKEY, XK_o, view, { 0 } },
  { MODKEY, XK_c, killclient, { 0 } },
  { MODKEY, XK_u, focusurgent, { 0 } },

  // {MODKEY, XK_t, setlayout, {.v = &layouts[0]}},
  // {MODKEY | ShiftMask, XK_w, setlayout, {.v = &layouts[1]}},
  // {MODKEY, XK_m, setlayout, {.v = &layouts[1]}},

  { MODKEY, XK_semicolon, cyclelayout, { .i = -1 } },
  { MODKEY | ShiftMask, XK_semicolon, cyclelayout, { .i = +1 } },

  { MODKEY | ShiftMask, XK_t, setlayout, { 0 } },
  { MODKEY, XK_w, togglefloating, { 0 } },
  { MODKEY, XK_f, togglefullscr, { 0 } },

  { MODKEY, XK_minus, view, { .ui = ~0 } },
  { MODKEY | ShiftMask, XK_minus, tag, { .ui = ~0 } },

  // {MODKEY, XK_comma, focusmon, {.i = -1}},
  // {MODKEY, XK_period, focusmon, {.i = +1}},
  // {MODKEY | ShiftMask, XK_comma, tagmon, {.i = -1}},
  // {MODKEY | ShiftMask, XK_period, tagmon, {.i = +1}},

  TAGKEYS(XK_1, 0),
  TAGKEYS(XK_2, 1),
  TAGKEYS(XK_3, 2),
  TAGKEYS(XK_4, 3),
  TAGKEYS(XK_5, 4),
  TAGKEYS(XK_6, 5),
  TAGKEYS(XK_7, 6),
  TAGKEYS(XK_8, 7),
  TAGKEYS(XK_9, 8),
  TAGKEYS(XK_0, 9),

  { MODKEY | ShiftMask, XK_r, quit, { 0 } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
 * ClkClientWin, or ClkRootWin */
static Button buttons[] = {
  /* click                event mask      button          function argument */
  { ClkLtSymbol, 0, Button1, setlayout, { .v = &layouts[0] } },
  { ClkLtSymbol, 0, Button2, setlayout, { .v = &layouts[3] } },
  { ClkLtSymbol, 0, Button3, setlayout, { .v = &layouts[1] } },
  { ClkWinTitle, 0, Button2, zoom, { 0 } },
  // {ClkStatusText, 0, Button2, spawn, {.v = termcmd}},
  { ClkClientWin, MODKEY, Button1, movemouse, { 0 } },
  { ClkClientWin, MODKEY, Button2, togglefloating, { 0 } },
  { ClkClientWin, MODKEY, Button3, resizemouse, { 0 } },
  { ClkTagBar, 0, Button1, view, { 0 } },
  { ClkTagBar, 0, Button3, toggleview, { 0 } },
  // {ClkTagBar, MODKEY, Button1, tag, {0}},
  // {ClkTagBar, MODKEY, Button3, toggletag, {0}},
};
