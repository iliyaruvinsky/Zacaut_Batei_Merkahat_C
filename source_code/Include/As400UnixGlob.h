/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*			   As400UnixGlob.h                        */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	Nov 1996					  */
/*  Written by: Gilad Haimov ( reshuma )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :							  */
/*         Header file types, definitions & constants for	  */
/*         both client & server AS400<->Unix processes.		  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

#if !defined( _AS400_UNIX_GLOBAL_MODULE__ )
#define _AS400_UNIX_GLOBAL_MODULE__

/* << -------------------------------------------------------- >> */
/* 		          #include files                          */
/* << -------------------------------------------------------- >> */
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>


/* << -------------------------------------------------------- >> */
/*  		       Communication messages                     */
/* << -------------------------------------------------------- >> */
#define CM_REQUESTING_COMMIT			9901 
#define CM_CONFIRMING_COMMIT			9902 
#define CM_REQUESTING_ROLLBACK			9903 
#define CM_CONFIRMING_ROLLBACK			9904 
#define CM_SYSTEM_GOING_DOWN			9905 
#define CM_SYSTEM_IS_BUSY				9906 
#define CM_COMMAND_EXECUTION_FAILED		9909 
#define CM_SYSTEM_IS_NOT_BUSY			9910 
#define CM_SQL_ERROR_OCCURED			9912
#define CM_DATA_PARSING_ERR				9913
#define CM_ALL_RETRIES_WERE_USED		9914
#define CM_END_OF_MEM_TABLE				9915
#define CM_OVERWRITE_MEM_TABLE			9916
#define CM_APPEND_MEM_TABLE				9917

// DonR December 2002: added discrepancy-trapping.
#define CM_REQ_COMMIT_N_ROWS			9918
#define CM_CONF_COMMIT_N_ROWS			9919
#define CM_COMMIT_N_ROWS_DISCREP		9920
#define CM_SUPPLYING_TABLE_SIZE_DATA	9921

/* << -------------------------------------------------------- >> */
/*  		AS400<->Unix listen-socket port no.               */
/* << -------------------------------------------------------- >> */
//20021105 Yulia
/*#ifdef DOCTORS_SR2
 int    PORT_IN_AS400 = 9229;
 int    AS400_UNIX_PORT = 9101;
#else
  #ifdef  DOCTORS_SRC
    int    PORT_IN_AS400 = 9129;
    int    AS400_UNIX_PORT = 9101;
  #else
    int    PORT_IN_AS400 = 9029;
    int    AS400_UNIX_PORT = 9001;
  #endif	
#endif
*/
#ifdef DOCTORS_SRC
//const int    PORT_IN_AS400 = 9129;
//const int    AS400_UNIX_PORT = 9101;
 int    PORT_IN_AS400 = 9129;
 int    AS400_UNIX_PORT = 9101;
 int    AS400_UNIX2_PORT = 9102; //Yulia 20050124

#else
//const int    PORT_IN_AS400 = 9029; 20020828
//const int    AS400_UNIX_PORT = 9001;
int    PORT_IN_AS400 = 9029;
int    AS400_UNIX_PORT = 9001;

#endif

const char * HOSTNAME_OF_SERVER_COMPUTER = "AS400";
//const char * AS400_IP_ADDR = "2.1.1.11";
const char * AS400_IP_ADDR = "as400";
/*=======================================================================
||									||
||			    Ascii to Ebcdic section			||
||									||
 =======================================================================*/
typedef  struct tg_AscEbc 
  {
     int ASC, 
	 EBC;
  }
  AscEbc;

/*
 *    All ascii <-> ebcdic pairs
 *
 */
