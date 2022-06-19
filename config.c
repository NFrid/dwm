// See LICENSE file for copyright and license details.

#include "config.h"
#include "actions.h"
#include "layouts.h"
#include "nwm.h"
#include <X11/keysym.h>

const unsigned int borderpx = 2;
const unsigned int gappx    = 5;
const unsigned int snap     = 32;

const unsigned int systraypinning          = 0;
const unsigned int systrayonleft           = 0;
const unsigned int systrayspacing          = 2;
const int          systraypinningfailfirst = 1;

const int showsystray = 1;
const int showbar     = 1;
const int topbar      = 1;

const int  showtab = showtab_auto;
const Bool toptab  = False;

const unsigned int barmargins      = 1;
const unsigned int barspacing      = 0;
const unsigned int barspacing_font = 1;

const char* fonts[]
    = { "Iosevka nf:size=12", "Noto Color Emoji", "Source Han Sans JP" };
const char  col_white[] = "#f8f8f2";
const char  col_gray1[] = "#282a36";
const char  col_gray2[] = "#44475a";
const char  col_black[] = "#21222C";
const char  col_com[]   = "#6272A4";
const char  col_pink[]  = "#ff79c6";
const char* colors[][3] = {
  //               fg         bg         border
  [SchemeNorm] = { col_white, col_gray1, col_gray1 }, // client's default
  [SchemeDark] = { col_white, col_black, col_gray1 }, // for unselected mon
  [SchemeInv]  = { col_com, col_black, col_gray1 },   // for unselected mon
  [SchemeSel]  = { col_white, col_gray2, col_pink },  // for selected client
};
const size_t fonts_len  = LENGTH(fonts);
const size_t colors_len = LENGTH(colors);

const XPoint stickyicon[]
    = { { 0, 0 }, { 4, 0 }, { 4, 8 }, { 2, 6 }, { 0, 8 }, { 0, 0 } };
const size_t stickyicon_len = LENGTH(stickyicon);
const XPoint stickyiconbb   = { 4, 8 };

const char* tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };

const PertagRule pertagrules[] = {
  // clang-format off
//* tag, layout, mfact
  { 2,   1,      -1 },
  { 4,   0,      .5 },
  { 6,   1,      -1 },
  // clang-format on
};
const size_t pertagrules_len = LENGTH(pertagrules);

const Rule rules[] = {
  // clang-format off
//* class                 instance  title   tags mask   isfloating  monitor
  { "Alacritty",          NULL,     "sys:", 1,          0,          1 },

  { "discord",            NULL,     NULL,   1 << 2,     0,          1 },
  { "TelegramDesktop",    NULL,     NULL,   1 << 2,     0,          1 },
  { "VK",                 NULL,     NULL,   1 << 2,     0,          1 },
  { "Element",            NULL,     NULL,   1 << 2,     0,          1 },
  { "Slack",              NULL,     NULL,   1 << 2,     0,          1 },

  { "zoom",               NULL,     NULL,   1 << 4,     0,          1 },

  { "code-oss",           NULL,     NULL,   1 << 6,     0,          0 },
  { "jetbrains-",         NULL,     NULL,   1 << 6,     0,          0 },

  { "sxiv",               NULL,     NULL,   0,          0,         -1 },
  { "Zathura",            NULL,     NULL,   0,          0,         -1 },

  { "Crow Translate",     NULL,     NULL,   0,          1,         -1 },
  { "copyq",              NULL,     NULL,   0,          1,         -1 },

  { "obs",                NULL,     NULL,   1 << 8,     0,          1 },
  // clang-format on
};
const size_t rules_len = LENGTH(rules);

const float mfact       = 0.55;
const int   nmaster     = 1;
const int   resizehints = 1;

const Layout layouts[] = {
  // clang-format off
//* symbol  arrange function */
  { "[]=",  tile },
  // { "><>",  NULL },
  { "[M]",  monocle },
  /* { "TTT",  bstack }, */
  { NULL,   NULL },
  // clang-format on
};
const size_t layouts_len = LENGTH(layouts);

