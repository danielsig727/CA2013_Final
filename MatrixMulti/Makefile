PROJ=matrix_mult

CC=g++

CFLAGS= -Wall -DUNIX -g -DDEBUG
#CFLAGS= -D__M2S__ -L/opt/m2s/lib -lm2s-opencl

LIBS=-lm2s-opencl
CFLAGS+=-m32

LIB_DIRS=/opt/m2s42/lib

$(PROJ): $(PROJ).cpp 
	$(CC) $(CFLAGS) -o $@ $^ $(INC_DIRS:%=-I%) $(LIB_DIRS:%=-L%) $(LIBS) 

.PHONY: clean

clean:
	rm $(PROJ)
