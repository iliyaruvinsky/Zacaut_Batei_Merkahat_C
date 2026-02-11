include ../Util/Defines.mak

# Files
# ----
SRCS	=	As400UnixClient.c
OBJS	=	As400UnixClient.o
#INTERMS	=	As400UnixClient.c	#Marianna 24.05.2020	
TARGET = as400unixclient.exe

include  ../Util/Rules.mak

# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

As400UnixClient.o			:	As400UnixClient.c	
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	As400UnixClient.o	As400UnixClient.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET) $(OBJS)
	
