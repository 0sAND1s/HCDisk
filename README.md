HCDisk 2.0 - PC utility for Sinclair Spectrum computer file transfer.
George Chirtoaca, 2014 - 2022, george.chirtoaca@gmail.com

1. What is it?
2. What can it do?
3. What are the usage scenarios?
4. Where does the name come from?
5. What commands are available?
6. What about the source code?
7. What features are planned?


## 1. What is it?

	HCDisk is a tool able to transfer files to and from several file systems and interfaces specific to the Sinclair Spectrum computer and clones. 

## 2. What can it do?

	It can understand these file systems:
- Generic CPM - Spectrum +3 and various clones (HC, CoBra)
- +3 BASIC (CPM based)
- Betadisk TR-DOS (read only)
- Miles Gordon Technologies +D (read only)
- Opus Discovery (read only)
- ICE Felix HC BASIC (Romanian clone, CPM based)
- ICTI CoBra Devil FS (Romanian clone) (read only)
- Electronica CIP-04 (Spectrum +3 clone from Romania, with 3.5 disk drive)
File systems based on CPM are read-write, the others are read only (for now).
	
	Can also send TAP and TZX (including copy protected ones) files over to a Spectrum computer via audio output of PC.
	Can also send TAP/TZX (standard blocks) to a Spectrum computer with IF1 interface via the PC RS-232 serial COM port.

	Several disk/tape image formats are supported:
- Physical disks inserted into the PC floppy drive (any format) or USB floppy drive (for CP/M formats only)
- RAW, including TRD, MGT, OPD
- DSK/EDSK - CPCEMU
- CQM (read only) - Copy QM by Sydex
- SCL (read only) - Russian emulators
- TD0 - Teledisk by Sydex (read only)
- Tape images: TAP, TZX
	
	Note: For accessing floppy disks in physical drive for all supported formats, you need Simon Owen's fdrawcmd driver: http://simonowen.com/fdrawcmd/ .
For standard geometries supported by Windows, the standard Windows driver is now used (added in January 2022). This also works with USB floppy drives for modern computers without a floppy controller.
The geometries supported by Windows are using sector size of 512 bytes and also 40 and 80 tracks, with 8, 9, 15 and 18 sectors per track, single or dual sided. The USB floppy drives only support 80x2x9 DD and 80x2x18 HD TxHxS.	

	It also includes file conversion for Spectrum Specific files, to be viewed on PC:
- converts Spectrum BASIC programs to BASIC source code 
- converts Spectrum SCREEN$ files to animated GIFs
- includes a disassembler for CODE files
- includes a hex viewer
	
## 3. What are the usage scenarios?

- browse and view files from physical disks, disk images or tape images; an extended
file catalog is displayed (read text documents from CPM disks, browse your old BASIC programs, etc)
- transfer physical disks into disk images for preservation or use on emulators
- export and import files between different file systems (Spectrum +3 to MGT +D, Opus to Tape, etc)
- disk to tape and tape to disk conversion, now including BASIC loader automatic conversion
- tape image to sound signal conversion or WAV file (for use on a Spectrum without disk)
- disk image conversion between formats (RAW to DSK, CQM to RAW, etc)
- create physical disks from downloaded disk images, to be used on the real machines
- send TAP/TZX files to Spectrum via audio
- send TAP/TZX/DSK files to Spectrum via COM port

PC		|		Transfer method		|		Spectrum
--------|---------------------------|--------------------------------
Audio out jack |	PC plays TAP/TZX		|	Loading from ear in
Floppy disk drive|	Read/Write disks		|	Read/write floppy disks
USB floppy disk	|	Read/Write disks		|	Read/write CP/M floppy disks
Disk images	|		PC reads/writes disks	|	Read/write floppy disks
Disk images	|		PC reads/writes images	|	GoTek emulator reads/writes disk images
COM port	|		PC Upload/download files|	Read/write files using IF1 COM port


## 4. Where does the name come from?

	HC is the name of a Sinclair Spectrum compatible computer series manufactured in Romania by the I.C.E. Felix factory. I had one as my first computer.
	The first version of HCDisk only understood the file system specific to HC, but I kept the name, even if now it can process file systems for several other clones and add-ons. Not by coincidence, HC is also an acronym for Home Computer. So this tool applies to home computers.

