#ifndef CORE_H_
#define CORE_H_

#include "nwm.h"

extern int xerror(Display* dpy, XErrorEvent* ee);
extern int xerrordummy(Display* dpy, XErrorEvent* ee);
extern int xerrorstart(Display* dpy, XErrorEvent* ee);

extern void         grabbuttons(Client* c, int focused);
extern void         grabkeys(void);
extern void         updatenumlockmask(void);
extern unsigned int cleanmask(unsigned int mask);

extern Atom getatomprop(Client* c, Atom prop);
extern int  getrootptr(int* x, int* y);
extern int  gettextprop(Window w, Atom atom, char* text, unsigned int size);

extern void sigchld(int unused);

extern void updateclientlist(void);
extern int  updategeom(void);

extern Client*  wintoclient(Window w);
extern Monitor* wintomon(Window w);

extern void checkotherwm(void);
extern void setup(void);
extern void run(void);
extern void scan(void);
extern void cleanup(void);

extern int isuniquegeom(
    XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info);

#endif // CORE_H_
