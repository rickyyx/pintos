#ifndef __LIB_FPOINT_H
#define __LIB_FPOINT_H

#include <stdint.h>

#define F_PAD (1<<14)

#define F_TOFPOINT(x) ((x)*F_PAD)
#define F_TOINT_ZERO(x) ((x) / F_PAD)
#define F_TOINT_NEAR(x) ((x) >= 0 ? ((x)+F_PAD/2)/F_PAD : ((x)-F_PAD/2)/F_PAD)
#define F_ADD(x,y) ((x)+(y))
#define F_SUBTRACT(x,y) ((x)-(y))
#define F_ADD_INT(x,n) ((x)+(n)*F_PAD)
#define F_SUBTRACT_N(x,n) ((x)-(n)*F_PAD)
#define F_MULTIPLE(x,y) ((int64_t) (x) * (y) / F_PAD)
#define F_MULTIPLE_INT(x,n) ((x) * (n))
#define F_DIVIDE(x, y) ((int64_t) (x) * F_PAD / (y))
#define F_DIVIDE_INT(x, n) ((x) / (n))

#endif
