@echo off
set input=dizzy2.tap
if not "%1"=="" set input=%1
echo 10 CLEAR 24574: POKE 23739, 111: LOAD "dizzy2scr"SCREEN$: LOAD "dizzy2code"CODE 24576: POKE 23728, 0 > dizzy2ldr.bas
echo 20 PAUSE NOT PI: IF INKEY$ = "y" THEN POKE 29289,201 >> dizzy2ldr.bas
echo 30 RANDOMIZE USR 24832 >> dizzy2ldr.bas
hcdisk2 open %input% : get "DIZZY3" : get "DIZZY4" : format dizzy2crack.tap : open dizzy2crack.tap : basimp dizzy2ldr.bas DIZZY2 : put "DIZZY3" -n dizzy2scr -t b -s 16384 : put "DIZZY4" -n dizzy2code -t b -s 24576 : dir : exit
del dizzy2ldr.bas "DIZZY3" "DIZZY4"