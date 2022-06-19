#pragma once

#include "nwm.h"

extern void configurenotify(XEvent* e);
extern void destroynotify(XEvent* e);
extern void configurerequest(XEvent* e);
extern void clientmessage(XEvent* e);
extern void keypress(XEvent* e);
extern void buttonpress(XEvent* e);
extern void enternotify(XEvent* e);
extern void expose(XEvent* e);
extern void focusin(XEvent* e);
extern void mappingnotify(XEvent* e);
extern void maprequest(XEvent* e);
extern void motionnotify(XEvent* e);
extern void propertynotify(XEvent* e);
extern void resizerequest(XEvent* e);
extern void unmapnotify(XEvent* e);
extern int  sendevent(
     Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
