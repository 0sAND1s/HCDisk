@echo off
set input=dizzy3.tap
if not "%1"=="" set input=%1
echo 10 CLEAR 24574: POKE 23739, 111: LOAD "dizzy3scr"SCREEN$: LOAD "dizzy3code"CODE 24576: POKE 23627, 0 > dizzy3ldr.bas
echo 20 PAUSE NOT PI: IF INKEY$ = "y" THEN POKE VAL "63001",VAL "0" >> dizzy3ldr.bas
echo 30 RANDOMIZE USR 24832 >> dizzy3ldr.bas
hcdisk2 open %input% : get "DIZZY 33" : get "DIZZY 34" : format dizzy3crack.tap : open dizzy3crack.tap : basimp dizzy3ldr.bas DIZZY3 : put "DIZZY 33" -n dizzy3scr -t b -s 16384 : put "DIZZY 34" -n dizzy3code -t b -s 24576 : dir : exit
del dizzy3ldr.bas "DIZZY 33" "DIZZY 34"