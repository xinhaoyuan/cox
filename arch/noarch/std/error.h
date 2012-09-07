#ifndef __ERROR_H__
#define __ERROR_H__

#define E_UNSPECIFIED       1   // Unspecified or unknown problem
#define E_INVAL             2   // Invalid parameter
#define E_NO_MEM            3   // No memory
#define E_FAULT             4   // Memory fault
#define E_PERM              5   // Permission violated
#define E_BUSY              6   // Resources are busy

/* the maximum error number allowed */
#define MAXERROR            6

#endif