Key keys[] = {
  // clang-format off
//* modifier, key                       function, argument
  { MODKEY, XK_backslash,               togglebar, { 0 } },
  { MODKEY | ShiftMask, XK_w,           tabmode,   { -1 } },

  { MODKEY, XK_j,                       focusstack, { .i = +1 } },
  { MODKEY, XK_k,                       focusstack, { .i = -1 } },
  { MODKEY, XK_l,                       shiftviewclients, { .i = +1 } },
  { MODKEY, XK_h,                       shiftviewclients, { .i = -1 } },

  { MODKEY, XK_i,                       incnmaster, { .i = +1 } },
  { MODKEY | ShiftMask, XK_i,           incnmaster, { .i = -1 } },
  { MODKEY | ShiftMask, XK_h,           setmfact, { .f = -0.05 } },
  { MODKEY | ShiftMask, XK_l,           setmfact, { .f = +0.05 } },
  { MODKEY | ShiftMask, XK_j,           movestack, { .i = +1 } },
  { MODKEY | ShiftMask, XK_k,           movestack, { .i = -1 } },

  { MODKEY, XK_x,                       zoom, { 0 } },
  { MODKEY, XK_o,                       view, { 0 } },
  { MODKEY, XK_c,                       killclient, { 0 } },
  { MODKEY, XK_u,                       focusurgent, { 0 } },
  { MODKEY | ShiftMask, XK_u,           takeurgent, { 0 } },

  // {MODKEY, XK_t,                        setlayout, {.v = &layouts[0]}},
  // {MODKEY | ShiftMask, XK_w,            setlayout, {.v = &layouts[1]}},
  // {MODKEY, XK_m,                        setlayout, {.v = &layouts[1]}},

  { MODKEY, XK_v,                       cyclelayout, { .i = +1 } },
  /* { MODKEY | ShiftMask, XK_backslash,   cyclelayout, { .i = -1 } }, */

  { MODKEY | ShiftMask, XK_t,           setlayout, { 0 } },
  { MODKEY, XK_w,                       togglefloating, { 0 } },
  { MODKEY, XK_f,                       togglefullscr, { 0 } },

  { MODKEY, XK_comma,                   focusmon, {.i = -1}},
  { MODKEY, XK_period,                  focusmon, {.i = +1}},
  { MODKEY | ShiftMask, XK_comma,       tagmon, {.i = -1}},
  { MODKEY | ShiftMask, XK_period,      tagmon, {.i = +1}},

  /* { MODKEY, XK_minus,                   view, { .ui = ~0 } }, */
  { MODKEY, XK_minus,                   toggleall, { 0 } },
  { MODKEY | ShiftMask, XK_minus,       togglesticky, { 0 } },

  { MODKEY | ShiftMask, XK_r,           quit, { 0 } },

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
  // clang-format on
  { 0, 0, NULL, { 0 } }
};

Button buttons[] = {
  // click can be:
  // ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or
  // ClkRootWin

  // clang-format off
//* click event     mask, button      function, argument
  { ClkLtSymbol,    0, Button1,       cyclelayout, { .v = &layouts[0] } },
  { ClkWinTitle,    0, Button2,       zoom, { 0 } },
  { ClkClientWin,   MODKEY, Button1,  movemouse, { 0 } },
  { ClkClientWin,   MODKEY, Button2,  togglefloating, { 0 } },
  { ClkClientWin,   MODKEY, Button3,  resizemouse, { 0 } },
  { ClkTagBar,      0, Button1,       view, { 0 } },
  { ClkTagBar,      0, Button2,       toggletag, { 0 } },
  { ClkTagBar,      0, Button3,       toggleview, { 0 } },
  { ClkTagBar,      MODKEY, Button1,  tag, { 0 } },
  { ClkTagBar,      MODKEY, Button3,  toggletag, { 0 } },
  { ClkTabBar,      0, Button1,       focuswin, { 0 } },
  // clang-format on
  { 0, 0, 0, NULL, { 0 } }
};
