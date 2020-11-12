/**********************************************************************************************************************************/
/* Module       : SCR2GIF.C                                                                                                       */
/* Executable   : SCR2GIF.EXE                                                                                                     */
/* Version type : Standalone program                                                                                              */
/* Last changed : 25-05-1998  20:30                                                                                               */
/* Update count : 2                                                                                                               */
/* OS type      : Generic                                                                                                         */
/* Description  : Spectrum .SCR to compressed GIF87a convertor.                                                                   */
/* Copyrights   : GIF87a functions based on a source from Sverre H. Huseby, Bjoelsengt. 17, N-0468 Oslo, Norway 26-09-1992        */
/*                Implementation taken from Workbench! v2.71.3, copyright (C) 1995-1998 ThunderWare Research Center               */
/*                GIF89a extensions added 25-05-1998, "NETSCAPE v2.0" compliant (web-based default) LOOPing                       */
/*                                                                                                                                */
/*                The Graphics Interchange Format(c) is the Copyright property of CompuServe Incorporated.                        */
/*                GIF(sm) is a Service Mark property of CompuServe Incorporated.                                                  */
/*                                                                                                                                */
/*                Copyright (C) 1998 by M. van der Heide of ThunderWare Research Center                                           */
/**********************************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
//#include <errno.h>
//#include <unistd.h>
#include "..\types.h"

#include <memory.h>
/**********************************************************************************************************************************/
/* Add some variables types                                                                                                       */
/**********************************************************************************************************************************/

//typedef unsigned char    byte;
//typedef unsigned short   word;                                                                                /* (Must be 16-bit) */
//typedef unsigned long    dword;                                                                               /* (Must be 32-bit) */


#ifndef TRUE
#define TRUE             (byte)1
#define FALSE            (byte)0
#endif

/**********************************************************************************************************************************/
/* Define the GIF return codes                                                                                                    */
/**********************************************************************************************************************************/

#define GIF_OK           0x00                                                                                        /* No errors */
#define GIF_ERRCREATE    0x01                                                                          /* Error creating GIF file */
#define GIF_ERRWRITE     0x02                                                                        /* Error writing to GIF file */
#define GIF_OUTMEM       0x03                                                                        /* Cannot allocate resources */
#define GIF_OUTMEM2      0x04                                                                        /* Cannot allocate resources */

/**********************************************************************************************************************************/
/* Define the static variables                                                                                                    */
/**********************************************************************************************************************************/

static FILE  *_OutFile;                                                                                       /* File to write to */
static char   _GIFErrorMessage[50];

/**********************************************************************************************************************************/
/* Define the information to write a bit-file                                                                                     */
/**********************************************************************************************************************************/

static byte   _Buffer[256];
static int    _Index;                                                                                   /* Current byte in buffer */
static int    _BitsLeft;                                          /* Bits left to fill in current byte. These are right-justified */

/**********************************************************************************************************************************/
/* Define the information to maintain an LZW-string table                                                                         */
/**********************************************************************************************************************************/

#define RES_CODES        2

#define HASH_FREE        0xFFFF
#define NEXT_FIRST       0xFFFF

#define MAXBITS          12
#define MAXSTR           (1 << MAXBITS)

#define HASHSIZE         9973
#define HASHSTEP         2039

#define HASH(Index, Lastbyte) (((Lastbyte << 8) ^ Index) % HASHSIZE)

static byte  *StrChr = NULL;
static word  *StrNxt = NULL;
static word  *StrHsh = NULL;
static word   NumStrings;

/**********************************************************************************************************************************/
/* Define the information on GIF images                                                                                           */
/**********************************************************************************************************************************/

typedef struct
{
  word        LocalScreenWidth;
  word        LocalScreenHeight;
  byte        GlobalColourTableSize : 3;
  byte        SortFlag              : 1;
  byte        ColourResolution      : 3;
  byte        GlobalColourTableFlag : 1;
  byte        BackgroundColourIndex;
  byte        PixelAspectRatio;
} _ScreenDescriptor;

typedef struct
{
  byte        Separator;
  word        LeftPosition;
  word        TopPosition;
  word        Width;
  word        Height;
  byte        LocalColourTableSize : 3;
  byte        Reserved             : 2;
  byte        SortFlag             : 1;
  byte        InterlaceFlag        : 1;
  byte        LocalColourTableFlag : 1;
} _ImageDescriptor;

typedef struct
{
  byte        ExtensionIntroducer;
  byte        GraphicControlLabel;
  byte        BlockSize;
  byte        TransparantColorFlag : 1;
  byte        UserInputFlag        : 1;
  byte        DisposalMethod       : 3;
  byte        Reserved             : 3;
  word        DelayTime;                                                                                           /* BIG endian! */
  byte        TransparantColorIndex;
  byte        BlockTerminator;
} _GraphicControlExtension;

static byte   _ApplicationExtensionHeader[19] =
{
  0x21,                                     /* GIF Extension Code */
  0xFF,                                     /* Application Extension Label */
  0x0B,                                     /* Length of Application Block */
  'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',   /* Application Name */
  '2', '.', '0',                            /* Application ID */
  0x03,                                     /* Length of Data Sub-Block */
  0x01,                                     /* Sub-Block ID */
  0x00,                                     /* Loop Count higher value (big endian) */
  0x00,                                     /* Loop Count lower value (0 = infinite) */
  0x00                                      /* Data Sub-Block Terminator */
};

static short  _BitsPrPrimColour;                                                                       /* Bits per primary colour */
static short  _NumColours;                                                                   /* Number of colours in colour table */
static byte  *_ColourTable = NULL;
static word   _ScreenHeight;
static word   _ScreenWidth;
static word   _ImageHeight;
static word   _ImageWidth;
static word   _ImageLeft;
static word   _ImageTop;
static word   _RelPixX;
static word   _RelPixY;

