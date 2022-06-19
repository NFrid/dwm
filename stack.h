#pragma once

#include "nwm.h"

extern void    attach(Client* c);
extern void    attachstack(Client* c);
extern void    detach(Client* c);
extern void    detachstack(Client* c);
extern Client* nexttiled(Client* c);
extern void    pop(Client*);