## 5. What commands are available?
Note command argument convention notation:
	<argument> - means mandatory argument
	[argument] - means optional argument
	arguments start with minus sign
	value follows the argument

-----------------------------------------------------------------------------------------------------------------------
help ?  - Command list, this message
- [command]: Show help only for the specified command.

-----------------------------------------------------------------------------------------------------------------------
fsinfo  - Display the known file systems

Displays the known file systems, with details, from the program help (geometry, block size, block count, directory capacity, 
boot track count):

Idx/Name           |Geometry   |Bl.Sz. |Bl.Cnt |Dir |Boot   |Writable
-------------------|-----------|-------|-------|----|-------|---------
 1\. HC BASIC 5.25" |40x2x16x256|2048   |160    |128 |0      |yes
 2\. HC BASIC 3.5"  |80x2x16x256|2048   |320    |128 |0      |yes
 3\. GENERIC CP/M 2.2 5.25"   |40x2x9x512  |2048   |175 |64  |2   |yes
 4\. GENERIC CP/M 2.2 3.5" |80x2x9x512  |2048   |351 |128 |4   |yes
 5\. Spectrum +3 BASIC 180K   |40x1x9x512  |1024   |175 |64  |1   |yes
 6\. Spectrum +3 BASIC 203K   |42x1x10x512 |1024   |205 |64  |1   |yes
 7\. Spectrum +3 BASIC 720K   |80x2x9x512  |2048   |357 |256 |1   |yes
 8\. Spectrum +3 BASIC PCW |40x2x9x512  |2048   |175 |64  |1   |yes
 9\. Spectrum +3 CP/M SSDD |40x1x9x512  |1024   |175 |64  |1   |yes
10\. Spectrum +3 CP/M DSDD |80x2x9x512  |2048   |357 |256 |1   |yes
11\. TRDOS DS 3.5"   |80x2x16x256 |256 |2544   |128 |1   |no
12\. TRDOS DS 5.25"  |40x2x16x256 |256 |1264   |128 |1   |no
13\. TRDOS SS 3.5"   |80x1x16x256 |256 |1264   |128 |1   |no
14\. TRDOS SS 5.25"  |40x1x16x256 |256 |624 |128 |1   |no
15\. Opus Discovery 40T SS |40x1x18x256 |256 |720 |112 |0   |no
16\. Opus Discovery 40T DS |40x2x18x256 |256 |1440   |112 |0   |no
17\. Opus Discovery 80T SS |80x1x18x256 |256 |2880   |112 |0   |no
18\. Opus Discovery 80T DS |80x2x18x256 |256 |5760   |112 |0   |no
19\. MGT +D    |80x2x10x512 |512 |3125   |80  |0   |no
20\. CoBra Devil  |80x2x18x256 |9216   |77  |108 |0   |no
21\. Electronica CIP-04 |80x1x9x512  |1024   |355 |64  |1   |yes

Known containers:
- A:/B: - PHYSICAL DISK (RW)
- RAW - DISK IMAGE (RW)
- DSK - CPCEMU DISK IMAGE (RW)
- EDSK - CPCEMU DISK IMAGE (RW)
- TRD - TR-DOS DISK IMAGE
- SCL - TR-DOS DISK IMAGE
- CQM - Sydex COPYQM DISK IMAGE
- OPD - OPUS Discovery DISK IMAGE
- MGT - Miles Gordon Tech DISK IMAGE
- TAP - TAPE IMAGE (RW)
- TZX - TAPE IMAGE (RW)
- TD0 - Sydex Teledisk DISK IMAGE

-----------------------------------------------------------------------------------------------------------------------
stat  - Display the current file system parameters: (total/free blocks, total/free space, disk geometry, disk type, etc)
	Example for HC BASIC:

	HC BASIC 3.5">stat
	Container   : EDSK - CPCEMU DISK IMAGE (RW)
	File system : HC BASIC 3.5"
	Path  : $ULTIMELE.DSK
	Disk geometry  : 80T x 2H x 16S x 256B/S
	Hard sector skew  : 0
	Raw Size : 640 KB
	Block size  : 2.00 KB
	Blocks free/max   : 6/320
	Disk free/max KB  : 12/640 = 1.88% free
	Catalog free/max  : 76/128 = 59.38% free
	File system features : Sinclair Spectrum, File attributes, File folders, Case sensitive names,
	File name structure  : 11.0 (name.extension)