/**********************************************************************************************************************************/
/* Define the static functions                                                                                                    */
/**********************************************************************************************************************************/

static byte   _Create                       (char *FileName);
static byte   _Write                        (void *Buf, word Length);
static byte   _WriteByte                    (byte B);
static byte   _WriteWord                    (word W);
static void   _Close                        (void);

static void   _InitBitFile                  (void);
static int    _ResetOutBitFile              (void);
static int    _WriteBits                    (short Bits, short NumBits);

static void   _FreeStrtab                   (void);
static byte   _AllocStrtab                  (void);
static word   _AddCharString                (word Index, byte B);
static word   _FindCharString               (word Index, byte B);
static void   _ClearStrtab                  (short CodeSize);

static byte   _LZW_Compress                 (short CodeSize, short (*GetPixelFunction)(short PixX, short PixY));

static short  _BitsNeeded                   (word N);
static short (*_GetPixel)                   (short PixX, short PixY);
static short  _InputByte                    (void);
static byte   _WriteScreenDescriptor        (_ScreenDescriptor *Sd);
static byte   _WriteImageDescriptor         (_ImageDescriptor *Id);
static byte   _WriteGraphicControlExtension (_GraphicControlExtension *Ext);

static byte   _GIFCreate                    (char *FileName, short Width, short Height, short NumColours, short ColourRes, byte GIF89a);
static void   _GIFSetColour                 (byte ColourNum, byte Red, byte Green, byte Blue);
static byte   _GIFWriteGlobalColorTable     (byte GIF89a);
static byte   _GIFCompressImage             (short StartX, short StartY, int Width, int Height,
                                             short (*GetPixelFunction)(short PixX, short PixY), byte GIF89a, word PictureDelayTime);
static byte   _GIFClose                     (void);
static char  *_GIFstrerror                  (byte ErrorCode);

/**********************************************************************************************************************************/
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> START OF GENERIC LIBRARY FUNCTIONS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
/**********************************************************************************************************************************/

static byte _Create (char *FileName)

/**********************************************************************************************************************************/
/* Pre   : `FileName' points to the name of the file to be created.                                                               */
/* Post  : Creates a new file and enables referencing using the global variable _OutFile.                                         */
/*         The return value is GIF_ERRCREATE if the file could not be created, GIF_OK otherwise.                                  */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  return (((_OutFile = fopen (FileName, "wb")) == NULL) ? GIF_ERRCREATE : GIF_OK);
}

static byte _Write (void *Buf, word Length)

/**********************************************************************************************************************************/
/* Pre   : `Buf' points to the buffer to be written, `Length' holds the length.                                                   */
/* Post  : The buffer has been written to the _OutFile.                                                                           */
/*         The return value is GIF_ERRWRITE if a write error occured, GIF_OK otherwise.                                           */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  return ((fwrite (Buf, 1, Length, _OutFile) < Length) ? GIF_ERRWRITE : GIF_OK);
}

static byte _WriteByte (byte B)

/**********************************************************************************************************************************/
/* Pre   : `B' holds the byte to be written.                                                                                      */
/* Post  : The byte has been written. The return value is GIF_ERRWRITE if an error occured, GIF_OK otherwise.                     */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  return ((putc (B, _OutFile) == EOF) ? GIF_ERRWRITE : GIF_OK);
}

static byte _WriteWord (word W)

/**********************************************************************************************************************************/
/* Pre   : `W' holds the word to be written.                                                                                      */
/* Post  : The word has been written BIG-endian. The return value is GIF_ERRWRITE if an error occured, GIF_OK otherwise.          */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  if (putc (W & 0xFF, _OutFile) == EOF)
    return (GIF_ERRWRITE);

  return ((putc ((W >> 8), _OutFile) == EOF) ? GIF_ERRWRITE : GIF_OK);
}

static void _Close (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Closes the _OutFile.                                                                                                   */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  fclose (_OutFile);
}

static void _InitBitFile (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Initiate when using a bitfile.                                                                                         */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  _Buffer[_Index = 0] = 0;
  _BitsLeft = 8;
}

