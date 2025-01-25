CXX=cl.exe
CXXFLAGS=/nologo /EHsc /std:c++14 /W4 /WX /MD /Zi /DUNICODE=1 /D_UNICODE=1 /DWINVER=_WIN32_WINNT_WIN10 

all_target=command-proxy.exe winpty-cmdinvoker.exe readinput.exe cmd-exe-invoker.exe
clean_target= $(all_target:.exe=.pdb) $(all_target:.exe=.ilk) $(all_target)
command_proxy_OBJS=command-proxy.obj
command_proxy_LIBS=
winpty_cmdinvoker_OBJS=winpty-cmdinvoker.obj
winpty_cmdinvoker_LIBS=
cmd_exe_invoker_OBJS=cmd-exe-invoker.obj
cmd_exe_invoker_LIBS=

all: $(all_target)

.cpp.obj::
	$(CXX) $(CXXFLAGS) /c $<

command-proxy.exe: $(command_proxy_LIBS) $(command_proxy_OBJS)
	$(CXX) $(CXXFLAGS) /Fe:$@ $**
winpty-cmdinvoker.exe: $(winpty_cmdinvoker_LIBS) $(winpty_cmdinvoker_OBJS)
	$(CXX) $(CXXFLAGS) /Fe:$@ $**

readinput.exe: readinput.cpp
	$(CXX) $(CXXFLAGS) /Fe:$@ $**

cmd-exe-invoker.obj: cmd-exe-invoker.cpp wh-parse-option.h

cmd-exe-invoker.exe: $(cmd_exe_invoker_OBJS)
	$(CXX) $(CXXFLAGS) /Fe:$@ $**

clean:
	for %i IN (vc140.pdb $(clean_target) ) do @if exist "%i" del "%i"
	if exist "*.obj" for /F "usebackq delims="  %i IN (`dir /b "*.obj"`) do @del "%~i"
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"