AscEbc  ascii_ebc[] = 
{
    { 0, 0 },
#if 0
    { 1, 0 },
    { 2, 0 },
    { 3, 0 },
    { 4, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 0 },
    { 8, 0 },
    { 9, 0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 0 },
    { 14, 0 },
    { 15, 0 },
    { 16, 0 },
    { 17, 0 },
    { 18, 0 },
    { 19, 0 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 0 },
    { 25, 0 },
    { 26, 0 },
    { 27, 0 },
    { 28, 0 },
    { 29, 0 },
    { 30, 0 },
    { 31, 0 },
#else
    { 1, 64 },
    { 2, 64 },
    { 3, 64 },
    { 4, 64 },
    { 5, 64 },
    { 6, 64 },
    { 7, 64 },
    { 8, 64 },
    { 9, 64 },
    { 10, 64 },
    { 11, 64 },
    { 12, 64 },
    { 13, 64 },
    { 14, 64 },
    { 15, 64 },
    { 16, 64 },
    { 17, 64 },
    { 18, 64 },
    { 19, 64 },
    { 20, 64 },
    { 21, 64 },
    { 22, 64 },
    { 23, 64 },
    { 24, 64 },
    { 25, 64 },
    { 26, 64 },
    { 27, 64 },
    { 28, 64 },
    { 29, 64 },
    { 30, 64 },
    { 31, 64 },
#endif
    { 32, 64 },
    { 33, 90 },
    { 34, 127 },
    { 35, 123 },
    { 36, 91 },
    { 37, 108 },
    { 38, 80 },
    { 39, 125 },
    { 40, 77 },
    { 41, 93 },
    { 42, 92 },
    { 43, 78 },
    { 44, 107 }, /* , */
    { 45, 96 },
    { 46, 75 },
    { 47, 97 },
    { 48, 240 },
    { 49, 241 },
    { 50, 242 },
    { 51, 243 },
    { 52, 244 },
    { 53, 245 },
    { 54, 246 },
    { 55, 247 },
    { 56, 248 },
    { 57, 249 },
    { 58, 122 },
    { 59, 94 },
    { 60, 76 },
    { 61, 126 },
    { 62, 110 },
    { 63, 111 },
    { 64, 124 },
    { 65, 193 },
    { 66, 194 },
    { 67, 195 },
    { 68, 196 },
    { 69, 197 },
    { 70, 198 },
    { 71, 199 },
    { 72, 200 },
    { 73, 201 },
    { 74, 209 },
    { 75, 210 },
    { 76, 211 },
    { 77, 212 },
    { 78, 213 },
    { 79, 214 },
    { 80, 215 },
    { 81, 216 },
    { 82, 217 },
    { 83, 226 },
    { 84, 227 },
    { 85, 228 },
    { 86, 229 },
    { 87, 230 },
    { 88, 231 },
    { 89, 232 },
    { 90, 233 },
    { 91, 186 },
    { 92, 224 },
    { 93, 187 },
    { 94, 176 },
    { 95, 109 },
    { 96, 121 },
    { 97, 129 },
    { 98, 130 },
    { 99, 131 },
    { 100, 132 },
    { 101, 133 },
    { 102, 134 },
    { 103, 135 },
    { 104, 136 },
    { 105, 137 },
    { 106, 145 },
    { 107, 146 },
    { 108, 147 },
    { 109, 148 },
    { 110, 149 },
    { 111, 150 },
    { 112, 151 },
    { 113, 152 },
    { 114, 153 },
    { 115, 162 },
    { 116, 163 },
    { 117, 164 },
    { 118, 165 },
    { 119, 166 },
    { 120, 167 },
    { 121, 168 },
    { 122, 169 },
    { 123, 192 },
//    { 124, 0 },
    { 124, 64 },
    { 125, 208 },
    { 126, 161 },
#if 0
    { 127, 0 },
    { 128, 0 },
    { 129, 0 },
    { 130, 0 },
    { 131, 0 },
    { 132, 0 },
    { 133, 0 },
    { 134, 0 },
    { 135, 0 },
    { 136, 0 },
    { 137, 0 },
    { 138, 0 },
    { 139, 0 },
    { 140, 0 },
    { 141, 0 },
    { 142, 0 },
    { 143, 0 },
    { 144, 0 },
    { 145, 0 },
    { 146, 0 },
    { 147, 0 },
    { 148, 0 },
    { 149, 0 },
    { 150, 0 },
    { 151, 0 },
    { 152, 0 },
    { 153, 0 },
    { 154, 0 },
    { 155, 0 },
    { 156, 0 },
    { 157, 0 },
    { 158, 0 },
    { 159, 0 },
#else
    { 127, 64 },
    { 128, 64 },
    { 129, 64 },
    { 130, 64 },
    { 131, 64 },
    { 132, 64 },
    { 133, 64 },
    { 134, 64 },
    { 135, 64 },
    { 136, 64 },
    { 137, 64 },
    { 138, 64 },
    { 139, 64 },
    { 140, 64 },
    { 141, 64 },
    { 142, 64 },
    { 143, 64 },
    { 144, 64 },
    { 145, 64 },
    { 146, 64 },
    { 147, 64 },
    { 148, 64 },
    { 149, 64 },
    { 150, 64 },
    { 151, 64 },
    { 152, 64 },
    { 153, 64 },
    { 154, 64 },
    { 155, 64 },
    { 156, 64 },
    { 157, 64 },
    { 158, 64 },
    { 159, 64 },
#endif
    { 160, 65 },
    { 161, 170 },
    { 162, 74 },
    { 163, 177 },
    { 164, 159 },
    { 165, 178 },
    { 166, 106 },
    { 167, 181 },
    { 168, 189 },
    { 169, 180 },
    { 170, 154 },
    { 171, 138 },
    { 172, 95 },
    { 173, 202 },
    { 174, 175 },
    { 175, 188 },
    { 176, 144 },
    { 177, 143 },
    { 178, 234 },
//    { 179, 0 },
    { 179, 64 },
    { 180, 190 },
    { 181, 160 },
    { 182, 182 },
    { 183, 179 },
    { 184, 157 },
    { 185, 218 },
    { 186, 155 },
    { 187, 139 },
    { 188, 183 },
    { 189, 184 },
    { 190, 185 },
    { 191, 171 },
    { 192, 100 },
    { 193, 101 },
    { 194, 98 },
    { 195, 102 },
    { 196, 99 },
    { 197, 103 },
    { 198, 158 },
    { 199, 104 },
    { 200, 116 },
    { 201, 113 },
    { 202, 114 },
    { 203, 115 },
    { 204, 120 },
    { 205, 117 },
    { 206, 118 },
    { 207, 119 },
    { 208, 172 },
    { 209, 105 },
    { 210, 237 },
    { 211, 238 },
    { 212, 235 },
    { 213, 239 },
    { 214, 236 },
//    { 215, 191 },
    { 215, 64 },
    { 216, 128 },
#if 0
    { 217, 0 },
    { 218, 0 },
    { 219, 0 },
    { 220, 0 },
#else
    { 217, 64 },
    { 218, 64 },
    { 219, 64 },
    { 220, 64 },
#endif
    { 221, 173 },
    { 222, 174 },
    { 223, 89 },
    { 224, 65 }, /* aleph */
    { 225, 66 },
    { 226, 67 },
    { 227, 68 }, /* dalet */
    { 228, 69 },
    { 229, 70 },
    { 230, 71 },
    { 231, 72 },
    { 232, 73 },
    { 233, 81 }, /* yud */
    { 234, 82 },
    { 235, 83 },
    { 236, 84 },
    { 237, 85 },
    { 238, 86 },
    { 239, 87 },
    { 240, 88 },
    { 241, 89 }, /* samech */
    { 242, 98 },
    { 243, 99 },
    { 244, 100 },
    { 245, 101 },
    { 246, 102 },
    { 247, 103 },
    { 248, 104 },
    { 249, 105 }, /* shin */
    { 250, 113 }, /* taf */
#if 0
    { 251, 0 },
    { 252, 0 },
    { 253, 0 },
    { 253, 0 },
    { 254, 0 },
    { 255, 0 }
#else
    { 251, 64 },
    { 252, 64 },
    { 253, 64 },
    { 253, 64 },
    { 254, 64 },
    { 255, 64 }
#endif
};

