# As400UnixServer.mak

#
include ../Util/Defines.mak
#

#
# Directories
# -----------
#Y BUILD_DIR=$(ROOT)/As400UnixServer

#
# Files
# -----

SRCS	=	As400UnixServer.c
#y1INCS	=	$(INC_DIR)/As400UnixServer.h	\
#y1			$(INC_DIR)/As400UnixGlob.h	
						
OBJS	=	As400UnixServer.o
		
#INTERMS	=	As400UnixServer.c
					
TARGET = as400unixserver.exe

#
include  ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
#	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) $(INFORM_LIB_OPTS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

As400UnixServer.o			:	As400UnixServer.c	
#	$(ESQL)	$(INC_OPTS)	$(COMP_OPTS)	As400UnixServer.o	As400UnixServer.ec
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	As400UnixServer.o	As400UnixServer.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET)	$(OBJS)
