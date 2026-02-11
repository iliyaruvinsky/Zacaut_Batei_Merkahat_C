
  #include <stdio.h> 
  #include <string.h> 
  #include <stdlib.h> 
  #include <unistd.h> 
  #include <sys/time.h> 

  #include "DebugPrint.h"

  #include "SocketAPI.h"

  #include "As400UnixMediator.h"
  #include "Global.h"
  #include "GenSql.h"

  // Define source name
  static char THIS_FILE[] = __FILE__;
  static char *GerrSource = __FILE__;

	// DonR 18Nov2024: Set up logic to stop "normal" sporadic Meishar connection
	// failures from triggering "emergency" error-found emails.
	// DonR 24Aug2025: Make the "error window" a variable as well - and set it to
	// 180 seconds (= 3 minutes). Also add a "last success" timestamp.
	static int	LastConnectionErr_date	= 0;
	static int	LastConnectionErr_time	= 0;
	static int	LastConnectionDate		= 0;
	static int	LastConnectionTime		= 0;
	static int	CommErrorWindow			= 180;

#ifdef _LINUX_
#include <errno.h>

// DonR 04Mar2024: Convert the value to "nap" into milliseconds; previously we were
// multiplying by 100 instead of 1000, and filling up the log with messages.
#define nap(x) usleep(1000*x)
#else
  extern int errno;  
