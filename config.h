#pragma once

#include "nwm.h"

// TODO: rethink how config should work

extern const unsigned int borderpx; // border pixel of windows
extern const unsigned int gappx;    // gap pixel between windows
extern const unsigned int snap;     // snap pixel

extern const unsigned int
    systraypinning; // 0: sloppy systray follows selected monitor, >0: pin
                    // systray to monitor X
extern const unsigned int systrayonleft;  // 0: systray in the right corner, >0:
                                          // systray on left of status text
extern const unsigned int systrayspacing; // systray spacing
extern const int
    systraypinningfailfirst; // 1: if pinning fails, display systray on the
                             // first monitor, False: display systray on the
                             // last monito

extern const int showsystray; // 0 means no systray
extern const int showbar;     // 0 means no bar
extern const int topbar;      // 0 means bottom bar

enum showtab_modes {
  showtab_auto,
  showtab_never,
  showtab_nmodes,
  showtab_always,
};

extern const int  showtab; /* Default tab bar show mode */
extern const Bool toptab;  /* False means bottom tab bar */

extern const unsigned int barmargins;      // vertical margins for bar
extern const unsigned int barspacing;      // spacing between bar elements
extern const unsigned int barspacing_font; // spacing in font widths

extern const char*  fonts[];
extern const size_t fonts_len; // length of fonts[]
extern const char   col_white[];
extern const char   col_gray1[];
extern const char   col_gray2[];
extern const char   col_black[];
extern const char   col_com[];
extern const char   col_pink[];
extern const char*  colors[][3];
extern const size_t colors_len;   // length of colors[]
extern const XPoint stickyicon[]; // represents the icon as an array of vertices
extern const size_t stickyicon_len; // length of stickyicon[]
extern const XPoint stickyiconbb;   // defines the bottom right corner of the
                                  // polygon's bounding box (speeds up scaling)

extern const char* tags[]; // tags, I suppose...

extern const PertagRule pertagrules[];   // specific defaults for specific tags
extern const size_t     pertagrules_len; // length of pertagrules[]
extern const Rule       rules[];   // specific defaults for specific clients
extern const size_t     rules_len; // length of rules[]

extern const float mfact;       // factor of master area size [0.05..0.95]
extern const int   nmaster;     // number of clients in master area
extern const int   resizehints; // 1 means respect size hints in tiled resizals

extern const Layout layouts[];   // stack layouts
extern const size_t layouts_len; // length of layouts[]

extern Key    keys[];    // keybindings
extern Button buttons[]; // mouse button keybindings

#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                         \
  { MODKEY, KEY, view, { .ui = 1 << TAG } },                      \
      { MODKEY | Mod1Mask, KEY, toggleview, { .ui = 1 << TAG } }, \
      { MODKEY | ShiftMask, KEY, tag, { .ui = 1 << TAG } }, {     \
    MODKEY | ControlMask, KEY, toggletag, { .ui = 1 << TAG }      \
  }
