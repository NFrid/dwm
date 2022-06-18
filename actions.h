#ifndef ACTIONS_H_
#define ACTIONS_H_

#include "nwm.h"

extern void quit(const Arg* arg);
extern void view(const Arg* arg);
extern void toggleall(const Arg* arg);
extern void toggleview(const Arg* arg);
extern void tag(const Arg* arg);
extern void toggletag(const Arg* arg);
extern void tabmode(const Arg* arg);
extern void tagmon(const Arg* arg);
extern void togglefloating(const Arg* arg);
extern void togglesticky(const Arg* arg);
extern void togglefullscr(const Arg* arg);
extern void cyclelayout(const Arg* arg);
extern void focusurgent(const Arg* arg);
extern void takeurgent(const Arg* arg);
extern void movestack(const Arg* arg);
extern void resizemouse(const Arg* arg);
extern void movemouse(const Arg* arg);
extern void zoom(const Arg* arg);
extern void togglebar(const Arg* arg);
extern void setlayout(const Arg* arg);
extern void setmfact(const Arg* arg);
extern void shiftviewclients(const Arg* arg);
extern void incnmaster(const Arg* arg);
extern void killclient(const Arg* arg);
extern void focuswin(const Arg* arg);
extern void focusmon(const Arg* arg);
extern void focusstack(const Arg* arg);

#endif // ACTIONS_H_
