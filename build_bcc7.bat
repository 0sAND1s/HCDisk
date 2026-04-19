@REM Compile project with Borland C++ 7.30 free compiler (C++ 11). It should be available in the PATH, as specified in the installation instructions.
cls

sjasmplus fileconverters/sna2tapldr.asm --raw=fileconverters/sna2tapldr.bin --lst=fileconverters/sna2tapldr.lst
bin2h.exe sna2tapldr < fileconverters\sna2tapldr.bin > fileconverters\sna2tapldr.h

bcc32c -w- -D_DZ80_EXCLUDE_SCRIPT -ehcdisk2b.exe main.cpp CFile.cpp CFileCPM.cpp CFilePlus3.cpp CFileHC.cpp CFileSpectrum.cpp CFileTRD.cpp CFSCPM.cpp CFSHCBASIC.cpp CFSTRD.cpp DiskBase.cpp DiskImgRaw.cpp DiskWin32LowLevel.cpp DiskWin32Native.cpp dsk.cpp edsk.cpp CFileSystem.cpp CFSTRDSCL.cpp CFSCobraDevil.cpp FileConverters\BasicDecoder.cpp FileConverters\dz80\dissz80.c FileConverters\dz80\noscript.c FileConverters\dz80\tables.c CFileArchive.cpp CFileArchiveTape.cpp Tape\tap.cpp Tape\tapeblock.cpp Tape\TZX.cpp Tape\Wave.cpp Tape\Tape2Wave.cpp CFSBASICPlus3.cpp DiskImgCQM.cpp CFSOpus.cpp CFSMGT.cpp DiskImgTD0.cpp CRC.cpp td0_lzss.cpp FileConverters\scr2gif.cpp FileUtil.cpp BASICUtil.cpp TapeUtil.cpp GUIUtil.cpp COMIF1.cpp FileConverters\bas2tap.c Snapshot\CSnapshotSNA.cpp Snapshot\CSnapshotZ80.cpp FileConverters\Screen.cpp sna2tap.cpp Compression\Compression.cpp Compression\compress.c Compression\memory.c Compression\optimize.c
del *.obj;*.tds

move /Y hcdisk2b.exe Release\hcdisk2b.exe