static int _ResetOutBitFile (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Tidy up after using a bitfile. Returns 0 if no errors occured, -1 otherwise.                                           */
/* Import: _WriteByte, _Write.                                                                                                    */
/**********************************************************************************************************************************/

{
  byte NumBytes;

  NumBytes = _Index + (_BitsLeft == 8 ? 0 : 1);                                             /* Find out how much is in the buffer */
  if (NumBytes)                                                                    /* Write whatever is in the buffer to the file */
  {
    if (_WriteByte (NumBytes) != GIF_OK)
      return (-1);
    if (_Write (_Buffer, NumBytes) != GIF_OK)
      return (-1);
    _Buffer[_Index = 0] = 0;
    _BitsLeft = 8;
  }
  return (0);
}

static int _WriteBits (short Bits, short NumBits)

/**********************************************************************************************************************************/
/* Pre   : 'Bits' holds the (right justified) bits to be written, `NumBits' holds the number of bits to be written.               */
/* Post  : The given number of bits are written to the _OutFile. If an error occured, -1 is returned.                             */
/* Import: _WriteByte, _Write.                                                                                                    */
/**********************************************************************************************************************************/

{
  int  register BitsWritten = 0;
  byte register NumBytes    = 255;

  do                                                                                           /* If the buffer is full, write it */
  {
    if ((_Index == 254 && !_BitsLeft) || _Index > 254)
    {
      if (_WriteByte (NumBytes) != GIF_OK)
        return (-1);
      if (_Write (_Buffer, NumBytes) != GIF_OK)
        return (-1);
      _Buffer[_Index = 0] = 0;
      _BitsLeft = 8;
    }

    if (NumBits <= _BitsLeft)                                                           /* Now take care of the two special cases */
    {
      _Buffer[_Index] |= (Bits & ((1 << NumBits) - 1)) << (8 - _BitsLeft);
      BitsWritten += NumBits;
      _BitsLeft -= NumBits;
      NumBits = 0;
    }
    else
    {
      _Buffer[_Index] |= (Bits & ((1 << _BitsLeft) - 1)) << (8 - _BitsLeft);
      BitsWritten += _BitsLeft;
      Bits >>= _BitsLeft;
      NumBits -= _BitsLeft;
      _Buffer[++ _Index] = 0;
      _BitsLeft = 8;
    }
  } while (NumBits);
  return (BitsWritten);
}

static void _FreeStrtab (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Free arrays used in string table routines.                                                                             */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  if (StrHsh)
  {
    free (StrHsh);
    StrHsh = NULL;
  }
  if (StrNxt)
  {
    free (StrNxt);
    StrNxt = NULL;
  }
  if (StrChr)
  {
    free (StrChr);
    StrChr = NULL;
  }
}

static byte _AllocStrtab (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Allocate arrays used in string table routines. Returns GIF_OUTMEM or GIF_OK.                                           */
/* Import: _FreeStrtab.                                                                                                           */
/**********************************************************************************************************************************/

{
  _FreeStrtab ();                                                                                             /* Just in case ... */

  if ((StrNxt = (word *)malloc (MAXSTR * 2)) == NULL)
  {
    _FreeStrtab ();
    return (GIF_OUTMEM2);
  }
  if ((StrChr = (byte *)malloc (MAXSTR)) == NULL)
  {
    _FreeStrtab ();
    return (GIF_OUTMEM2);
  }
  if ((StrHsh = (word *)malloc (HASHSIZE * 2)) == NULL)
  {
    _FreeStrtab ();
    return (GIF_OUTMEM2);
  }
  return (GIF_OK);
}

static word _AddCharString (word Index, byte B)

/**********************************************************************************************************************************/
/* Pre   : `Index' holds the index to the first part of the string, or 0xFFFF is only 1 byte is wanted, `B' holds the last byte   */
/*         in the new string.                                                                                                     */
/* Post  : Add a string consisting of the string of Index plus the byte B. The return value is 0xFFFF if room is not available.   */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  word register HshIdx;

  if (NumStrings >= MAXSTR)                                                                       /* Check if there is more room */
    return 0xFFFF;
  HshIdx = HASH (Index, B);                                             /* Search the string table until a free position is found */
  while (StrHsh[HshIdx] != 0xFFFF)
    HshIdx = (HshIdx + HASHSTEP) % HASHSIZE;
  StrHsh[HshIdx] = NumStrings;                                                                               /* Insert new string */
  StrChr[NumStrings] = B;
  StrNxt[NumStrings] = (Index != 0xFFFF ? Index : NEXT_FIRST);
  return (NumStrings ++);
}

static word _FindCharString  (word Index, byte B)

/**********************************************************************************************************************************/
/* Pre   : `Index' holds the index to the first part of the string, or 0xFFFF is only 1 byte is wanted, `B' holds the last byte   */
/*         in the new string.                                                                                                     */
/* Post  : Find index of string consisting of the string of index plus the byte B. The return value is 0xFFFF if not found.       */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  word register HshIdx;
  word register NxtIdx;

  if (Index == 0xFFFF)                                           /* Check if Index is 0xFFFF. In that case we need only return B, */
    return (B);                                             /* since all one-character strings has their bytevalue as their index */
  HshIdx = HASH (Index, B);                                              /* Search the string table until the string is found, or */
  while ((NxtIdx = StrHsh[HshIdx]) != 0xFFFF)                       /* we find HASH_FREE. In that case the string does not exist. */
  {
    if (StrNxt[NxtIdx] == Index && StrChr[NxtIdx] == B)
      return (NxtIdx);
    HshIdx = (HshIdx + HASHSTEP) % HASHSIZE;
  }
  return (0xFFFF);                                                                                           /* No match is found */
}

static void _ClearStrtab (short CodeSize)

/**********************************************************************************************************************************/
/* Pre   : `CodeSize' holds the number of bits to encode one pixel.                                                               */
/* Post  : Mark the entire table as free, enter the 2**CodeSize one-byte strings and reserve the RES_CODES reserved codes.        */
/* Import: _AddCharString.                                                                                                        */
/**********************************************************************************************************************************/

{
  int  register  Q;
  int  register  W;
  word register *Wp;

  NumStrings = 0;                                                                            /* No strings currently in the table */
  Wp = StrHsh;                                                                                   /* Mark entire hashtable as free */
  for (Q = 0 ; Q < HASHSIZE ; Q ++)
    *Wp ++ = HASH_FREE;
  W = (1 << CodeSize) + RES_CODES;                                /* Insert 2**CodeSize one-character strings, and reserved codes */
  for (Q = 0 ; Q < W ; Q ++)
    _AddCharString (0xFFFF, (byte)Q);
}

static byte _LZW_Compress (short CodeSize, short (*GetPixelFunction)(short PixX, short PixY))

