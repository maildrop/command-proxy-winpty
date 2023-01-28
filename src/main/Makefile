CC=cl.exe
CCFLAGS=/nologo /EHsc /std:c++14 /W4 /WX /MD /Zi /DUNICODE=1 /D_UNICODE=1 /DWINVER=_WIN32_WINNT_WIN10 
all: command-proxy.exe

command-proxy.exe: command-proxy.cpp
	$(CC) $(CCFLAGS) $?
clean:
	for %i IN (vc140.pdb command-proxy.exe command-proxy.pdb command-proxy.ilk command-proxy.obj) do @if exist "%i" del "%i"
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"
