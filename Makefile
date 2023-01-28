
all:
	cd src\main && $(MAKE) /nologo $@
	cd src\test && $(MAKE) /nologo $@
	cd filter && $(MAKE) /nologo $@
clean:
	cd src\main && $(MAKE) /nologo $@
	cd src\test && $(MAKE) /nologo $@
	cd filter && $(MAKE) /nologo $@
	if exist "*~"  for /F "usebackq delims=" %i IN (`dir /b "*~"`) do @del "%~i"