-----------------------------------------------------------------------------------------------------------------------
open  - Open disk or disk image
- \<drive|image>: The disk/image to open
- [-t] x: The number of file system type to use - optionaly, auto select the type from the list
	The program tries to auto detect the file system based on disk geometry. If several matches are found,
a list is displayed for the user to pick one. TRDos is the only file system that has a signature that can be check,
otherwise there's no easy way to detect if the selection is valid, and errors can occur, like strange, unreadable file names.

-----------------------------------------------------------------------------------------------------------------------
close  - Close disk or disk image

-----------------------------------------------------------------------------------------------------------------------
ls dir  - List directory
- [<folder><\>file spec.]: filespec: *.com or 1\*, etc - folder for CPM can be specified with back slash
- [-sn|-ss|-st]: Sort by name|size|type
- [-ne]: Don't show extended info, faster - doesn't access the file header, and it's faster for physical disks
- [-del]: Include deleted files in listing. 

	Example of HC BASIC disk catalog. Other file systems for Spectrum look similar. Information displayed is:
file index, file folder (for CPM), file name and extension, file size on disk, file attributes (if available for the file system). 
For BASIC file systems, there's also specific info displayed: file type, code start address/program start line, and lenght.

		HCDisk 2.0 by George Chirtoaca, compiled on Apr 24 2024 09:31:11.
		Container       : EDSK - CPCEMU DISK IMAGE (RW).
		File system     : HC BASIC 3.5".
		HC BASIC 3.5">dir
		
		IDX     Folder  Name            Size(KB)        Attr    Type    Start   BasLen  VarLen
		--------------------------------------------------------------------------------------
		 01     0       Saboteur2        2.00           ---     Program    10     176       0
		 02     0       sab2scr          8.00           ---     Bytes   40000    6912
		 03     0       sab2code        40.00           ---     Bytes   25100   40436
		 04     0       sab1scr          8.00           ---     Bytes   40000    6912
		 05     0       Saboteur1        2.00           ---     Program    10     157       0
		 06     0       Serpenti         2.00           ---     Program    10      94       0
		 07     0       sab1code        40.00           ---     Bytes   25200   40336
		 08     0       serpcode        26.00           ---     Bytes   32768   25000
		 09     0       Turtles          2.00           ---     Program    10     188       0
		 10     0       turt_scr         8.00           ---     Bytes   40000    6912
		 11     0       turt_c1         40.00           ---     Bytes   25000   40535
		 12     0       turt_c2          8.00           ---     Bytes   40000    6912
		 13     0       TurboEspr.       2.00           ---     Program    10     203       0
		 14     0       te1              8.00           ---     Bytes   40000    7168
		 15     0       te2             38.00           ---     Bytes   26731   38351
		 16     0       Video Pool       2.00           ---     Program    10     116       0
		 17     0       vp_code         26.00           ---     Bytes   32768   25000
		 18     0       run              2.00           ---     Program     1     775      47
		 19     0       zx_test          2.00           ---     Program    10      68       0
		 20     0       zxtestcode       6.00           ---     Bytes   40000    4800
		 21     0       Harrier          2.00           ---     Program     1     254       0
		 22     0       har_scr          8.00           ---     Bytes   30000    6912
		 23     0       har_code         8.00           ---     Bytes   40000    6385
		 24     0       manic1.p         2.00           ---     Program    10     166       0
		 25     0       manic1_1.c       2.00           ---     Bytes   32768     256
		 26     0       manic1_2.c      34.00           ---     Bytes   32768   32767
		 27     0       manic2.p         2.00           ---     Program    10      76       0
		 28     0       manic2.c        34.00           ---     Bytes   32000   33536
		 29     0       manic3.p         2.00           ---     Program    10     237       0
		 30     0       manic3.$         4.00           ---     Bytes   30000    4022
		 31     0       manic3.c        20.00           ---     Bytes   32768   18943
		 32     0       manic4.p         2.00           ---     Program    10     191       0
		 33     0       manic4.c        18.00           ---     Bytes   32768   16628
		 34     0       intro.p          2.00           ---     Program  8000     695      31
		 35     0       catalog.1        2.00           ---     Chr.Arr     F     236
		 36     0       intro1.$         8.00           ---     Bytes   40000    6912
		 37     0       intro2.$         8.00           ---     Bytes   50000    6912
		 38     0       intro.c          2.00           ---     Bytes   65000     536
		 39     0       mons4.c          8.00           ---     Bytes   32768    6528
		 40     0       gens4.c         12.00           ---     Bytes   32768   10880
		 41     0       service.p        2.00           ---     Program     1    1880      84
		 42     0       gens4.p          2.00           ---     Program    10      71       0
		 43     0       mons4.p          2.00           ---     Program     1      71       0
		 44     0       chuckie.p        2.00           ---     Program    10     161       0
		 45     0       chuckie.c       20.00           ---     Bytes   33300   18900
		 46     0       loderun.p        2.00           ---     Program    10     101       5
		 47     0       loderun.$        8.00           ---     Bytes   32768    6912
		 48     0       loderun.c       42.00           ---     Bytes   24576   40959
		 49     0       bignose.p        2.00           ---     Program     1     107       0
		 50     0       bignose.$        8.00           ---     Bytes   32768    6912
		 51     0       bignose.c       40.00           ---     Bytes   25344   40192
		 52     0       dip.p            2.00           ---     Program    10     721       0
		 53     0       dip.c            4.00           ---     Bytes   24832    2816
		 54     0       ed.p             2.00           ---     Program     1    1094      25
		 55     0       du.p             2.00           ---     Program    10     179       0
		 56     0       du.c             6.00           ---     Bytes   49152    4706
		 57     0       t2d.p            2.00           ---     Program    10     202       0
		 58     0       t2d.c            2.00           ---     Bytes   16384    1568
		 59     0       voice.p          2.00           ---     Program  8000     928       0
		 60     0       voice.c          2.00           ---     Bytes   60000     801
		 61     0       menu.old         4.00           ---     Program          2753
		 62     0       catalog.2        2.00           ---     Chr.Arr     N     446
		 63     0       menu.p           4.00           ---     Program     1    2457     848
		Space free/filled/total : 020/620/640 KB

