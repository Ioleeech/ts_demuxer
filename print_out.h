#ifndef __PRINT_OUT_H__
#define __PRINT_OUT_H__

#include <stdio.h>

#define ERR(format, args...) printf("Error: " format, ##args)
#define OUT(format, args...) printf(format, ##args)

#endif // __PRINT_OUT_H__
