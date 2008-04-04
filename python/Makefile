CFLAGS = -g -O3 -Wall -I/users/sransom/64bit/include
PY_INCLUDE = /users/sransom/64bit/include/python2.5
all:  _possem.so
clean:
	rm -f *~ *.o test_psrfits_0*.fits _possem.so
INSTALL_DIR = ../bin
_possem.so:
	$(CC) -I$(PY_INCLUDE) -o $@ -shared -fPIC -O2 possem_wrap.c
