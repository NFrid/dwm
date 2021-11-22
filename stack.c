#include "stack.h"
#include "client.h"
#include "monitor.h"

// attach client to stack
void attach(Client* c) {
  c->next         = c->mon->clients;
  c->mon->clients = c;
}

// attach stack to stack
void attachstack(Client* c) {
  c->snext      = c->mon->stack;
  c->mon->stack = c;
}

// detach client from stack
void detach(Client* c) {
  Client** tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

// detack stack from stacks stack (idk)
void detachstack(Client* c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

// get next tiled client in stack
Client* nexttiled(Client* c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

// pop a client in the stack
void pop(Client* c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}