-----------------------------------------------------------------------------------------------------------------------
get  - Copy file(s) to PC
- \<["]filespec["]>: * or *.com or readme.txt, "1 2", etc - names with spaces are supported, but must be enclosed in quotes
- [-t]: Copy as text - only display printable chars, usefull for Tasword files
- [-n newName]: Set the PC file name, default: original name

Examples:
- get run		- will copy from the current disk to PC the file named 'run'.
- get "Dizzy ?"		- will copy from the current disk to PC all files named like "Dizzy 1", "Dizzy 2", "Dizzy 3", etc.
- get "A B C" -n "ABC"	- will copy from the current disk to PC the file named "A B C" but will set the PC file name to "ABC".

-----------------------------------------------------------------------------------------------------------------------
type cat  - Display file
- \<file spec.>: * or *.com or readme.txt, etc
- [-h]: display as hex
- [-t]: display as text
- [-d]: display as Z80 assembler

Examples:
- type run		- will list program 'run' as BASIC code
- type 1.asm -t		- will list text file as text
- type 1.bin -h		- will list binary file as hex
- type 1.bin -d		- will list binary file as Z80 assembler

	BASIC programs are decoded into source code. Numeric values are displayed, but only if are different from the textual representations. 
Also, embedded attributes are displayed (as text). The program variables values are displayed, if saved with the 
program, as it is the case when saving a program after being run, without first CLEARing the memory.
	SCREEN$ files are displayed as animated GIF files.
	Code files are disassembled.
	Any file can also be displayed as hexadecimal.

-----------------------------------------------------------------------------------------------------------------------
copydisk  - Copy current disk to another disk or image
- \<destination>: destination disk/image - if writing to physical a disk, that disk must be properly formatted
- [-f]: format destination disk while copying

Examples:
- open A: : copydisk image.dsk -f	- will copy current disk (A:) to image file; destination image file must exist and must be formatted with the same format as source OR
					- f argument will create and format the destination disk or image file.
- copydisk B:				- will copy the image file to physical disk in drive B:
- format hc.dsk -t 2 : open A: : copydisk hc.dsk	- will copy from HC disk in drive A: to disk image hc.dsk.

-----------------------------------------------------------------------------------------------------------------------
copyfs - Copy only used blocks from current file system to another disk (same file system type, CP/M only)
- \<direction>: 'to'/'from'
- \<remote>: source/destination disk image (i.e. 1.dsk) or COM port (i.e. COM1)

