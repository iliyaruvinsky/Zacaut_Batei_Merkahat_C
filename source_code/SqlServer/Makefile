# SqlServer.mak

#
include ../Util/Defines.mak
#

#
# Directories
# -----------

#
# Files
# -----
SRCS=	SqlServer.c			SqlHandlers.c	\
        SocketAPI.c			DebugPrint.c 	\
		As400UnixMediator.c	ElectronicPr.c TikrotRPC.c MessageFuncs.c DigitalRx.c

INCS=	$(GEN_INCS)			$(SQL_INCS)			\
		SocketAPI.h			As400UnixMediator.h	\
		$(INC_DIR)/TikrotRPC.h $(INC_DIR)/cJSON.h

OBJS=	SqlServer.o			SqlHandlers.o	 \
		SocketAPI.o			DebugPrint.o	\
		As400UnixMediator.o	ElectronicPr.o TikrotRPC.o MessageFuncs.o DigitalRx.o 

INTERMS=	

TARGET=sqlserver.exe

#
include ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -lcurl
#	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) $(INFORM_LIB_OPTS) 	-lodbc
#	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) $(INFORM_LIB_OPTS) 
	
SqlServer.o			:	$(INCS) SqlServer.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) SqlServer.o		SqlServer.c
SqlHandlers.o		:	$(INCS) SqlHandlers.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) SqlHandlers.o		SqlHandlers.c
ElectronicPr.o		:	$(INCS) ElectronicPr.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) ElectronicPr.o	ElectronicPr.c
DigitalRx.o		:	$(INCS) DigitalRx.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) DigitalRx.o		DigitalRx.c
MessageFuncs.o		:	$(INCS) MessageFuncs.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) MessageFuncs.o	MessageFuncs.c
SocketAPI.o			:	$(INCS)	SocketAPI.c		
	$(CC) $(INC_OPTS) $(COMP_OPTS) SocketAPI.o		SocketAPI.c 
DebugPrint.o		:	$(INCS) DebugPrint.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) DebugPrint.o		DebugPrint.c 
As400UnixMediator.o	:	$(INCS) As400UnixMediator.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) As400UnixMediator.o	As400UnixMediator.c 
TikrotRPC.o		:	$(INCS) TikrotRPC.c
	$(CC) $(INC_OPTS) $(COMP_OPTS) TikrotRPC.o		TikrotRPC.c 

clean: 
	rm $(TARGET)	$(OBJS)		$(INTERMS)
	