/**********************************************************************************************************************************/
/* Pre   : `CodeSize' holds the number of bits needed to represent one pixelvalue.                                                */
/*         `GetPixelFunction' points to a callback function used to fetch the color value of each pixel of the image.             */
/* Post  : Perform LZW compression as specified in the GIF87a standard. The return value is one of GIF_OK or GIF_OUTMEM.          */
/* Import: _InitBitFile, _ClearStrtab, _WriteBits, _FindCharString, _AddCharString, _ResetOutBitFile, _FreeStrtab.                */
/**********************************************************************************************************************************/

{
  word  register Index;
  short register C;
  int            ClearCode;
  int            EndOfInfo;
  int            NumBits;
  int            Limit;
  int            ErrCode;
  word           Prefix = 0xFFFF;

  _GetPixel = GetPixelFunction;
  _InitBitFile ();                                                                                   /* Set up the given _OutFile */
  ClearCode = 1 << CodeSize;                                                                       /* Set up variables and tables */
  EndOfInfo = ClearCode + 1;
  NumBits = CodeSize + 1;
  Limit = (1 << NumBits) - 1;
  if ((ErrCode = _AllocStrtab ()) != GIF_OK)
    return (ErrCode);
  _ClearStrtab (CodeSize);
  _WriteBits (ClearCode, NumBits);                             /* First send a code telling the unpacker to clear the stringtable */
  while ((C = _InputByte ()) != -1)
  {                                                                                                    /* Now perform the packing */
    if ((Index = _FindCharString (Prefix, (byte)C)) != 0xFFFF)        /* Check if the prefix + the new character is a string that */
    {                                                                                                      /* exists in the table */
      Prefix = Index;                                          /* The string exists in the table. Make this string the new prefix */
    }
    else
    {                                                                                   /* The string does not exist in the table */
      _WriteBits (Prefix, NumBits);                                             /* First write code of the old prefix to the file */
      if (_AddCharString (Prefix, (byte)C) > Limit)     /* Add the new string (the prefix + the new character) to the stringtable */
      {
        if (++ NumBits > 12)
        {
          _WriteBits (ClearCode, NumBits - 1);
          _ClearStrtab (CodeSize);
          NumBits = CodeSize + 1;
        }
        Limit = (1 << NumBits) - 1;
      }
      Prefix = C;                  /* Set prefix to a string containing only the character read. Since all possible one-character */
    }                                                      /* strings exist in the table, there's no need to check if it is found */
  }
  if (Prefix != 0xFFFF)                                                              /* End of info is reached. Write last prefix */
    _WriteBits (Prefix, NumBits);
  _WriteBits (EndOfInfo, NumBits);                                                                      /* Write end of info-mark */
  _ResetOutBitFile ();                                                                                        /* Flush the buffer */
  _FreeStrtab ();                                                                                                      /* Tidy up */
  return (GIF_OK);
}

static short _BitsNeeded (word N)

/**********************************************************************************************************************************/
/* Pre   : `N' holds the number of numbers to store (0 to (n - 1)).                                                               */
/* Post  : Calculates the number of bits needed to store numbers between 0 and (n - 1). This number is returned.                  */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  short Ret = 1;

  if (!N --)
    return (0);
  while (N >>= 1)
    Ret ++;
  return (Ret);
}

static short _InputByte (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Get next pixel from the image. Called by the _LZW_Compress function. Returns next pixelvalue or -1 at the end.         */
/* Import: _GetPixel.                                                                                                             */
/**********************************************************************************************************************************/

{
  short Ret;

  if (_RelPixY >= _ImageHeight)
    return (-1);
  Ret = _GetPixel (_ImageLeft + _RelPixX, _ImageTop + _RelPixY);
  if (++ _RelPixX >= _ImageWidth)
  {
    _RelPixX = 0;
    _RelPixY ++;
  }
  return (Ret);
}

static byte _WriteScreenDescriptor (_ScreenDescriptor *Sd)

/**********************************************************************************************************************************/
/* Pre   : `Sd' points to the screen descriptor to output.                                                                        */
/* Post  : Output a screen descriptor to the current GIF-file. Returns GIF_ERRWRITE if an error occured.                          */
/* Import: _WriteWord, _WriteByte.                                                                                                */
/**********************************************************************************************************************************/

{
  byte Tmp;

  if (_WriteWord (Sd->LocalScreenWidth) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Sd->LocalScreenHeight) != GIF_OK)
    return (GIF_ERRWRITE);
  Tmp = (Sd->GlobalColourTableFlag << 7) | (Sd->ColourResolution << 4) | (Sd->SortFlag << 3) | Sd->GlobalColourTableSize;
  if (_WriteByte (Tmp) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Sd->BackgroundColourIndex) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Sd->PixelAspectRatio) != GIF_OK)
    return (GIF_ERRWRITE);
  return (GIF_OK);
}

static byte _WriteImageDescriptor (_ImageDescriptor *Id)

/**********************************************************************************************************************************/
/* Pre   : `Id' points to the image descriptor to output.                                                                         */
/* Post  : Output an image descriptor to the current GIF-file. Returns GIF_ERRWRITE if an error occured.                          */
/* Import: _WriteWord, _WriteByte.                                                                                                */
/**********************************************************************************************************************************/

{
  byte Tmp;

  if (_WriteByte (Id->Separator) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Id->LeftPosition) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Id->TopPosition) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Id->Width) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Id->Height) != GIF_OK)
    return (GIF_ERRWRITE);
  Tmp = (Id->LocalColourTableFlag << 7) | (Id->InterlaceFlag << 6) | (Id->SortFlag << 5) | (Id->Reserved << 3) |
         Id->LocalColourTableSize;
  if (_WriteByte (Tmp) != GIF_OK)
    return (GIF_ERRWRITE);
  return (GIF_OK);
}

