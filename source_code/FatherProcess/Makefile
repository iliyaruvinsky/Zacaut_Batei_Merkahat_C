# FatherProcess.mak

#
include ../Util/Defines.mak
#

#
# Directories
# -----------

#
# Files
# -----
SRCS=	FatherProcess.c

INCS=	$(GEN_INCS)			$(SQL_INCS)

OBJS=	FatherProcess.o

TARGET=FatherProcess.exe

#
include ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET): $(OBJS)
#	$(ESQL) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS)
#	$(CC)  -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) $(INFORM_LIB_OPTS)
	$(CC)  -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc  -D_GNU_SOURCE


FatherProcess.o	: FatherProcess.c 
#	$(ESQL) $(INC_OPTS) $(COMP_OPTS)	FatherProcess.o  FatherProcess.ec
	$(CC) $(INC_OPTS) $(COMP_OPTS) FatherProcess.o		FatherProcess.c -lodbc -D_GNU_SOURCE

clean: 
	rm $(TARGET) $(OBJS)