Examples:
- copyfs from COM1 - will write curret disk with file system blocks read from COM1
- copyfs to COM2 - will copy file system blocks from current disk to COM2 port

How to copy HC BASIC disks over serial cable (COM port):
1. Notice in Windows device manager the name of the COM port (COM1, COM2, etc).
2. Run the latest HCDisk version and open or create a disk image in the format for HC BASIC 3.5.
3. In HCDisk use command 'copyfs to COM1' to copy from PC to HC or command 'copyfs from COM1' to copy from HC to PC.
4. Run the latest HCCmd version and use menu '9-Disk', then option '3. Copy A:->COM' to copy from HC to PC or option '4. Copy COM->A:' to copy from PC to HC.
5. A message will show on PC and on HC showing how many blocks are left to copy. A full disk takes about 7 minutes to copy.

-----------------------------------------------------------------------------------------------------------------------
put  - Copy PC file to file system
- \<source file>: the file to copy
- [-n newname]: name for destination file
- [-d \<destination folder>]: CP/M file folder
- [-s start]: Spectrum program start line/byte start address
- [-t p|b|c|n file type]: Spectrum file type: program, bytes, char arr., no. arr
- [-turbo \<1364|2250|3000|6000>]: Turbo baud rate for TZX blocks

Examples:
- put dizzy.scr -n "Dizzy1SCR" -t b -s 16384
will copy a file from PC to the current Spectrum disk; the PC file name is dizzy.scr, the new file name on destination will be Dizzy1SCR, the type will be Bytes (b), the start address will be 16384.
- put "pc file" -n "HC file" -t p -s 10
will copy a file from PC to the current Spectrum disk; the PC file name is "pc file" (includes space, so quotes must be used), the Spectrum name will be "HC file" (also containing space), the type will pe Program (p), the start line of program will be 10.

-----------------------------------------------------------------------------------------------------------------------
del rm  - Delete file(s)
- \<file spec.>: the file(s) to delete, ex: del *.com; A confirmation is displayed.

-----------------------------------------------------------------------------------------------------------------------
ren  - Rename file
- \<file name>: the file to rename
- \<new name>: new file name

-----------------------------------------------------------------------------------------------------------------------
!  - Execute DOS command after '!' -  will execut the shell command. Note the space after !.
- \<DOS command>: ! dir, ! mkdir, etc

-----------------------------------------------------------------------------------------------------------------------
tapplay  - Play the tape as sound asynchronously
- [-w]: play to a .wav file instead of real-time
	You can connect the PC audio output to a Spectrum tape input, and load the tape image.
Copy protected programs, like Speedlock are supported, in TZX images.

Example of async. tape playing:
Playing the tape. Press ESC to quit, SPACE to pause. Block count is: 80.
00:  Std. Block   Program: run   19   00
01:  Std. Block   Data   : 1288 FF
Progress: 77 %

-----------------------------------------------------------------------------------------------------------------------
tapexp  - Exports the files to a tape image
- \<.tap name>: the TAP file name
- [file mask]: the file name mask - if specified, only these files will be exported, not all the files.
- [-convldr]: convert BASIC loader synthax, file names
The parameter -convldr will cause BASIC program conversion to match the tape LOAD synthax.

Examples:
- tapexp dizzy.tap dizzy*			- will export all blocks starting with "dizzy" into tape file named "dizzy.tap".
- tapexp dizzy1.tap dizzy1* -convldr	- will export all blocks starting with "dizzy1" into tape file named "dizzy1.tap", converting the BASIC loader to destination file system synthax (tape, disk, etc)

-----------------------------------------------------------------------------------------------------------------------
tapimp  - Imports the TAP file to disk
- \<.tap name>: the TAP file name
- [file mask]: the file name mask - if specified, only these files will be exported, not all the files.
- [-convldr]: convert BASIC loader synthax, file names
The parameter -convldr will cause BASIC program conversion to match the destination file system LOAD syntax.

-----------------------------------------------------------------------------------------------------------------------
saveboot  - Save boot tracks to file - usefull for CPM disks with booloader
- \<file name>: output file

-----------------------------------------------------------------------------------------------------------------------
loadboot  - Load boot tracks from file - usefull for CPM disks with booloader
- \<file name>: input file

-----------------------------------------------------------------------------------------------------------------------
format - Format a physical disk or a disk image for a certain file system
- \<disk/image> : the disk drive or image to format
- [-t] : the file system format number to use, as found in fsinfo command

