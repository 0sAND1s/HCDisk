@REM Compile project with Borland C++ 7.30 free compiler (C++ 11). It should be available in the PATH, as specified in the installation instructions.
cls

Utils\sjasmplus fileconverters/sna2tapldr.asm --raw=fileconverters/sna2tapldr.bin --lst=fileconverters/sna2tapldr.lst
Utils\bin2h.exe sna2tapldr < fileconverters\sna2tapldr.bin > fileconverters\sna2tapldr.h

bcc32c -w- -D_DZ80_EXCLUDE_SCRIPT -ehcdisk2b.exe main.cpp FileSystem/CFile.cpp FileSystem/CFileCPM.cpp FileSystem/CFilePlus3.cpp FileSystem/CFileHC.cpp FileSystem/CFileSpectrum.cpp FileSystem/CFileTRD.cpp FileSystem/CFSCPM.cpp FileSystem/CFSHCBASIC.cpp FileSystem/CFSTRD.cpp Disk/DiskBase.cpp Disk/DiskImgRaw.cpp Disk/DiskWin32LowLevel.cpp Disk/DiskWin32Native.cpp Disk/dsk.cpp Disk/edsk.cpp FileSystem/CFileSystem.cpp FileSystem/CFSTRDSCL.cpp FileSystem/CFSCobraDevil.cpp FileConverters/BasicDecoder.cpp FileConverters/dz80/dissz80.c FileConverters/dz80/noscript.c FileConverters/dz80/tables.c FileSystem/CFileArchive.cpp FileSystem/CFileArchiveTape.cpp Tape/tap.cpp Tape/tapeblock.cpp Tape/TZX.cpp Tape/Wave.cpp Tape/Tape2Wave.cpp FileSystem/CFSBASICPlus3.cpp Disk/DiskImgCQM.cpp FileSystem/CFSOpus.cpp FileSystem/CFSMGT.cpp Disk/DiskImgTD0.cpp Disk/CRC.cpp Disk/td0_lzss.cpp FileConverters/scr2gif.cpp Utils/FileUtil.cpp Utils/BASICUtil.cpp Utils/TapeUtil.cpp Utils/GUIUtil.cpp FileSystem/COMIF1.cpp FileConverters/bas2tap.c Snapshot/CSnapshotSNA.cpp Snapshot/CSnapshotZ80.cpp FileConverters/Screen.cpp FileConverters/sna2tap.cpp Compression/Compression.cpp Compression/compress.c Compression/memory.c Compression/optimize.c
del *.obj;*.tds

move /Y hcdisk2b.exe Release\hcdisk2b.exe