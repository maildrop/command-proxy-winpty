CC=cl.exe
CCFLAGS=/nologo /EHsc /std:c++14 /W4 /WX /MD /Zi /DUNICODE=1 /D_UNICODE=1 /DWINVER=_WIN32_WINNT_WIN10 
all_target=command-proxy.exe winpty-cmdinvoker.exe readinput.exe cmd-exe-invoker.exe
clean_target= $(all_target:.exe=.pdb) $(all_target:.exe=.ilk) $(all_target)
all: $(all_target)

command-proxy.exe: command-proxy.cpp
	$(CC) $(CCFLAGS) $?
winpty-cmdinvoker.exe: winpty-cmdinvoker.cpp
	$(CC) $(CCFLAGS) $?
readinput.exe: readinput.cpp
	$(CC) $(CCFLAGS) $?
cmd-exe-invoker.exe: cmd-exe-invoker.cpp wh-parse-option.h
	$(CC) $(CCFLAGS) /Fe:$@ $*.cpp

clean:
	for %i IN (vc140.pdb $(clean_target) ) do @if exist "%i" del "%i"
	if exist "*.obj" for /F "usebackq delims="  %i IN (`dir /b "*.obj"`) do @del "%~i"
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"
