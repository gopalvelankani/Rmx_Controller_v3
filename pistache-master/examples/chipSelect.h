#ifndef __CHIP_SELECT_H__
#define __CHIP_SELECT_H__

/*****************************************************************************************
    Include Header Files
    (No absolute paths - paths handled by make file)
*****************************************************************************************/
#ifndef _MSC_VER 
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#endif


typedef enum 
{
  OUTPUT_1 = 11,
  OUTPUT_2 = 12,
  OUTPUT_3 = 13,
  OUTPUT_4 = 14,
  OUTPUT_5 = 15,
  OUTPUT_6 = 16,
  OUTPUT_7 = 17,
  OUTPUT_8 = 18
} QAM_CS;