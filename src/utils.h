#ifndef UTILS_H_
#define UTILS_H_

#include <math.h>
#include <stdint.h>
#include <stddef.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define NUMOF(l) (sizeof(l) / sizeof(*l))

#define linear_to_dB(x) (log10(x) * 20)

/*! Calculate the little string containing a bargraph
 * 'VU-meter' from the peak value measured
 */
const char* level(int channel, int peak);

size_t strlen_utf8(const char *s);

#endif

