#ifndef CLIENT_H_
#define CLIENT_H_

#include "nwm.h"

extern void configure(Client* c);
extern void applyrules(Client* c);

extern Bool applysizehints(
    Client* c, int* x, int* y, unsigned* w, unsigned* h, Bool interact);
extern void resize(
    Client* c, int x, int y, unsigned w, unsigned h, Bool interact);
extern void resizeclient(Client* c, int x, int y, unsigned w, unsigned h);

extern void updatesizehints(Client* c);
extern void updatetitle(Client* c);
extern void updatewindowtype(Client* c);
extern void updatewmhints(Client* c);

extern void sendmon(Client* c, Monitor* m);
extern void setclientstate(Client* c, long state);
extern void focus(Client* c);
extern void setfocus(Client* c);
extern void setfullscreen(Client* c, int fullscreen);
extern void seturgent(Client* c, int urg);
extern void showhide(Client* c);

extern void unfocus(Client* c, int setfocus);
extern void unmanage(Client* c, int destroyed);

#endif // CLIENT_H_
