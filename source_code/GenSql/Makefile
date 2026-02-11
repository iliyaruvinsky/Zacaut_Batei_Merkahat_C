# GenSql makefile

#
include ../Util/Defines.mak
#

#
# Directories
# -----------

#
# Files
# -----
INCS	=	$(GEN_INCS)			$(SQL_INCS)

SRCS	=	GenSql.c			# DebugPrn.c

#INTERMS	=	GenSql.c	//Marianna

#OBJS	=	$(SRCS:.ec=.o)
OBJS	=	GenSql.o			# DebugPrn.o

TARGET	=	libgensql.a

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


GenSql.o		: 	GenSql.c	
	$(CC) $(INC_OPTS)  $(COMP_OPTS)	GenSql.o 	GenSql.c 	-lodbc -D _GNU_SOURCE
#DebugPrn.o	:	DebugPrn.c	
#	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	DebugPrn.o 	DebugPrn.c


clean: 
	rm $(TARGET) $(OBJS) 