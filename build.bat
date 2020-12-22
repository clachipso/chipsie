call vcvarsall.bat x86_amd64

cl main.cpp ChatProcessing.cpp Database.cpp TwitchConn.cpp sqlite3.c^
 /O2 /W3 /EHsc^
 /link ws2_32.lib /out:chipsie.exe

::clang main.cpp ChatProcessing.cpp Database.cpp TwitchConn.cpp sqlite3.c^
 ::-O3 -o chipsie.exe -lws2_32
 
del *.obj