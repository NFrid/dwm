#pragma once

#include "nwm.h"

// apply systray to selected monitor
Monitor* systraytomon(Monitor* mon);

// draw status bar (with pretty colors ^-^)
int drawstatusbar(Monitor* mon, unsigned barh, char* stext, unsigned statusw);

// draw a status bar on a monitor
extern void drawbar(Monitor* mon);

// draw all status bars on all monitors
extern void drawbars(void);

// update status in bar
extern void updatestatus(void);

// update all the bars
extern void updatebars(void);

// update position of the bar
extern void updatebarpos(Monitor* mon);

// update position of the tab bar
extern void updatetabspos(Monitor* mon);

// resize bar window
extern void resizebarwin(Monitor* mon);

// draw a tabs on a monitors
extern void drawtab(Monitor* mon);

// draw all tabs on all monitors
extern void drawtabs(void);

// get width of systray
extern unsigned getsystraywidth();

// update geometry of systray icons
extern void updatesystrayicongeom(Client* icon, unsigned w, unsigned h);

// update states of systray icons
extern void updatesystrayiconstate(Client* icon, XPropertyEvent* ev);

// update systray
extern void updatesystray(void);

// gets a systray icon client from its window
extern Client* wintosystrayicon(Window win);

// remove system tray icon
extern void removesystrayicon(Client* icon);
