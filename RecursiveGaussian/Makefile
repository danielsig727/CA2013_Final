PROJ=RecursiveGaussian

CC=g++

CFLAGS= -Wall -DUNIX -g -DDEBUG
#CFLAGS= -D__M2S__ -L/opt/m2s/lib -lm2s-opencl

LIBS=-lm2s-opencl
CFLAGS+=-m32

INC_DIRS=/opt/AMDAPP/include
LIB_DIRS=/opt/m2s42/lib

$(PROJ): $(PROJ).cpp $(PROJ).hpp SDKApplication.cpp SDKApplication.hpp SDKBitMap.cpp SDKBitMap.hpp SDKCommandArgs.cpp SDKCommandArgs.hpp SDKCommon.cpp SDKCommon.hpp SDKFile.cpp SDKFile.hpp SDKThread.cpp SDKThread.hpp
	$(CC) $(CFLAGS) -o $@ $^ $(INC_DIRS:%=-I%) $(LIB_DIRS:%=-L%) $(LIBS) 

.PHONY: clean

clean:
	rm $(PROJ)
