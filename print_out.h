#ifndef __PRINT_OUT_H__
#define __PRINT_OUT_H__

#include <stdio.h>

#define DO_NOTHING do {} while(0)

#ifdef DEBUG
    #define ERR(format, args...) printf("Error: " format, ##args)
    #define OUT(format, args...) printf(format, ##args)
    #define DBG(format, args...) printf(format, ##args)
#else
    #define ERR(format, args...) printf("Error: " format, ##args)
    #define OUT(format, args...) printf(format, ##args)
    #define DBG(format, args...) DO_NOTHING
#endif

#endif // __PRINT_OUT_H__
