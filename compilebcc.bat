@REM Compile project with Borland C++ 5.5 free compiler. It should be available in the PATH, as specified in the installation instructions.
del hcdisk2.exe
bcc32 -w- -D_DZ80_EXCLUDE_SCRIPT -6 -ehcdisk2.exe main.cpp CFile.cpp CFileCPM.cpp CFilePlus3.cpp CFileHC.cpp CFileSpectrum.cpp CFileTRD.cpp CFSCPM.cpp CFSCPMHC.cpp CFSTRD.cpp DiskBase.cpp DiskImgRaw.cpp DiskWin32.cpp dsk.cpp edsk.cpp CFileSystem.cpp CFSTRDSCL.cpp CFSCobraDevil.cpp FileConverters\BasicDecoder.cpp FileConverters\dz80\dissz80.c FileConverters\dz80\noscript.c FileConverters\dz80\tables.c CFileArchive.cpp CFileArchiveTape.cpp Tape\tap.cpp Tape\tapeblock.cpp Tape\TZX.cpp Tape\Wave.cpp Tape\Tape2Wave.cpp CFSCPMPlus3.cpp DiskImgCQM.cpp CFSOpus.cpp CFSMGT.cpp DiskImgTD0.cpp CRC.cpp td0_lzss.cpp
del *.obj;*.tds