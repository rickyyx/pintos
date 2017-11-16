#ifndef __LIB_MATH_H
#define __LIB_MATH_H



#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b);   \
     _a < _b ? _a : _b; })


#endif /* lib/math.h */

