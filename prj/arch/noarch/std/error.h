#ifndef __ERROR_H__
#define __ERROR_H__

#define E_UNSPECIFIED       1   // Unspecified or unknown problem
#define E_BAD_PROC          2   // Process doesn't exist or otherwise
#define E_INVAL             3   // Invalid parameter
#define E_NO_MEM            4   // Request failed due to memory shortage
#define E_NO_FREE_PROC      5   // Attempt to create a new process beyond
#define E_FAULT             6   // Memory fault
#define E_PERM              7
#define E_BUSY              8

/* the maximum allowed */
#define MAXERROR            8

#endif /* !__ERROR_H__ */

