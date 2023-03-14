hcdisk2 format dizzyhc.dsk -t 2 -y : exit
for %%f in (*crack.tap) do hcdisk2 open dizzyhc.dsk : tapimp "%%f" * -convldr : exit
hcdisk2 open dizzyhc.dsk : dir : exit