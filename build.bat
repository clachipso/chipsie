::call vcvarsall.bat x86_amd64

::cl main.cpp Networking.cpp Command.cpp sqlite3.c /Z7 /Od /W3 /EHsc^
:: /link /DEBUG ws2_32.lib /out:chipsie.exe

::cl main.cpp Networking.cpp Command.cpp sqlite3.c /O2 /W3 /EHsc^
:: /link ws2_32.lib /out:chipsie.exe

clang main.cpp Networking.cpp Command.cpp sqlite3.c -O3 -o chipsie.exe^
 -lws2_32
 
del *.obj