-----------------------------------------------------------------------------------------------------------------------
bin2bas  - Put binary to BASIC block, in a REM statement or variable
- \<type>: type of conversion: 'rem' or 'var'
- \<file>: blob file to add
- [name of block]: name of BASIC block, default blob file name
- [address of execution]: address to copy the block to before execution, default 0 - no moving blob to an address, > 0 means the blob will be LDIR-ed to the specified address before execution.

Examples:
- bin2bas rem 1.bin run		- will create a BASIC program block, named "run" containing file "1.bin" in a REM statement
- bin2bas var loader run 32768	- will create a BASIC program block named "run" containing file named "loader", which will be moved to address 32768 and will be executed.

-----------------------------------------------------------------------------------------------------------------------
convldr - Converts a BASIC loader to work with another storage device
- \<.tap name>: destination TAP file name
- \<loader type>: type of loader: TAPE, MICRODRIVE, OPUS, HCDISK, IF1COM, PLUS3, MGT
Will convert the BASIC program to LOAD from a different device. The ones supported are mentioned.
Is usefull for tape to disk conversion, where it also handles file naming (distinct, unique, non-empty names for disk files).

Examples:
- convldr dizzy1.tap HCDISK	- converts current tape file to dizzy1.tap, updating the BASIC program synthax for HC with disk.

-----------------------------------------------------------------------------------------------------------------------
putif1  - Send a file or collection to IF1 trough the COM port
- \<file name/mask>: file mask to select files for sending
- [COM port index]: COMx port to use, default 1
- [baud rate]: baud rate for COM, default is 4800
If one of the blocks is a Program block, the loaded blocks for that block are also sent and the BASIC loader is updated to match IF1 synthax. 
So it automatically does TAP to IF1 uploading.

Examples:
- putif1 Dizzy1 COM1 9600		- uploads file/block named Dizzy1 to COM1 port using baud rate 9600; if Dizzy1 is BASIC program, loaded blocks will also be uploaded

-----------------------------------------------------------------------------------------------------------------------
getif1  - Get a single file from IF1 trough the COM port
- \<file name>: file name for the received file
- [COM port index]: COMx port to use, default 1
- [baud rate]: baud rate for COM, default is 9600

-----------------------------------------------------------------------------------------------------------------------
diskview  - View disk sectors on screen, displayed as hex and ASCII.
- [track]: track index
- [head]: head index
- [sector]: sector index (not ID)

-----------------------------------------------------------------------------------------------------------------------
basimp  - Import a BASIC program from a text file
- \<BASIC file>: BASIC program file to compile
- [file name]: Program file name (default: file name)
- [autorun line]: Autorun line number (default: 0)

Example:
- basimp 1.bas run 10	- will create a Spectrum file named "run" on current disk/tape, from text file 1.bas, setting auto-run for line 10

-----------------------------------------------------------------------------------------------------------------------
screen  - SCREEN$ block processing functions
- \<operation\>: order, blank
- \<argument\>: order: column/cell; blank: line1xcol1xline2xcol2
- \<input file\>: SCREEN$ file on PC read
- \<output file\>: SCREEN$ file on PC write

Examples: 
- screen order column - will order SCREEN$ by columns, to improve compression
- screen blank 0x0x1x1 - will set to 0 the upper left character cell in the SCREEN$, for both pixels and attributes.

-----------------------------------------------------------------------------------------------------------------------
bincut  - PC file section cut, starting at offset, with size length
- \<input file>: input file name
- \<output file>: output file name
- \<offset>: 0 based start offset
- [lenght]: length of block, default: file len - offset

-----------------------------------------------------------------------------------------------------------------------
binpatch  - Patches a PC file, with content of another file, at set offset
- \<input file>: input file name
- \<patch file>: patch content file
- \<offset>: 0 based start offset in input file

-----------------------------------------------------------------------------------------------------------------------
bitmirror - Reverses bits in each byte of PC file; Example: bits 0001 become 1000
- \<input file>: input file name
- \<output file>: output file name

-----------------------------------------------------------------------------------------------------------------------
exit quit  - Exit program

-----------------------------------------------------------------------------------------------------------------------
	Commands can be concatenated using the ":" character. Example: "open -t 1 : ls : close" .
	The same commands can be entered interactively, or on the program command line, making it easier to use it in batch programs:
