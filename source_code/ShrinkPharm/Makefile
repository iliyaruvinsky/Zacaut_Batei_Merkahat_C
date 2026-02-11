include ../Util/Defines.mak

# Files
# ----
SRCS	=	ShrinkPharm.c
OBJS	=	ShrinkPharm.o
#INTERMS	=	ShrinkPharm.c	#Marianna 24.05.2020	
TARGET = ShrinkPharm.exe

include  ../Util/Rules.mak

# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

ShrinkPharm.o			:	ShrinkPharm.c	
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	ShrinkPharm.o	ShrinkPharm.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET) $(OBJS)
	
