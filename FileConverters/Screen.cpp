#pragma once

#include "Screen.h"
#include <memory.h>

void ScrLiniarizeByColumn(ScreenType const* scr, byte* buf)
{
    byte col, part, charl, pixl;
    byte Column[8 * 8 * 3];
    byte k = 0;
    word off = 0;

    for (col = 0; col < 32; col++)
    {
        for (part = 0; part < 3; part++)
            for (charl = 0; charl < 8; charl++)
                for (pixl = 0; pixl < 8; pixl++)
                    Column[k++] = scr->Part[part].PixelLine[pixl].CharLine[charl].PixelCell[col].Byte;

        memcpy(buf + off, &Column, sizeof(Column));
        off += sizeof(Column);
        k = 0;
    }


    // Just copy attributes
    memcpy(buf + off, &scr->Attribute, sizeof(scr->Attribute));
}



void ScrDeLiniarizeByColumn(byte const* buf, ScreenType* scr)
{
    byte col, part, charl, pixl;
    byte Column[8 * 8 * 3];
    byte k = 0;
    word off = 0;

    for (col = 0; col < 32; col++)
    {
        memcpy(&Column, buf + off, sizeof(Column));

        for (part = 0; part < 3; part++)
            for (charl = 0; charl < 8; charl++)
                for (pixl = 0; pixl < 8; pixl++)
                    scr->Part[part].PixelLine[pixl].CharLine[charl].PixelCell[col].Byte =
                    Column[k++];

        k = 0;
        off += sizeof(Column);
    }

    // Just copy attributes
    memcpy(&scr->Attribute, buf + off, sizeof(scr->Attribute));
}


void ScrLiniarizeByCell(ScreenType const* scr, byte* buf)
{
    byte col, part, charl, pixl;
    byte Cell[8];
    word off = 0;

    for (part = 0; part < 3; part++)
        for (charl = 0; charl < 8; charl++)
            for (col = 0; col < 32; col++)
            {
                for (pixl = 0; pixl < 8; pixl++)
                    Cell[pixl] =
                    scr->Part[part].PixelLine[pixl].CharLine[charl].PixelCell[col].Byte;

                memcpy(buf + off, &Cell, sizeof(Cell));
                off += sizeof(Cell);
            }

    // Just copy attributes
    memcpy(buf + off, &scr->Attribute, sizeof(scr->Attribute));
}


void ScrDeLiniarizeByCell(byte const* buf, ScreenType* scr)
{
    byte col, part, charl, pixl;
    byte Cell[8];

    word off = 0;

    for (part = 0; part < 3; part++)
        for (charl = 0; charl < 8; charl++)
            for (col = 0; col < 32; col++)
            {
                //fread(&Cell, sizeof(Cell), 1, g);
                memcpy(&Cell, buf + off, sizeof(Cell));

                for (pixl = 0; pixl < 8; pixl++)
                    scr->Part[part].PixelLine[pixl].CharLine[charl].PixelCell[col].Byte =
                    Cell[pixl];

                off += sizeof(Cell);
            }

    // Just copy attributes
    memcpy(&scr->Attribute, buf + off, sizeof(scr->Attribute));
}

void GetScrCell(ScreenType const* scr, byte line, byte col, byte* buff)
{
    byte pixl = 0;
    for (; pixl < 8; pixl++)
        buff[pixl] = scr->Part[line / 8].PixelLine[pixl].CharLine[line % 8].PixelCell[col].Byte;
}

void SetScrCell(ScreenType* scr, byte line, byte col, byte* buff)
{
    byte pixl = 0;
    for (; pixl < 8; pixl++)
        scr->Part[line / 8].PixelLine[pixl].CharLine[line % 8].PixelCell[col].Byte = buff[pixl];
}

void ScrBlankRectangle(ScreenType* scr, byte line1, byte col1, byte line2, byte col2)
{
    byte cell[8] = { 0, 0, 0, 0, 0, 0, 0, 0};

    for (byte line = line1; line <= line2; line++)
        for (byte col = col1; col < col2; col++)
        {
            SetScrCell(scr, line, col, cell);
            scr->Attribute[line][col].Byte = 0;
        }
}

/*Formula from Z80 emulator reference, for pixels:
16384+INT (x/8)+1792*INT (y/64)-2016*INT (y/8)+256*y */
bool GetScrBit(word x, word y, byte* scr)
{
    byte scrPixelByte = scr[(x / 8) + 1792 * (int)(y / 64) - 2016 * (int)(y / 8) + 256 * y];

    return (scrPixelByte & (0x80 >> (x % 8))) > 0;
}

byte GetScrByte(word x, word y, byte* scr)
{
    return scr[x + 1792 * (int)(y / 64) - 2016 * (int)(y / 8) + 256 * y];
}

void SetScrBit(word x, word y, byte* scr, bool bitState)
{
    byte* scrPixelByte = &scr[(x / 8) + 1792 * (int)(y / 64) - 2016 * (int)(y / 8) + 256 * y];

    if (bitState == true)
        *scrPixelByte |= (0x80 >> (x % 8));
    else
        *scrPixelByte &= !(0x80 >> (x % 8));
}
