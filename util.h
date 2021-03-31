/* See LICENSE file for copyright and license details. */

#define MAX(A, B)        ((A) > (B) ? (A) : (B))
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))

// throw an error and fucking die
void die(const char* fmt, ...);
// like calloc, but with error handling
void* ecalloc(size_t nmemb, size_t size);
// count number of ones in binary representation
unsigned int n_ones(unsigned int n);