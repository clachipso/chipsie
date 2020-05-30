call vcvarsall.bat x86_amd64

cl main.cpp Networking.cpp Command.cpp sqlite3.c /O2 /EHsc /link ws2_32.lib /out:chipsy.exe
 
del *.obj