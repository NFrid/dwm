#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(A, B)        ((A) > (B) ? (A) : (B))
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))

void die(const char* fmt, ...);
void die_happy(const char* fmt, ...);

void* ecalloc(size_t nmemb, size_t size);

unsigned int n_ones(unsigned int n);
int          cmpint(const void* p1, const void* p2); // compare pointers as int

unsigned int intersect(int x1, int y1, unsigned w1, unsigned h1, int x2, int y2,
    unsigned w2, unsigned h2);

extern char debuginfo[36];
