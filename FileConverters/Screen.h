//Order SCREEN$ memory by column or by cell, needed to improve compression by having input data more homogenous.

#pragma once

#include "..\types.h"

#define MAX_LINE 24
#define MAX_COL  32

typedef enum
{
	ORD_NONE,	
	ORD_COLUMN,
	ORD_CELL
} ScrOrderType;

static const char* ScrOrdNames[] =
{
	"none",
	"column",
	"cell"	
};

typedef struct
{
    struct _Part
    {
        struct _PixelLine
        {
            struct _CharLine
            {
                union _PixelCell
                {
                    struct _Pixels
                    {
                        byte Pixel0 : 1;
                        byte Pixel1 : 1;
                        byte Pixel2 : 1;
                        byte Pixel3 : 1;
                        byte Pixel4 : 1;
                        byte Pixel5 : 1;
                        byte Pixel6 : 1;
                        byte Pixel7 : 1;
                    } Pixels; // Pixels
                    byte Byte;
                } PixelCell[32];
            } CharLine[8];
        } PixelLine[8];
    } Part[3];

    union _Attribute
    {
        struct _Bit
        {
            byte Ink : 3;
            byte Paper : 3;
            byte Bright : 1;
            byte Flash : 1;
        } Bit;
        byte Byte;
    } Attribute[24][32];

} ScreenType;

void ScrLiniarizeByColumn(ScreenType const* scr, byte* buf);
void ScrLiniarizeByCell(ScreenType const* scr, byte* buf);
//Usefull for further improving the compression for screeens, when the game only uses the border of a SCREEN$.
void ScrBlankRectangle(ScreenType* scr, byte line1, byte col1, byte line2, byte col2);
