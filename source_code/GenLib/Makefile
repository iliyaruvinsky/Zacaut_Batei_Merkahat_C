# GenLib makefile

#
include ../Util/Defines.mak
#


#
# Directories
# -----------

#
# Files
# -----

SRCS=	GeneralError.c		Memory.c		\
		Semaphores.c		Sockets.c		\
		SharedMemory.cpp	GxxPersonality.c

OBJS=	GeneralError.o		Memory.o		\
		Semaphores.o		Sockets.o		\
		SharedMemory.o		GxxPersonality.o	

TARGET=libgenlib.a

#
include ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) r $(TARGET) $(OBJS)

GeneralError.o		: 	GeneralError.c 		
	$(CC)	$(COMP_OPTS)	GeneralError.o 	GeneralError.c
Memory.o			:	Memory.c			
	$(CC)	$(COMP_OPTS)	Memory.o 	 	Memory.c
Semaphores.o		: 	Semaphores.c		
	$(CC)	$(COMP_OPTS)	Semaphores.o 	Semaphores.c
Sockets.o			: 	Sockets.c			
	$(CC)	$(COMP_OPTS)	Sockets.o 		Sockets.c
SharedMemory.o		: 	SharedMemory.cpp	
	$(CC)	$(COMP_OPTS)	 SharedMemory.o SharedMemory.cpp
GxxPersonality.o	:	GxxPersonality.c
	$(CC)	$(COMP_OPTS)	GxxPersonality.o GxxPersonality.c
clean: 
	rm $(TARGET) $(OBJS)

