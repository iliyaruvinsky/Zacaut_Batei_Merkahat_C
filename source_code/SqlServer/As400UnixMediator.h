 #if ! defined( __AS400_UNIX_MEDIATOR_H__ )
  #define __AS400_UNIX_MEDIATOR_H__

  #define  __TIME_CONTROL__

//#include "AddTypes.h"
#ifndef __PINT__
#define __PINT__
typedef int*			PINT;
#endif

#ifndef __PCHAR__
#define __PCHAR__
typedef char*			PCHAR;
#endif

#ifndef __BOOLEAN__
#define __BOOLEAN__
//typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#endif	// BOOLEAN

  #define MAX_BUF_SIZE 256

  #define SUCCESS_PATTERN  "OK"  
  #define ERR_PATTERN	   '#'
  
  #define DELIMITER   ':'
  #define DELIMITER1  ';'

//  #define SERVER "10.70.1.12" // AS/400
  #define SERVER "AS400" // DonR 07Nov2010: Change from numeric IP address to /etc/hosts lookup.
  #define PORT	  9400        // AS/400

  #define RECV_TIMEOUT 10   // sec. DonR 01Dec2020: Changed from 4 to 10 to diagnose problems in the Test environment.
  #define SEND_TIMEOUT 4   // sec

  #define TRY_TIMEOUT  100 // millisec

  #define LOG_DIR_AS400_UNIX   "/pharm/log/mishur"
  #define LOG_AS400_UNIX       "as400UnixMediator"

  typedef enum
  {

	enFunction1    = 1 ,   // as400EligibilityTest
	enFunction2 // = 2 ,      as400SaveSaleRecord
			
  } FUNCTION_NUM;

  typedef enum
  {

	enActWrite     = 1 ,
	enActSterNo // =  2 : erase corresponding record made before 
			
  } ACTION_TYPE;

  typedef enum
  {
	enMishurProblems = -2		,	
	enCommFailed				, // = -1	
	enOk						, // =  0	
	enHasEligibility =  enOk	,	

  } RETURN_CODE;

  typedef enum
  {
    enAlreadyPurchased			= 7		,
    enPharmacyNotExist			= 8		,
    enNoAppropriateInsurance	= 11	,
	enPay2MonthLate				= 14	,
	enUnknownPharmacy					,	// = 15
	enCalculationError					,	// = 16
	enNoAccordance						,	// = 17
    enNotEligible				= 20	,
	Rejected_TrafficAccident	= 47	,
	IdNumberIndebted			= 50    ,
	PregQtyNotYetUtilized		= 62	,
	PregNoOpenPregCheck					,	// = 63
	PregWaitPeriodNotPassed				,	// = 64
	PregBadMeisharInputData				,	// = 65
	PregZeroBalance						,	// = 66
	PregQuantityProblem					,	// = 67
	PregRejectedOtherReason				,	// = 68
	PregPeriodQuantityAllUsed			,	// = 69
	Approved_WorkAccident		= 88	,
	IdNumberNotFound			= 99

  } MISHUR_ERROR_CODE;

  ////////////////////    As400Unix API      ///////////////////

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
							int   *plPrescId			,
							int   *plIdNumber			,
							PINT  pnIdCode				,
							PINT  pnQuantityPermitted	,
							PINT  pnRequestNum			,
							PINT  pnSumPartakingAgorot	,
							PINT  pnInsurance			); 

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
							int   nActionCode			,
							int   nRetries				);

  static int ParseString(	PCHAR pszString				,
							PINT  plPrescId				,
							PINT  plIdNumber			,
							PINT  pnIdCode				,
							PINT  pnQuantityPermitted	,
							PINT  pnSumPartakingAgorot	,
							PINT  pnRequestNum			,
							PINT  pnInsurance			);
  ////////////////////    As400Unix API      ///////////////////

 #if defined( __TIME_CONTROL__ )	
   long DiffTime( struct timeval tvBegin , struct timeval tvEnd );
 #endif

 #endif
