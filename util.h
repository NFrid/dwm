/* See LICENSE file for copyright and license details. */

#define MAX(A, B)        ((A) > (B) ? (A) : (B))
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))

// like calloc, but with error handling
void die(const char* fmt, ...);
// throw an error and fucking die
void* ecalloc(size_t nmemb, size_t size);
