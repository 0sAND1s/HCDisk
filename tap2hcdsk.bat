set out="hcdisk.dsk"
hcdisk2.exe format %out% -t 2 : exit
for %%f in (*.tap;*.tzx) do hcdisk2.exe open %out% : tapimp %%f * -convldr : exit