static byte _WriteGraphicControlExtension (_GraphicControlExtension *Ext)

/**********************************************************************************************************************************/
/* Pre   : `Ext' points to the graphic control extension block to output.                                                         */
/* Post  : Output a graphic control extension block to the current GIF-file. Returns GIF_ERRWRITE if an error occured.            */
/* Import: _WriteWord, _WriteByte.                                                                                                */
/**********************************************************************************************************************************/

{
  byte Tmp;

  if (_WriteByte (Ext->ExtensionIntroducer) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Ext->GraphicControlLabel) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Ext->BlockSize) != GIF_OK)
    return (GIF_ERRWRITE);
  Tmp = (Ext->Reserved << 5) | (Ext->DisposalMethod << 2) | (Ext->UserInputFlag << 1) | Ext->TransparantColorFlag;
  if (_WriteByte (Tmp) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteWord (Ext->DelayTime) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Ext->TransparantColorIndex) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_WriteByte (Ext->BlockTerminator) != GIF_OK)
    return (GIF_ERRWRITE);
  return (GIF_OK);
}

static byte _GIFCreate (char *FileName, short Width, short Height, short NumColours, short ColourRes, byte GIF89a)

/**********************************************************************************************************************************/
/* Pre   : `FileName' points to the filename to create, `Width', `Height' hold the dimensions on the screen, `NumColours' holds   */
/*         the number of colours in the colourmaps, `ColourRes' holds the colour resolution - number of bits for each primary     */
/*         colour. `GIF89a' is TRUE is the GIF89a header should be written rather than the GIF87a header.                         */
/* Post  : Create a GIF-file and write the GIF and screen header. The return value is one of:                                     */
/*         GIF_OK        : Ok;                                                                                                    */
/*         GIF_ERRCREATE : Could not create file;                                                                                 */
/*         GIF_ERRWRITE  : Error writing to the file;                                                                             */
/*         GIF_OUTMEM    : Could not allocate the colour table.                                                                   */
/* Import: _Create, _Write, _BitsNeeded.                                                                                          */
/**********************************************************************************************************************************/

{
  int      register  Q;
  int                TabSize;
  byte              *Bp;
  _ScreenDescriptor  SD;

  _NumColours = NumColours ? (1 << _BitsNeeded (NumColours)) : 0;                          /* Initiate variables for new GIF-file */
  _BitsPrPrimColour = ColourRes;
  _ScreenHeight = Height;
  _ScreenWidth = Width;
  if (_Create (FileName) != GIF_OK)                                                                      /* Create file specified */
    return (GIF_ERRCREATE);
  if (GIF89a)
  {	
    if ((_Write ("GIF89a", 6)) != GIF_OK)                                                                  /* Write GIF signature */
      return (GIF_ERRWRITE);
  }
  else if ((_Write ("GIF87a", 6)) != GIF_OK)
    return (GIF_ERRWRITE);
  SD.LocalScreenWidth = Width;                                                            /* Initiate and write screen descriptor */
  SD.LocalScreenHeight = Height;
  if (_NumColours)
  {
    SD.GlobalColourTableSize = _BitsNeeded (_NumColours) - 1;
    SD.GlobalColourTableFlag = 1;
  }
  else
  {
    SD.GlobalColourTableSize = 0;
    SD.GlobalColourTableFlag = 0;
  }
  SD.SortFlag = 0;
  SD.ColourResolution = ColourRes - 1;
  SD.BackgroundColourIndex = 0;
  SD.PixelAspectRatio = 0;
  if (_WriteScreenDescriptor (&SD) != GIF_OK)
    return (GIF_ERRWRITE);
  if (_ColourTable)                                                                                      /* Allocate colour table */
  {
    free (_ColourTable);
    _ColourTable = NULL;
  }
  if (_NumColours)
  {
    TabSize = _NumColours * 3;
    if ((_ColourTable = (byte *)malloc (TabSize)) == NULL)
      return (GIF_OUTMEM);
    else
    {
      Bp = _ColourTable;
      for (Q = 0 ; Q < TabSize ; Q ++)
        *Bp ++ = 0;
     }
  }
  return (0);
}

static void _GIFSetColour (byte ColourNum, byte Red, byte Green, byte Blue)

/**********************************************************************************************************************************/
/* Pre   : `ColourNum' holds the the colour index to set in range [0, _NumColours - 1], `Red', `Green', `Blue' hold the           */
/*         components.                                                                                                            */
/* Post  : Set red, green and blue component of one of the colours. They are all in the range [0, (1 << _BitsPrPrimColour) -1].   */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  dword  MaxColour;
  byte  *P;

  MaxColour = (1L << _BitsPrPrimColour) - 1L;
  P = _ColourTable + ColourNum * 3;
  *P ++ = (byte)((Red * 255L) / MaxColour);
  *P ++ = (byte)((Green * 255L) / MaxColour);
  *P ++ = (byte)((Blue * 255L) / MaxColour);
}

static byte _GIFWriteGlobalColorTable (byte GIF89a)

/**********************************************************************************************************************************/
/* Pre   : `GIF89a' is TRUE if animated GIFs are written. Should be used after each successive _GETSetColor call.                 */
/* Post  : Write the global color table to the GIF file. If animated GIFs are written, insert a LOOP control block as well.       */
/* Import: _Write.                                                                                                                */
/**********************************************************************************************************************************/

{
  if (_NumColours)                                                                            /* Write global colour table if any */
    if ((_Write (_ColourTable, _NumColours * 3)) != GIF_OK)
      return (GIF_ERRWRITE);
  if (GIF89a)
    if ((_Write (&_ApplicationExtensionHeader, 19)) != GIF_OK)                                      /* Write Netscape Loop Header */
      return (GIF_ERRWRITE);
  return (0);
}

