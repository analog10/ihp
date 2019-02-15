NAME=ihp
IHP_OBJ=ihp.o ihpa.o
INC=ihp.h ihpa.h

CFLAGS+=-std=gnu11 -g -Wall -fPIC

DEPEND = $(SOURCES:.c=.d)

CC       = $(CROSS_COMPILE)gcc
CXX      = $(CROSS_COMPILE)g++
LD       = $(CROSS_COMPILE)ld
AR       = $(CROSS_COMPILE)ar
RANLIB   = $(CROSS_COMPILE)ranlib

ifeq ($(OS),Windows_NT)
	STATIC_LIB=$(NAME).lib
	LIB=$(NAME).dll
else
	STATIC_LIB=lib$(libname).a
	LIB=lib$(libname).so
endif

all: $(STATIC_LIB) $(LIB) ihp_test ihp_fill_test

header_install :
	mkdir -p $(INC_DEST)/$(NAME)
	cp $(INC) $(INC_DEST)/$(NAME)

install :
	mkdir -p $(LIB_DEST)/$(NAME)
	cp $(STATIC_LIB) $(LIB) $(LIB_DEST)/$(NAME)

$(LIB) : $(IHP_OBJ)
	$(LD) $(LDFLAGS) -shared -o $@ $^

$(STATIC_LIB) : $(IHP_OBJ)
	$(AR) rcs $@ $^
	$(RANLIB) $@


ihpa.o : ihpa.c ihpa.h
	$(CC) -c $(CFLAGS) $< -o $@ 

ihp.o : ihp.c ihp.h
	$(CC) -c $(CFLAGS) $< -o $@ 

ihp_fill_test: ihp_fill_test.c ihp.o ihpa.o
	$(CC) $(CFLAGS) $(FORCE_FLAGS) $(LDFLAGS) $< -o $@ ihp.o ihpa.o

ihp_test: ihp_test.c ihp.o ihpa.o
	$(CC) $(CFLAGS) $(FORCE_FLAGS) $(LDFLAGS) $< -o $@ ihp.o ihpa.o

clean:
	rm -f ihp_test ihp_fill_test *.o *.a *.so