/* << ---------------------------------------------------------- >>	*/
/*																	*/
/*		    switch ascii string to ebcdic				            */
/*																	*/
/* << ---------------------------------------------------------- >>	*/
void ascii_to_ebcdic_dic (char * buf, int len)
{
	int  i;

	for (i = 0; i < len; i++)
	{
		buf[i] = (unsigned char)ascii_ebc [(unsigned char)buf[i]].EBC;
	}
}

/* << -------------------------------------------------------- >> */
/*		           debug levels				  */
/* << -------------------------------------------------------- >> */
#define LEVEL_1	100
#define LEVEL_2	200



/* << -------------------------------------------------------- >> */
/*		       length of message header                   */
/* << -------------------------------------------------------- >> */
#define MESSAGE_ID_LEN	4


/* << -------------------------------------------------------- >> */
/*  		      User defined data types                     */
/* << -------------------------------------------------------- >> */
typedef int TSocket;

typedef unsigned long TIP_Address;

typedef short int TSystemMsg;

typedef struct timeval TTimeStruct;

typedef struct fd_set Tfd_set;

typedef struct sockaddr_in  TSocketAddress; 	/* client socket address */

// This is now #defined in MsgHndlr.h
#if 0
typedef enum tag_bool
{
  false = 0,
  true
}
bool ;			/* simple boolean typedef */
#endif

// DonR 17Nov2016: Added new value skip_row, for situations where we want to
// skip past a "bad" row (usually because some value is out of range and the
// message string is too long) without aborting the transaction. This prevents
// bad rows from blocking the flow of good rows to AS/400.
typedef enum tag_TSuccessStat
{
  failure	= 0,
  success	= 1,
  skip_row	= 2
}
TSuccessStat ;		/* success/failure toggle type  */


/* << -------------------------------------------------------- >> */
/*				macro's				  */
/* << -------------------------------------------------------- >> */

/*
 * DbgAssert is removed after debug-period:
 */
#define DbgAssert( p )				\
    if( !(p) ) 					\
    {						\
        GerrLogReturn( GerrId,			\
		 "Assertion Failed:  %s",	\
		  #p );				\
    }

/*
 * StaticAssert is never removed:
 */
#define StaticAssert( p ) DbgAssert( p )

/*
 * end og MessageSequance table not yet reached:
 */
#define NOT_END_OF_TABLE( i ) ( MessageSequence[i].ID )

/*
 * Same as above to AS400 message-list:
 */
#define NOT_END_OF_AS400_TABLE( i ) ( MessagesToAs400[i].ID )

/*
 * a shorter way of writig to log file:
 */
#define WRITE_LOG( ID, OP )						\
    GerrLogReturn( GerrId,						\
		 "SQL error in msg " ID " at " OP " ( %d ) %s.",	\
		  GerrStr );

#endif /* _AS400_UNIX_GLOBAL_MODULE__ */


/* << -------------------------------------------------------- >> */
/* 		             E O F                                */
/* << -------------------------------------------------------- >> */