static byte _GIFCompressImage (short StartX, short StartY, int Width, int Height, short (*GetPixelFunction)(short PixX, short PixY),
                               byte GIF89a, word PictureDelayTime)

/**********************************************************************************************************************************/
/* Pre   : `StartX', `StartY' holds the screen-relative pixel coordinates of the image, `Width', `Height' the dimensions.         */
/*         `GetPixelFunction' points to a callback function used to fetch the color value of each pixel of the image.             */
/*         `GIF89a' is TRUE is animated GIFs are written, in which case `PictureDelayTime' holds the delay time to write this     */
/*         image in 1/100'th second resolution.                                                                                   */
/* Post  : Compress an image into the GIF-file previously created using _GIFCreate. All colour values should have been specified  */
/*         before this function is called. The pixels are retrieved using a user defined callback function. This function should  */
/*         accept two parameters, X and Y, specifying which pixel to retrieve. The pixel values sent to this function are as      */
/*         follows: X = [Left, Left + Width - 1], Y = [Top, Top + Height - 1]. The function should return the pixel value for the */
/*         point given, in the interval [0, _NumColours - 1]. The return value is GIF_OK, GIF_OUTMEM or GIF_ERRWRITE.             */
/* Import: _Write, _WriteImageDescriptor, _BitsNeeded, _LZW_Compress, _WriteByte.                                                 */
/**********************************************************************************************************************************/

{
  int                      CodeSize;
  int                      ErrCode;
  _ImageDescriptor         ID;
  _GraphicControlExtension Ext;

  if (Width < 0)
  {
    Width = _ScreenWidth;
    StartX = 0;
  }
  if (Height < 0)
  {
    Height = _ScreenHeight;
    StartY = 0;
  }
  if (StartX < 0)
    StartX = 0;
  if (StartY < 0)
    StartY = 0;
  if (GIF89a)
  {	
    Ext.ExtensionIntroducer = 0x21;                                               /* Initiate and write graphic control extension */
    Ext.GraphicControlLabel = 0xF9;
    Ext.BlockSize = 4;
    Ext.DisposalMethod = 0;                                                                            /* (No disposal specified) */
    Ext.UserInputFlag = 0;                                                                            /* (No user input expected) */
    Ext.TransparantColorFlag = 0;                                                                      /* (No transparant colour) */
    Ext.DelayTime = PictureDelayTime;
    Ext.TransparantColorIndex = 0;
    Ext.BlockTerminator = 0;
    if (_WriteGraphicControlExtension (&Ext) != GIF_OK)
      return (GIF_ERRWRITE);
  }
  ID.Separator = 0x2C;                                                                     /* Initiate and write image descriptor */
  ID.LeftPosition = _ImageLeft = StartX;
  ID.TopPosition = _ImageTop = StartY;
  ID.Width = _ImageWidth = Width;
  ID.Height = _ImageHeight = Height;
  ID.LocalColourTableSize = 0;
  ID.Reserved = 0;
  ID.SortFlag = 0;
  ID.InterlaceFlag = 0;
  ID.LocalColourTableFlag = 0;
  if (_WriteImageDescriptor (&ID) != GIF_OK)
    return (GIF_ERRWRITE);
  CodeSize = _BitsNeeded (_NumColours);                                                                        /* Write code size */
  if (CodeSize == 1)
    CodeSize ++;
  if (_WriteByte ((byte)CodeSize) != GIF_OK)
    return (GIF_ERRWRITE);
  _RelPixX = _RelPixY = 0;                                                                                 /* Perform compression */
  if ((ErrCode = _LZW_Compress (CodeSize, GetPixelFunction)) != GIF_OK)
    return (ErrCode);
  if (_WriteByte (0) != GIF_OK)                                                                       /* Write terminating 0-byte */
    return (GIF_ERRWRITE);
  return (GIF_OK);
}

static byte _GIFClose (void)

/**********************************************************************************************************************************/
/* Pre   : None.                                                                                                                  */
/* Post  : Close the GIF-file.                                                                                                    */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  _ImageDescriptor ID;

  ID.Separator = ';';                                                               /* Initiate and write ending image descriptor */
  if (_WriteImageDescriptor (&ID) != GIF_OK)
    return (GIF_ERRWRITE);
  _Close ();                                                                                                        /* Close file */
  _FreeStrtab ();                                                                                             /* Just in case ... */
  if (_ColourTable)                                                                                       /* Release colour table */
  {
    free (_ColourTable);
    _ColourTable = NULL;
  }
  return (GIF_OK);
}

static char *_GIFstrerror (byte ErrorCode)

/**********************************************************************************************************************************/
/* Pre   : `ErrorCode' holds the error code to be returned.                                                                       */
/* Post  : A string describing the error is returned.                                                                             */
/* Import: None.                                                                                                                  */
/**********************************************************************************************************************************/

{
  switch (ErrorCode)
  {
    case GIF_OK        : strcpy (_GIFErrorMessage, "(Call succesfull)"); break;
    case GIF_ERRCREATE : strcpy (_GIFErrorMessage, "Error creating file"); break;
    case GIF_ERRWRITE  : strcpy (_GIFErrorMessage, "Error writing to file"); break;
    case GIF_OUTMEM    : strcpy (_GIFErrorMessage, "Out of memory"); break;
    case GIF_OUTMEM2   : strcpy (_GIFErrorMessage, "Out of memory!"); break;
    default            : sprintf (_GIFErrorMessage, "Undefined error %02X", ErrorCode);
  }
  return (_GIFErrorMessage);
}

