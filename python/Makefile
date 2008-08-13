OPT64 = /users/sransom/64bit
ifeq ($(shell hostname),beef) 
    OPT64 = /opt/64bit
endif
ifeq ($(shell hostname),tofu) 
    OPT64 = /opt/64bit
endif
CFLAGS = -g -O3 -Wall -I$(OPT64)/include
PY_INCLUDE = $(OPT64)/include/python2.5
all:  _possem.so
clean:
	rm -f *~ *.o test_psrfits_0*.fits _possem.so
INSTALL_DIR = ../bin
_possem.so:
	$(CC) -I$(PY_INCLUDE) -o $@ -shared -fPIC -O2 possem_wrap.c
