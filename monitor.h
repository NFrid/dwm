#ifndef MONITOR_H_
#define MONITOR_H_

#include "nwm.h"

extern Monitor* createmon(void);
extern void     arrange(Monitor* m);
extern void     arrangemon(Monitor* m);
extern Monitor* dirtomon(int dir);
extern long     getstate(Window w);
extern void     manage(Window w, XWindowAttributes* wa);
extern Monitor* recttomon(int x, int y, int w, int h);
extern void     cleanupmon(Monitor* mon);
extern void     restack(Monitor* m);

#endif // MONITOR_H_