/**********************************************************************************************************************************/
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> START OF MAIN PROGRAM <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
/**********************************************************************************************************************************/

dword SpecPalette[16] = { 0x00000000, 0x002F0000, 0x0000002F, 0x002F002F, 0x00002F00, 0x002F2F00, 0x00002F2F, 0x002A2A2A,
                          0x00000000, 0x003F0000, 0x0000003F, 0x003F003F, 0x00003F00, 0x003F3F00, 0x00003F3F, 0x003F3F3F };

byte  ScreenBuffer[6912];
byte  PreparedScreenData[49152U];                                                               /* (Make room for 256*192 pixels) */
byte  Use89aForFLASH = FALSE;
byte  ScreenContainsFLASH;
byte  ConvertTo89a;

short MyGetPixel (short PixX, short PixY)

/**********************************************************************************************************************************/
/* Pre   : (`PixX', `PixY') are the coordinates of the requested pixel.                                                           */
/*         (This is the callback function from _LZX_Compress)                                                                     */
/* Post  : The appropriate pixel colour value has been returned.                                                                  */
/**********************************************************************************************************************************/

{
  return ((short)PreparedScreenData[PixY * 256 + PixX]);
}

byte SpectrumScreenToGIF (byte FLASHState, byte ConvertTo89a)

/**********************************************************************************************************************************/
/* Pre   : `FLASHState' is TRUE if animated GIFs are written and this is the FLASH 1 image.                                       */
/* Post  : The buffered SCR image has been inserted into the open GIF file.                                                       */
/**********************************************************************************************************************************/

{
  byte            PaletteData;
  byte            PaletteINK;
  byte            PalettePAPER;
  byte            PixelData;
  byte            PixelMask;
  byte            ScreenBlockIndex;
  word            ScreenBlockCount;
  word  register  PreparedDataIndex;
  word  register  ScreenDataIndex;
  word  register  PaletteIndex;
  short register  CntY;                                                                              /* Vertical scanline counter */
  short register  CntX;                                                                                /* Horizontal byte counter */

  PaletteIndex = 6144;                                                                            /* Start of the attributes area */
  PreparedDataIndex = 0;
  for (ScreenBlockCount = 0 ; ScreenBlockCount < 6144 ; ScreenBlockCount += 2048)    /* A Spectrum screen is composed of 3 blocks */
    for (CntY = 0 ; CntY < 256 ; CntY += 32)                                                   /* 8 character lines in each block */
    {
      ScreenDataIndex = ScreenBlockCount + CntY;
      for (ScreenBlockIndex = 0 ; ScreenBlockIndex < 8 ; ScreenBlockIndex ++)                  /* Each character has 8 scan lines */
      {
        for (CntX = 0 ; CntX < 32 ; CntX ++)                                               /* And each scanline contains 32 bytes */
        {
          PaletteData  = *(ScreenBuffer + PaletteIndex + CntX);
          if ((PaletteData & 0x80) && FLASHState)                                                              /* FLASH 1 state ? */
          {
            PaletteINK   = (PaletteData & 0x78) >> 3;
            PalettePAPER = (PaletteData & 0x07) | ((PaletteData & 0x40) >> 3);
          }
          else                                                                                       /* (No FLASH or first state) */
          {
            PaletteINK   = (PaletteData & 0x07) | ((PaletteData & 0x40) >> 3);
            PalettePAPER = (PaletteData & 0x78) >> 3;
          }
          PixelData    = *(ScreenBuffer + ScreenDataIndex + CntX);
          for (PixelMask = 128 ; PixelMask ; PixelMask >>= 1)                                 /* 8 bits in a byte (left to right) */
            PreparedScreenData[PreparedDataIndex ++] = (PixelData & PixelMask ? PaletteINK : PalettePAPER);
        }
        ScreenDataIndex += 256;                                                       /* One spectrum scan line down is 256 bytes */
      }
      PaletteIndex += 32;                                                                     /* Palette is `correctly' addressed */
    }

  /* All set? Here goes! */
  /* If a FLASHING screen is written, the inter-image pause is 32/100 = 16/50's of a second, just like a real Speccy :-) */

  return (_GIFCompressImage (0, 0, 256, 192, &MyGetPixel, ConvertTo89a, 32));
}

