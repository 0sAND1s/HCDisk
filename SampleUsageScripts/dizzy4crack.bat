@echo off
set input=dizzy4.tap
if not "%1"=="" set input=%1
echo 10 CLEAR VAL"24136": POKE VAL"23739",VAL "111": LOAD "dizzy4scr"SCREEN$: LOAD "dizzy4code"CODE: POKE VAL "29326", NOT PI: POKE VAL "65535", VAL "255" > dizzy4ldr.bas
echo 20 PAUSE NOT PI: IF INKEY$ = "y" THEN POKE VAL "29623",VAL "182" >> dizzy4ldr.bas
echo 30 RANDOMIZE USR VAL "24158" >> dizzy4ldr.bas
hcdisk2 open %input% : get Dizzy_43 : get Dizzy_44 : get Dizzy_45 : format dizzy4crack.tap : open dizzy4crack.tap : basimp dizzy4ldr.bas DIZZY4 : put "Dizzy_43" -n dizzy4scr -t b -s 16384 : exit
copy /b Dizzy_44 + Dizzy_45 dizzy4code
hcdisk2 open dizzy4crack.tap : put "dizzy4code" -t b -s 24137 : dir : exit
del dizzy4ldr.bas Dizzy_43 Dizzy_44 Dizzy_45 dizzy4code