#endif


  int as400EligibilityTest( int	  lPharmNum				, 
							int	  lPrescId				, 
							int	  lIdNumber				,
							int	  nIdCode				,
							int   nService1				, 
							int   nService2				,	 
							int   nService3				, 
							int   LargoCode				,
							int   nQuantityRequested	,	 
							int   nBasicPriceAgorot		,
							int   nRetries				, 
							int	  *plPrescId			,
							int	  *plIdNumber			,
							int	  *pnIdCode				,
							int	  *pnQuantityPermitted	,
							int	  *pnRequestNum			,
							int	  *pnSumPartakingAgorot	,
							int	  *pnInsurance			)
  { // returns 0 if successfull , otherwise error code  ?!?
	
	PCHAR pCur;
	char  szText   [ MAX_BUF_SIZE ];
	char  szSendMsg[ MAX_BUF_SIZE ];
	char  szRecvMsg[ MAX_BUF_SIZE ];
	char  szBuf	   [ MAX_BUF_SIZE ];
	
	int	  lService = 100 * ( nService1 * 100 + nService2 ) + nService3;
	int   nSock , nCount , nLen , nAttempt;
	int   nRet = enCommFailed;

  #if defined( __TIME_CONTROL__ )
	struct timeval tvStart , tvFinish;
	BOOL  bTimeGetError = FALSE;
  #endif

	SetDebugFileName( LOG_DIR_AS400_UNIX , LOG_AS400_UNIX );

  #if defined( __TIME_CONTROL__ )
	if ( gettimeofday( &tvStart , 0 ) != -1 )
	{
		 DebugPrint( "as400EligibilityTest started : %d sec , %d millisec\n" ,
					tvStart.tv_sec , tvStart.tv_usec / 1000 );
	}
	else
	{
		 DebugPrint( "as400EligibilityTest : gettimeofday error #%d : %s\n" ,
							errno , strerror( errno ) );
		 bTimeGetError = TRUE;
	}
  #endif

    DebugPrint( "nFunction1			= %d"	, enFunction1			);
	DebugPrint( "lPharmNum			= %d"	, lPharmNum				);
	DebugPrint( "lPrescId			= %d"	, lPrescId				); 
	DebugPrint( "lIdNumber			= %d"	, lIdNumber				);	
	DebugPrint( "nIdCode			= %d"	, nIdCode				);	
	DebugPrint( "nService1			= %d"	, nService1				);
	DebugPrint( "nService2			= %d"	, nService2				);	 
	DebugPrint( "nService3			= %d"	, nService3				);
	DebugPrint( "lService			= %d"	, lService				);
	DebugPrint( "Largo Code			= %d"	, LargoCode				);
	DebugPrint( "nQuantityRequested	= %d"	, nQuantityRequested	);	 
	DebugPrint( "nBasicPriceAgorot	= %d"	, nBasicPriceAgorot		);
	DebugPrint( "nRetries			= %d"	, nRetries				);

	// DonR 14Jan2015: New format including Largo Code. This entered Production
	// yesterday, 13 January.
	sprintf( szText ,  "%d;%d;%d;%d;%d;%d;%d;%d;%d"	, 
						enFunction1						,	// = 1
						lPharmNum						,
						lPrescId						,
						lIdNumber						,
						nIdCode							,
						lService						,		
						LargoCode						,
						nQuantityRequested				,
						nBasicPriceAgorot				);	
	
	DebugPrint( "szText : \"%s\"" , szText );
    
	nCount = sprintf( szSendMsg , "%d:%s" , strlen( szText ) , szText );
	DebugPrint( "szSendMsg : \"%s\"( %d chars ) " , szSendMsg , nCount );

	for ( nAttempt = 0;  nAttempt < nRetries;  nAttempt ++ )
	{
		DebugPrint( "%d attempt" , nAttempt + 1 );
		
		nSock = SocketCreate();
		if ( nSock > 0 )
		{
			DebugPrint( "Socket ( handle = %d ) created." , nSock );
			if ( SocketConnect( nSock ,SERVER , PORT ) )
			{
				int nBytesSent;
				DebugPrint( "Socket connected to %s ( port %d )" , 
								SERVER , PORT );
	
				nBytesSent = SocketSend( nSock , ( PBYTE ) szSendMsg , 
											nCount , SEND_TIMEOUT );
  				if ( nBytesSent > 0 )
				{
					int nMsgSize , nPortion = 0;
					DebugPrint( "%d bytes sent OK!" , nBytesSent );
				  
					DebugPrint( "sizeof( szBuf ) = %d" , sizeof( szBuf ) );
					memset( szRecvMsg , 0x0 , MAX_BUF_SIZE );
					do
					{
						++ nPortion;
						DebugPrint( "nPortion = %d" , nPortion );

						// DonR 28Sep2025: Should the error message produced by SocketReceive()
						// be changed so that it doesn't include the word "error"? This would
						// prevent some (probably) unneccessary alerts.
						nLen = SocketReceive(  nSock , ( PBYTE )szBuf , 
												MAX_BUF_SIZE , RECV_TIMEOUT );
						if ( nLen > 0 )
						{
					 	   szBuf[ nLen ] = 0;
						   DebugPrint( "Received : \"%s\"" , szBuf );
						   if ( 1 == nPortion )
						   {
						        if ( ( pCur = strchr( szBuf , ERR_PATTERN ) ) != NULL )
								{    // error
									 nRet = atoi( pCur + 1 );
									 DebugPrint( "Error : \"%s\"(%d)" , pCur , nRet );
									 // nRet = MakeReturnCode( nRet );
									 break;
								}
								if ( ( pCur = strchr( szBuf , DELIMITER ) ) != NULL )
								{
                                     strncpy( szRecvMsg , szBuf , pCur - szBuf );
									 szRecvMsg [pCur - szBuf] = (char)0;	// DonR 04May2005
                                     nMsgSize = atoi( szRecvMsg );
									 DebugPrint( "nMsgSize = %d" , nMsgSize );
									 strcpy( szRecvMsg , pCur + 1 );
								}
                                else
								{
								     nRet = enCommFailed;
									 break;
								}
						   } // 1- st portion
						   else
						   {
						        strcat( szRecvMsg , szBuf );  
						   }
						} // Successful receive
						else
						{
							nRet = enCommFailed;
							DebugPrint ("Receive timed out after %d seconds.", RECV_TIMEOUT);

							// DonR 28Jan2024: I'm disabling this log message, since it seems to come up frequently
							// even when there are no serious system problems and there's no point in generating
							// meaningless alarm emails.
//							GerrLogMini (GerrId, "as400EligibilityTest: Receive timeout error after %d seconds.", RECV_TIMEOUT);
							break;
						}
						
						nMsgSize -= nLen;

					  } while ( nMsgSize > 0 );

					  DebugPrint( "After Receive" );

				} // successful send
				else
				{
					GerrLogMini (GerrId, "as400EligibilityTest: Error sending to socket.");
				}
				
				close( nSock ); 
			
			} // connect socket 
			else
			{
				// DonR 09Mar2025: In addition to suppressing "normal" sporadic Meishar connection-failure
				// messages during normal working hours, suppress *all* of these messages between 23:00
				// and 01:00, since the "listener" program is typically down during AS/400 backup-time and
				// connection failures in that case don't represent an actual problem.
				int NowDate = GetDate ();
				int NowTime = GetTime ();
 
				// DonR 28Jan2024: Write a diagnostic to log only if this was the last attempt before giving up.
				// DonR 18Nov2024: Add logic to stop "normal" sporadic Meishar connection failures from triggering
				// "emergency" error-found emails. For now at least, log an error if this is the second connection
				// failure in less than 2 minutes.
				// DonR 24Aug2025: The "error window" is now a variable - and it's currently set to 180 seconds
				// (= 3 minutes). Also, don't log the error if the last successful communication attempt was
				// within the error window. This should suppress error logging unless there is a real problem
				// at a time with at least a reasonable amount of activity - i.e. at least 2 failures and no
				// successes within the error window.
				if (	(nAttempt >= (nRetries - 1))																				&&
						((NowTime > 10000) && (NowTime < 230000))	/* DonR 09Mar2025. */											&&
						(diff_from_then (LastConnectionDate,		LastConnectionTime,		NowDate, NowTime) > CommErrorWindow)	&&
						(diff_from_then (LastConnectionErr_date,	LastConnectionErr_time,	NowDate, NowTime) < CommErrorWindow)		)
				{
					GerrLogMini (GerrId, "as400EligibilityTest error connecting to socket for Meishar after %d attempts.", (nAttempt + 1));
				}

				LastConnectionErr_date = NowDate;
				LastConnectionErr_time = NowTime;
				close( nSock );   
				nap( TRY_TIMEOUT );
				continue;
			}

			// DonR 24Aug2025: If we get here, we *did* manage to connect to Meishar, so zero out the last-error timestamp
			// and set the last-success timestamp.
			LastConnectionErr_date	= LastConnectionErr_time = 0;
			LastConnectionDate		= GetDate ();
			LastConnectionTime		= GetTime ();

			if ( strlen( szRecvMsg ) > 0 )
			{
				 ParseString( szRecvMsg				,
							  plPrescId				,
							  plIdNumber			,
							  pnIdCode				,
							  pnQuantityPermitted	,
							  pnSumPartakingAgorot	,
							  pnRequestNum			,
							  pnInsurance			); 
				 
				 nRet = enHasEligibility;
            
			} 
			break;
		} // create socket
		else
		{
			DebugPrint( "Could not create socket" );
			GerrLogMini (GerrId, "as400EligibilityTest error creating socket for Meishar.");
			nap( TRY_TIMEOUT );
			nRet = enCommFailed;
		}
	} // attempts

  #if defined( __TIME_CONTROL__ )	
	if ( gettimeofday( &tvFinish , 0 ) != -1 )
	{
		 DebugPrint( "as400EligibilityTest finished : %d sec , %d millisec\n" , 
						tvFinish.tv_sec , tvFinish.tv_usec / 1000 );
	}
	else
	{
		DebugPrint( "as400EligibilityTest : gettimeofday error #%d : %s\n" ,
							errno , strerror( errno ) );
		bTimeGetError = TRUE;
	}

    if ( ! bTimeGetError )
	{
		   DebugPrint( "as400EligibilityTest executed %d millisec\n" , 
							DiffTime( tvStart , tvFinish ) );		
	}
  #endif

    DebugPrint( "as400EligibilityTest returns %d" , nRet );

    return nRet;

  }  // as400EligibilityTest



  static BOOL ParseString(	PCHAR pszString				,
							int	  *plPrescId				,
							int	  *plIdNumber			,
							PINT  pnIdCode				,
							PINT  pnQuantityPermitted	,
							PINT  pnSumPartakingAgorot	,
							PINT  pnRequestNum			,
							PINT  pnInsurance			) 
  {
    BOOL bRet = FALSE;
	char szTmp[ MAX_BUF_SIZE ] , szBuf[ MAX_BUF_SIZE ] , *pCursor;
	int  nOffset;

	// DonR 23Dec2014: No change here, but the "Listener" program TCPWRKR is now sending an "info code"
	// instead of the Prescription ID, which was being ignored in any case. For now, this code will be
	// either 0 or 88; but more values may be added in future.
	strcpy( szBuf , pszString );
	if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
	strncpy( szTmp , szBuf , nOffset = pCursor - szBuf );
    szTmp[ nOffset ] = 0x0;	// DonR 04May2005: Moved to before atol() to avoid errors.
   *plPrescId = atol( szTmp );
    DebugPrint( "Info Code: \"%s\"" , szTmp );

    strcpy( szBuf , pCursor + 1 );
    if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
    strncpy( szTmp , szBuf , nOffset = pCursor - szBuf );
	szTmp[ nOffset ] = 0x0;
   *plIdNumber = atol( szTmp );
    DebugPrint( "IdNumber : \"%s\"" , szTmp ); 

    strcpy( szBuf , pCursor + 1 );
	// DebugPrint( "szBuf : \"%s\"" , szBuf ); 
    if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
	nOffset = pCursor - szBuf;
    strncpy( szTmp , szBuf , nOffset );
	szTmp[ nOffset ] = 0x0;
   *pnIdCode = atoi( szTmp );
    DebugPrint( "szTmp : \"%s\"" , szTmp ); 
	DebugPrint( "IdCode : \"%s\"" , szTmp );

    strcpy( szBuf , pCursor + 1 );
    if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
    strncpy( szTmp , szBuf , nOffset = pCursor - szBuf );
	szTmp[ nOffset ] = 0x0;
   *pnQuantityPermitted = atoi( szTmp );
    DebugPrint( "QuantityPermitted : \"%s\"" , szTmp );

    strcpy( szBuf , pCursor + 1 );
    if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
    strncpy( szTmp , szBuf , nOffset = pCursor - szBuf );
	szTmp[ nOffset ] = 0x0;
   *pnSumPartakingAgorot = atoi( szTmp );
    DebugPrint( "Sumpartaking : \"%s\"" , szTmp );
	
    strcpy( szBuf , pCursor + 1 );
    if ( ! ( pCursor = strchr( szBuf , DELIMITER1 ) ) ) 
	{
	         return bRet;
	}
    strncpy( szTmp , szBuf , nOffset = pCursor - szBuf );
	szTmp[ nOffset ] = 0x0;
   *pnRequestNum = atoi( szTmp );
    DebugPrint( "nRequestNum : \"%s\"" , szTmp );

	strcpy( szBuf , pCursor + 1 );
   *pnInsurance = atoi( szBuf );
    DebugPrint( "Insurance : \"%s\"" , szBuf );

    return bRet = TRUE;

  } // ParseString



   int as400SaveSaleRecord(	int	  lPharmNum				, 
							int	  lPrescId				, 
							int	  lIdNumber				,
							int	  nIdCode				,
							int   nService1				, 
							int   nService2				,	 
							int   nService3				, 
							int   LargoCode				,
							int   nQuantity				,	 
							int   nBasicPriceAgorot		,
							int   nSumPartakingAgorot	,
							int	  lEvenDate				,
							int	  lContractor			,
							int	  nRequestDate			,
							int   nRequestNum			,
							int   nActionCode  		    ,
							int   nRetries				)
  {
	PCHAR pCur;
	char  szSendMsg[ MAX_BUF_SIZE ];
	char  szText   [ MAX_BUF_SIZE ];
	char  szBuf	   [ MAX_BUF_SIZE ];
	
	int	  lService = 100 * ( nService1 * 100 + nService2 ) + nService3;
	int   nSock , nCount , nAttempt;
	int   nRet = enCommFailed;	

  #if defined( __TIME_CONTROL__ )
	struct timeval tvStart , tvFinish;
	BOOL   bTimeGetError = FALSE;
  #endif

	SetDebugFileName( LOG_DIR_AS400_UNIX , LOG_AS400_UNIX );

  #if defined( __TIME_CONTROL__ )
	if ( gettimeofday( &tvStart , 0 ) != -1 )
	{
		 DebugPrint( "as400SaveSaleRecord started : %d sec , %d millisec\n" ,
					tvStart.tv_sec , tvStart.tv_usec / 1000 );
	}
	else
	{
		 DebugPrint( "as400SaveSaleRecord : gettimeofday error #%d : %s\n" ,
							errno , strerror( errno ) );
		 bTimeGetError = TRUE;
	}
  #endif

	DebugPrint( "nFunction2			= %d"	, enFunction2			);
	DebugPrint( "lPharmNum			= %d"	, lPharmNum				);
	DebugPrint( "lPrescId			= %d"	, lPrescId				); 
	DebugPrint( "lIdNumber			= %d"	, lIdNumber				);	
	DebugPrint( "nIdCode			= %d"	, nIdCode				);	
	DebugPrint( "nService1			= %d"	, nService1				);
	DebugPrint( "nService2			= %d"	, nService2				);	 
	DebugPrint( "nService3			= %d"	, nService3				);
	DebugPrint( "lService			= %d"	, lService				);
	DebugPrint( "Largo Code			= %d"	, LargoCode				);
	DebugPrint( "nQuantity			= %d"	, nQuantity				);	 
	DebugPrint( "nBasicPriceAgorot	= %d"	, nBasicPriceAgorot		);
	DebugPrint( "nSumPartakingAgorot= %d"	, nSumPartakingAgorot	);
	DebugPrint( "lEvenDate			= %d"	, lEvenDate				);
	DebugPrint( "lContractor		= %d"	, lContractor			);
	DebugPrint( "nRequestDate		= %d"	, nRequestDate			);
	DebugPrint( "nRequestNum		= %d"	, nRequestNum			);
	DebugPrint( "nActionCode		= %d"	, nActionCode			);
	DebugPrint( "nRetries			= %d"	, nRetries				);


	// DonR 14Jan2015: New format including Largo Code. This entered Production
	// yesterday, 13 January.
	sprintf( szText ,  "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d"	, 
						enFunction2											,
						lPharmNum											,
						lPrescId											,
						lIdNumber											,
						nIdCode												,
						lService											,			
						LargoCode											,
						nQuantity											,
						nBasicPriceAgorot									,		
						nSumPartakingAgorot									,
						lEvenDate											,
						lContractor											,
						nRequestDate										,		
						nRequestNum											,
						nActionCode											);	
	
	DebugPrint( "szText : \"%s\"" , szText );
    
	nCount = sprintf( szSendMsg , "%d:%s" , strlen( szText ) , szText );
	DebugPrint( "szSendMsg : \"%s\"" , szSendMsg );

	for ( nAttempt = 0;  nAttempt < nRetries;  nAttempt ++ )
	{
		DebugPrint( "%d attempt" , nAttempt + 1 );
		
		nSock = SocketCreate();
		if ( nSock > 0 )
		{
			DebugPrint( "Socket ( handle = %d ) created." , nSock );

   			if ( SocketConnect( nSock ,SERVER , PORT ) )
			{
				int nBytesSent , nLen;
				DebugPrint( "Socket connected to %s ( port %d )" , 
								SERVER , PORT );
				nBytesSent = SocketSend( nSock , ( PBYTE ) szSendMsg , 
											nCount , SEND_TIMEOUT );
				if ( nBytesSent > 0 )
				{
					int nMsgSize , nPortion = 0;
					DebugPrint( "Before Receive" );
					  
					nLen = SocketReceive( nSock , ( PBYTE ) szBuf  , 
										MAX_BUF_SIZE , RECV_TIMEOUT );
					if ( nLen > 0 )
					{
				 	     szBuf[ nLen ] = 0;
						 DebugPrint( "Received : \"%s\"" , szBuf );
						 if ( pCur = strstr( szBuf , SUCCESS_PATTERN ) )
						 {
							  nRet = enOk;
						 }
						 else
						 {    // error
							  nRet = enMishurProblems;
							  GerrLogMini (GerrId, "as400SaveSaleRecord: Error in socket receive.");
						 }

						// DonR 24Aug2025: If we get here, we *did* manage to connect to Meishar, so zero out the last-error timestamp.
						LastConnectionErr_date	= LastConnectionErr_time = 0;
						LastConnectionDate		= GetDate ();
						LastConnectionTime		= GetTime ();

					} // successful receive
					else
					{
						// DonR 28Jan2024: I'm disabling this log message, since it seems to come up frequently
						// even when there are no serious system problems and there's no point in generating
						// meaningless alarm emails.
//						GerrLogMini (GerrId, "as400SaveSaleRecord: Receive timeout error after %d seconds.", RECV_TIMEOUT);
					}
					
				} // successful send
				else
				{
					GerrLogMini (GerrId, "as400SaveSaleRecord: Error sending to socket.");
				}
				
				close( nSock ); 
				break;

			} // connect socket 
			else
			{
				// DonR 09Mar2025: In addition to suppressing "normal" sporadic Meishar connection-failure
				// messages during normal working hours, suppress *all* of these messages between 23:00
				// and 01:00, since the "listener" program is typically down during AS/400 backup-time and
				// connection failures in that case don't represent an actual problem.
				int NowDate = GetDate ();
				int NowTime = GetTime ();
 
//				DebugPrint( "Could not connect to socket" );
//
				// DonR 28Jan2024: Write a diagnostic to log only if this was the last attempt before giving up.
				// DonR 18Nov2024: Add logic to stop "normal" sporadic Meishar connection failures from triggering
				// "emergency" error-found emails. For now at least, log an error if this is the second connection
				// failure in less than 2 minutes.
				// DonR 24Aug2025: The "error window" is now a variable - and it's currently set to 180 seconds
				// (= 3 minutes). Also, don't log the error if the last successful communication attempt was
				// within the error window. This should suppress error logging unless there is a real problem
				// at a time with at least a reasonable amount of activity - i.e. at least 2 failures and no
				// successes within the error window.
				if (	(nAttempt >= (nRetries - 1))																				&&
						((NowTime > 10000) && (NowTime < 230000))	/* DonR 09Mar2025. */											&&
						(diff_from_then (LastConnectionDate,		LastConnectionTime,		NowDate, NowTime) > CommErrorWindow)	&&
						(diff_from_then (LastConnectionErr_date,	LastConnectionErr_time,	NowDate, NowTime) < CommErrorWindow)		)
				{
					GerrLogMini (GerrId, "as400SaveSaleRecord error connecting to socket for Meishar after %d attempts.", (nAttempt + 1));
				}

				LastConnectionErr_date = NowDate;
				LastConnectionErr_time = NowTime;
			  	close( nSock );   
				nap( TRY_TIMEOUT );
				continue;
			}
		} // create socket
		else
		{
			DebugPrint( "Could not create socket" );
			GerrLogMini (GerrId, "as400EligibilityTest error creating socket for Meishar.");
			nap( TRY_TIMEOUT );
			nRet = enCommFailed;
		}
	} // attempts

  #if defined( __TIME_CONTROL__ )	
	if ( gettimeofday( &tvFinish , 0 ) != -1 )
	{
		 DebugPrint( "as400SaveSaleRecord finished : %d sec , %d millisec\n" , 
						tvFinish.tv_sec , tvFinish.tv_usec / 1000 );
	}
	else
	{
		DebugPrint( "as400SaveSaleRecord : gettimeofday error #%d : %s\n" ,
							errno , strerror( errno ) );
		bTimeGetError = TRUE;
	}

    if ( ! bTimeGetError )
	{
		   DebugPrint( "as400SaveSaleRecord executed %d millisec\n" , 
							DiffTime( tvStart , tvFinish ) );		
	}
  #endif

	DebugPrint( "as400SaveSaleRecord returns %d" , nRet );

	return nRet;

  } // as400SaveSaleRecord
 
// #if defined( __TIME_CONTROL__ )	
// long DiffTime( struct timeval tvBegin , struct timeval tvEnd )
//  {
//	long lDiffSec = tvEnd.tv_sec - tvBegin.tv_sec;
//	long lDiffMilliSec = ( tvEnd.tv_usec - tvBegin.tv_usec ) / 1000;
//
//	return lDiffSec * 1000 + lDiffMilliSec;
//		
//  } // DiffTime
// #endif
  
  // static int MakeReturnCode( CINT nCode )
  // {
  //	int nRet = nCode;
	
  //	switch( nCode )
  //	{
  //	  case	enNoAppropriateInsurance	:
  //	  case	enPay2MonthLate				:
  //	  case	IdNumberIndebted			:
  //		
  //		nRet = enNoEligibility;
  //		break;
  //    
  //	  case  enPharmacyNotExist			:
  //	  case 	enUnknownPharmacy			:
  //	  case  enCalculationError			:
  //	  case	enNoAccordance				:
  //      case	enNotEligible				:
  //      case  IdNumberNotFound			:
  //
  //		nRet = enMishurProblems;   
  //		break;
  //
  //	  default : 
  //		break;
  //	}
  // 
  // } // nMakeReturnCode
  
