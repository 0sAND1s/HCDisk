@echo off
hcdisk2 format dizzyplus3.dsk -t 5 -y : exit
for %%f in (*crack.tap) do hcdisk2 open dizzyplus3.dsk -t 1 : tapimp "%%f" * -convldr : exit
hcdisk2 open dizzyplus3.dsk -t 1 : dir : exit