"hcdisk2.exe open plus3.dsk -t 1 : cat run : exit >> run.bas" - will open the disk plus3.dsk, specifying the type of file system
to be preselected, as the first in the list (1), display the basic program named "run", and then exit. 
Because the output is redirected into the file run.bas, the result is the BASIC program into a text file.
	
## 6. What about the source code?

	The program is written in C++, and it's using the Standard Template Library (STL). I used Visual Studio 2008 for development,
but I made sure a free compiler can also be used, and I tested with Borland C++ 5.5 free compiler, that can be downloaded from here:
http://edn.embarcadero.com/article/20633 .
	Most of the code is written by me, from scratch, but some components are used, as follows:
- SCREEN$ to GIF code is written by M. van der Heide; I had my own implementation, with the added functionality of saving to 
other formats besides GIF, but it used the GDI+ library, which is harder to use with the Borland compiler.
- The disassembler is dz80, Copyright 1996-2002 Mark Incley
- The CQM, TD0 image drivers are based on LIBDSK 1.2.1, Copyright (C) 2001-2,2005  John Elliott \<jce@seasip.demon.co.uk>
- BAS2TAP implementation is written by M. van der Heide, used for the basimp commmand. Code is adapted to accept HC IF1 synthax.
	
	The program is designed with Object Oriented Design in mind. The disk object is decoupled from the file system object,
so that any combination can be used, CPM in DSK image, TRDOS in CQM image, MGT +D on physical disks, etc. The file system is 
abstracted as a collection of allocation blocks, and a file catalog. The file system has attribute flags, to be able to determine
at runtime what features it support: file attributes, disk label, Spectrum specific files, etc. A file object contains a list of file system blocks 
that it occupies, a list of catalog entries that is occupies, a file name, attributes, length. The file system object is a factory for file objects. 
Files can be raw files, or Sinclair Spectrum specific files. The base class for file system is file archive, used for file collections that are not 
disks, like the tape images, SCL images, or in the future, file archive (ZIP, etc).
	The code is not as clean as I would like. It is gathered from parts I wrote across several years.
	The license is GPLv3, which in my understanding, means that you can use it in your own programs, as long as you specify 
the original author and include the changes you added to the code, when you release your code.

## 7. What other features are planned?
- Add support for the FAT file system, for DOS disk images.
- Add file system configuration in external config. file, to be able to add varations of a file system without recompiling
- Add write support for the current read-only file systems, if usefull.
- Audio tape signal digitisation.
- COM port transfer speed improvement using 19200 baud for HC computers, from 4800/9600 baud currently supported.
- Create a GUI using MFC or QT.

## 8. What are the system requirements?
- For best compatibility for floppy disk access, an older PC is recomended, with a floppy controller on the motherboard. These were produced up until about 2003-2004. 
- The operating system is at minimum Windows XP, but any later Windows version will work.
- USB floppy drives can be used for CP/M disks. Other file systems and disk geometries don't work due to hardware limitations of USB floppy drives and due to Windows floppy driver limitations.
- For COM port/RS232 usage, an on board controller works (but is not usually found on today's computers) or an USB to serial controller also works.
- For data transfer via audio, from the PC to the Spectrum, the usual 3.5 audio jack output works fine, using a jack to DIN5 cable. For recording (not yet implemented in HCDisk), 
an audio splitter can be used to capture Spectrum output using the microphone input of PC via the same audio jack, for computers with mic input on the headphones jack, using Audacity software for example.
The resulting WAV file can be digitized using MakeTZX from Ramsoft or using audio2tape.exe from the fuse-utils package.


## Class Diagram
![ScreenShot](https://raw.githubusercontent.com/0sAND1s/HCDisk/master/HCDisk_ClassDiagram.png)

Example class usage:
1. Accessing ZXSpectrum+3 floppy disks on PC floppy drive. CDiskBase is instantiated with CDiskFloppy class. CFileSystem is ZXSpectrum+3 BASIC. CFile class is instantiated with CFileSpectrum.
2. Accessing CP/M disks in DSK disk image files. CDiskBase is instantiated with CDiskImage. CFileSystem is CPM.
3. Transferring tape image blocks to a HC BASIC file system. CFileSystem is HCBasic. CFileArchiveTape is CTapImage. Tape blocks are read as CFileSpectrumTape and written to the file system as CFileSpectrum using class CFileArchiveTape.