//int mainSCR2GIF (int argc, char **argv)
//
///**********************************************************************************************************************************/
///* Pre   : None.                                                                                                                  */
///* Post  : All SCR files specified on the command line have been converted to (animated) GIF files.                               */
///**********************************************************************************************************************************/
//
//{
//  FILE           *InputFile;
//  char            InputFileName[256];
//  char            OutputFileName[256];
//  int             FileIndex = 0;
//  word            BIndex;
//  byte  register  ColourCount;
//  byte            ErrCode;
//  dword          *PP;                                                                                          /* Palette Pointer */
//
//  printf ("\nSCR2GIF v1.0 - Copyright (C) 1998 M. van der Heide, ThunderWare Research Center\n\n");
//  if (argc < 2)                                                                                                    /* Moron check */
//  {
//    fprintf (stderr, "Usage: scr2gif [-f] filename[.scr] ...\n");
//    fprintf (stderr, "       -f = Convert FLASHing screens to animated GIF\n");
//    exit (1);
//  }
//  if (!strcmp (argv[1], "-f") || !strcmp (argv[1], "-F"))                            /* Want animated GIFs for FLASHing screens ? */
//  {
//    if (argc < 3)                                                                                                /* (Moron check) */
//    {
//      fprintf (stderr, "Usage: scr2gif [-f] filename[.scr] ...\n");
//      fprintf (stderr, "       -f = Convert FLASHing screens to animated GIF\n");
//      exit (1);
//    }
//    Use89aForFLASH = TRUE;
//    FileIndex = 1;
//  }
//  while (++ FileIndex < argc)                                                   /* Do all SCR files specified on the command line */
//  {
//    strcpy (InputFileName, argv[FileIndex]);
//    if ((InputFile = fopen (InputFileName, "rb")) == NULL)                                            /* Try to open the SCR file */
//    {
//      strcat (InputFileName, ".scr");
//      if ((InputFile = fopen (InputFileName, "rb")) == NULL)
//      {
//        strcpy (InputFileName + strlen (InputFileName) - 3, "SCR");
//        if ((InputFile = fopen (InputFileName, "rb")) == NULL)
//        { printf ("Cannot find any file named %s, %s.scr or %s.SCR, so there!\n",
//                  argv[FileIndex], argv[FileIndex], argv[FileIndex]); exit (1); }
//      }
//    }
//    strcpy (OutputFileName, InputFileName);
//    if (!strcmp (OutputFileName + strlen (OutputFileName) - 4, ".scr"))
//      strcpy (OutputFileName + strlen (OutputFileName) - 3, "gif");                                 /* Determine output file name */
//    else if (!strcmp (OutputFileName + strlen (OutputFileName) - 4, ".SCR"))
//      strcpy (OutputFileName + strlen (OutputFileName) - 3, "GIF");                                            /* (Maintain case) */
//    else
//      strcat (OutputFileName, ".gif");
//    printf ("Converting %s to %s ... ", InputFileName, OutputFileName);
//    fflush (stdout);
//    if (fread (ScreenBuffer, 1, 6912, InputFile) != 6912)
//    { printf ("\nSorry, %s doesn't look like a SCR file to me!\n", InputFileName); fclose (InputFile); exit (1); }
//    fclose (InputFile);
//    ScreenContainsFLASH = FALSE;
//    if (Use89aForFLASH)
//      for (BIndex = 6144 ; BIndex < 6912 && !ScreenContainsFLASH ; BIndex ++)                           /* Check if there's FLASH */
//        if (*(ScreenBuffer + BIndex) & 0x80)
//          ScreenContainsFLASH = TRUE;
//    ConvertTo89a = (byte)(Use89aForFLASH && ScreenContainsFLASH);                   /* No need for animation is there's no FLASH! */
//    printf (ConvertTo89a ? "animated ... " : "single image ... ");
//    fflush (stdout);
//    if ((ErrCode = _GIFCreate (OutputFileName, 256, 192, 16, 6, ConvertTo89a)) != GIF_OK)
//    { printf ("\nCannot create GIF file (%s)\n", _GIFstrerror (ErrCode)); exit (1); }
//    PP = SpecPalette;
//    for (ColourCount = 0 ; ColourCount < 16 ; ColourCount ++, PP ++)
//      _GIFSetColour (ColourCount, (byte)(*PP & 0x000000FF), (byte)((*PP & 0x0000FF00) >> 8), (byte)((*PP & 0x00FF0000) >> 16));
//    _GIFWriteGlobalColorTable (ConvertTo89a);
//    if ((ErrCode = SpectrumScreenToGIF (FALSE)) != GIF_OK)
//    { printf ("\nCannot compress GIF image (%s)\n", _GIFstrerror (ErrCode)); _GIFClose (); unlink (OutputFileName); exit (1); }
//    if (ConvertTo89a)
//      if ((ErrCode = SpectrumScreenToGIF (TRUE)) != GIF_OK)
//      { printf ("\nCannot compress GIF image (%s)\n", _GIFstrerror (ErrCode)); _GIFClose (); unlink (OutputFileName); exit (1); }
//    if ((ErrCode = _GIFClose ()) != GIF_OK)
//    { printf ("\nCannot close GIF file (%s)\n", _GIFstrerror (ErrCode)); unlink (OutputFileName); exit (1); }
//    puts ("done");
//  }
//  return (0);
//}

void ConvertSCR2GIF(byte* scrBuf, const char* gifName)
{
	byte ColourCount;
	byte ErrCode;
	dword* PP;                                                                                          /* Palette Pointer */
	byte ScreenContainsFLASH = FALSE;
	word            BIndex;

	memcpy(ScreenBuffer, scrBuf, 6912);
	for (BIndex = 6144 ; BIndex < 6912 && !ScreenContainsFLASH ; BIndex ++)                           /* Check if there's FLASH */
		if (*(ScreenBuffer + BIndex) & 0x80)
			ScreenContainsFLASH = TRUE;	
	ErrCode = _GIFCreate((char*)gifName, 256, 192, 16, 6, ScreenContainsFLASH);
	if (ErrCode == GIF_OK)
	{
		PP = SpecPalette;
		for (ColourCount = 0 ; ColourCount < 16 ; ColourCount ++, PP ++)
			_GIFSetColour (ColourCount, (byte)(*PP & 0x000000FF), (byte)((*PP & 0x0000FF00) >> 8), (byte)((*PP & 0x00FF0000) >> 16));
	}
	
	if (ErrCode == GIF_OK)
		ErrCode = _GIFWriteGlobalColorTable(ScreenContainsFLASH);

	if (ErrCode == GIF_OK)
		ErrCode = SpectrumScreenToGIF(FALSE, ScreenContainsFLASH);

	if (ErrCode == GIF_OK && ScreenContainsFLASH)
		ErrCode = SpectrumScreenToGIF(TRUE, ScreenContainsFLASH);
	
	if (ErrCode == GIF_OK)
		ErrCode = _GIFClose();
}
