# PharmWebServer.mak

#
include ../Util/Defines.mak
#

#
# Directories
# -----------

#
# Files
# -----
SRCS=	CSocketConn.cpp			CMessage.cpp 		\
		CPharmLiteProtocol.cpp	PharmWebServer.cpp	\
		DebugPrn.cpp

INCS=	$(GEN_INCS)			CPharmLiteProtocol.h	\
		CConnection.h		CSocketConn.h			\
		CMessage.h 			CProtocol.h				\
		misc.h				Log.h					\
		timer.h				DebugPrn.h


OBJS= $(SRCS:.cpp=.o)

TARGET=PharmWebServer.exe

#
include ../Util/Rules.mak
#

#
# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CCC) -o $(TARGET) $(OBJS) $(LIB_OPTS) -lboost_system -lserved -D_GNU_SOURCE
	
CSocketConn.o		: $(INCS) 	CSocketConn.cpp
	$(CCC) $(INC_OPTS) $(COMP_OPTS)	CSocketConn.o	CSocketConn.cpp 	
CMessage.o			: $(INCS) 	CMessage.cpp
	$(CCC) $(INC_OPTS) $(COMP_OPTS)	CMessage.o		CMessage.cpp
CPharmLiteProtocol.o	: $(INCS) CPharmLiteProtocol.cpp
	$(CCC) $(INC_OPTS) $(COMP_OPTS)	CPharmLiteProtocol.o	CPharmLiteProtocol.cpp
PharmWebServer.o		: $(INCS) PharmWebServer.cpp
	$(CCC) $(INC_OPTS) -std=c++11 $(COMP_OPTS)	PharmWebServer.o	PharmWebServer.cpp
DebugPrn.o				: $(INCS) DebugPrn.cpp
	$(CCC) $(INC_OPTS) $(COMP_OPTS)	DebugPrn.o	DebugPrn.cpp

clean: 
	rm $(TARGET) $(OBJS) 	
