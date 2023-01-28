CC=cl.exe
CCFLAGS=/nologo /EHsc /W4 /WX /MD /Zi /DUNICODE=1 /D_UNICODE=1 /DWINVER=_WIN32_WINNT_WIN10 
all: command-proxy.exe

command-proxy.exe: command-proxy.cpp
	$(CC) $(CCFLAGS) $?
clean:
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"
