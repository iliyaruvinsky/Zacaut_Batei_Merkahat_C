# As400UnixDocServer.mak

#
include ../Util/Defines.mak
#

#
# Files
# -----

SRCS	=	As400UnixDocServer.c
						
OBJS	=	As400UnixDocServer.o
		
					
TARGET = as400unixdocserver.exe

#
include  ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

As400UnixDocServer.o			:	As400UnixDocServer.c	
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	As400UnixDocServer.o	As400UnixDocServer.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET)	$(OBJS)
