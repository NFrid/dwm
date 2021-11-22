#ifndef BAR_H_
#define BAR_H_

#include "nwm.h"

extern void         drawbar(Monitor* m);
extern void         drawbars(void);
extern void         drawtab(Monitor* m);
extern void         drawtabs(void);
extern void         updatebars(void);
extern void         updatebarpos(Monitor* m);
extern void         resizebarwin(Monitor* m);
extern Monitor*     systraytomon(Monitor* m);
extern unsigned int getsystraywidth();
extern int          drawstatusbar(Monitor* m, int bh, char* text, int stw);
extern void         updatestatus(void);
extern void         updatesystrayicongeom(Client* i, int w, int h);
extern void         updatesystrayiconstate(Client* i, XPropertyEvent* ev);
extern void         updatesystray(void);
extern Client*      wintosystrayicon(Window w);
extern void         removesystrayicon(Client* i);

#endif // BAR_H_
