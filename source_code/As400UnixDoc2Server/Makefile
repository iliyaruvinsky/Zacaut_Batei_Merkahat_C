# As400UnixDoc2Server.mak

#
include ../Util/Defines.mak
#

#
# Files
# -----

SRCS	=	As400UnixDoc2Server.c
						
OBJS	=	As400UnixDoc2Server.o
		
					
TARGET = as400unixdoc2server.exe

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

As400UnixDoc2Server.o			:	As400UnixDoc2Server.c	
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	As400UnixDoc2Server.o	As400UnixDoc2Server.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET)	$(OBJS)
