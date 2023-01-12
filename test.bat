@rem for %%d in (testfiles\*) do hcdisk2 open "%%d" : dir : exit
REM CPM HC on 5.25 disk, CQM image format, shows a text file.
cls
hcdisk2 open testfiles\hccpm525.cqm -t 1 : dir : cat hello.pas : exit
pause

REM Spectrum +3 BASIC on 2.5 disk, EDSK image format, shows a decoded Program file
cls
hcdisk2 open testfiles\+3BASIC.DSK -t 1 : dir : cat CATUSER : exit
pause

REM HC2000 CP/M on 3.5 disk, EDSK image format, shows a text file
cls
hcdisk2 open testfiles\CPM22-HC.DSK -t 1 : dir : cat hello.pas : exit
pause

REM CIP-04 BASIC on 3.5 disk, RAW image format, shows a Program file
cls
hcdisk2 open testfiles\CIP04.IMG -t 5 : dir : cat PRIMU : exit
pause

REM COBRA CP/M on 3.5 disk, RAW image format, shows a text file
cls
hcdisk2 open testfiles\COBRACPM.IMG -t 1 : dir : cat READ.ME : exit
pause

REM HC BASIC on 3.5 disk, RAW image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\HCBASIC.IMG -t 1 : dir : cat bignose.$ : exit
pause

REM MGT BASIC on 3.5 disk, RAW image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\MGT.mgt -t 1 : dir : cat scr : exit
pause

REM Opus BASIC on 3.5 disk, RAW image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\opus.opd -t 1 : dir : cat solscr : exit
pause

REM TRDOS BASIC on SCL image, SCL image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\trdos.scl -t 1 : dir : cat amp3.s : exit
pause

REM Spectrum BASIC on TAPE image, TAP image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\tap.tap -t 1 : dir : cat pots$ : exit
pause

REM Cobra DEVIL BASIC on 3.5 disk, TD0 image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\cobradevil.td0 -t 2 : dir : cat PSP : exit
pause

REM TRDOS BASIC on 3.5 disk, TRD image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\trdos.trd -t 1 : dir : cat bleep.C : exit
pause

REM BASIC tape with turbo loading and headerless blocks, TZX image format, shows a SCREEN$ file
cls
hcdisk2 open testfiles\Castlemaster1.tzx -t 1 : dir : cat CASTLE3 : exit
pause

REM BASIC tape with SPEEDLOCK loading, TZX image format
REM *****issue on command line batch commands ****
cls
hcdisk2 open "testfiles\Highway Encounter.tzx" -t 1 : dir : exit
pause

REM BASIC tape with standard loading, TZX image format, shows a PROGRAM file
cls
hcdisk2 open "testfiles\tzx.tzx" -t 1 : dir : cat "Copy86m" : exit
pause