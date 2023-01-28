
CC=cl.exe

all: filter-winpty.exe

filter-winpty.exe: filter-winpty.c lex.yy.c
	$(CC) /nologo /W4 /Zi /MD /DYY_NO_UNISTD_H=1 /Fe:$@ filter-winpty.c lex.yy.c 

clean:
	for %i IN (vc140.pdb filter-winpty.exe filter-winpty.pdb filter-winpty.ilk lex.yy.obj filter-winpty.obj) do @if exist "%i" del "%i"
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"

lex.yy.c: filter-winpty.l
	flex $?
