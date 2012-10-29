#ifndef __MARSHAL_H__
#define __MARSHAL_H__

/* A simple marshal/unmarshal wrapper using standard string routines */

#include <string.h>

#define MARSHAL_DECLARE(__name, __start, __end)                 \
    struct { char *start, *ptr, *end;  } __name = { (char *)(__start), (char *)(__start), (char *)(__end) }
#define MARSHAL_SIZE(__buf) ((__buf).ptr - (__buf).start)

#define MARSHAL(__buf, __size, __ptr) do {                              \
        if ((__buf).ptr + (__size) <= (__buf).end)                      \
        { memmove((__buf).ptr, (__ptr), (__size)); (__buf).ptr += (__size); } \
    } while (0)

#define UNMARSHAL(__buf, __size, __ptr) do {                            \
        if ((__buf).ptr + (__size) <= (__buf).end)                      \
        { memmove((__ptr), (__buf).ptr, (__size)); (__buf).ptr += (__size); } \
    } while (0)

#endif

