/*=======================================================================
||																		||
||				Semaphores.c											||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Date : 02/05/96														||
|| Written by : Ely Levy ( reshuma )									||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Purpose :															||
||	Library routines for handling semaphores. 							||
||	Semaphores handling should											||
||	be only with this library routines.									||
||																		||
||----------------------------------------------------------------------||
||																		||
|| 			Include files												||
||																		||
 =======================================================================*/

#include "../Include/Global.h"
static char	*GerrSource = __FILE__;

/*=======================================================================
||																		||
||			Private parameters											||
||																		||
||	CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!			||
||																		||
 =======================================================================*/

/*
 **
 *** CHOOSE SEMAPHORES FACILITY
 *** --------------------------
 **
 */

#define	XENIX_SEM		1		/* Xenix semaphores	*/

#define	POSIX_SEM		2		/* Posix semaphores	*/

#define	BSD_SEM			3		/* Bsd semaphores	*/


#define SEMAPHORES		BSD_SEM


/*
 **
 *** STATIC VARIAVLES
 *** ----------------
 **
 */


static	int		SemMode		= 0777;			/* Semaphore file mode	*/

static	char	buffer[512];

static	char	*SemName1	= "mac_sem1";	/* Key name for semaphore       */

/*=======================================================================
||			XENIX SEMAPHORES											||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

static	int sem_num = -1;

/*=======================================================================
||			POSIX SEMAPHORES											||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

static	semaphore_type *sem_p = NULL;

/*=======================================================================
||			BSD SEMAPHORES												||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

static	int sem_num = -1;

static	struct sembuf
	W	= { 0, -1, SEM_UNDO },
	P	= { 0,  1, SEM_UNDO },
	WNB	= { 0, -1, SEM_UNDO | IPC_NOWAIT },
	PNB	= { 0,  1, SEM_UNDO | IPC_NOWAIT };

static	key_t SemKey1 = 0x000abcde;		/* Semaphore key	*/

#endif	/* SEMAPHORES */


static	short	int	sem_busy_flg = 0;


/*=======================================================================
||																		||
||				CreateSemaphore()										||
||																		||
||		Create  sempahore  and return its id							||
||		This function should be called only by Father process			||
||		It also initializes & opens the semaphore for access			||
||																		||
 =======================================================================*/
int	CreateSemaphore (void)
{
	/*=======================================================================
	||																		||
	||				GET SEMAPHORE NAME										||
	||																		||
	 =======================================================================*/
	sprintf (buffer, "%s/%s", NamedpDir, SemName1);

	/*=======================================================================
	||			XENIX SEMAPHORES											||
	 =======================================================================*/
	#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/
	    sem_num = creatsem (buffer, SemMode);

		if (sem_num == -1)
		{
			GerrLogReturn (GerrId,	"Error while creating Xenix semaphore. Error (%d) %s", GerrStr);
			return (SEM_CREAT_ERR);
		}

	/*=======================================================================
	||			POSIX SEMAPHORES											||
	 =======================================================================*/
	#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/
		sem_p = sem_open (buffer, O_CREAT, SemMode, 1);

		if (sem_p == (semaphore_type *)-1)
		{
			GerrLogReturn (GerrId, "Error while creating Posix semaphore. Error (%d) %s", GerrStr);
			return (SEM_CREAT_ERR);
		}
    
	/*=======================================================================
	||			BSD SEMAPHORES												||
	=======================================================================*/
	#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/
		sem_num = semget (SemKey1, 1, (SemMode | IPC_CREAT));

		if (sem_num == -1)
		{
			GerrLogReturn (GerrId, "Error while creating BSD semaphore. Error (%d) %s", GerrStr);
			return (SEM_CREAT_ERR);
		}

		ret = semctl (sem_num, 0, SETVAL, 1);

		if (ret == -1)
		{
			GerrLogReturn (GerrId, "Error while initializing BSD semaphore. Error (%d) %s", GerrStr);
			sem_num = -1;
	
			return (SEM_CREAT_ERR);
		}

	#endif	/* SEMAPHORES	*/

	return (MAC_OK);

}


/*=======================================================================
||									||
||				DeleteSemaphore()			||
||									||
||		Delete a  sempahore by it's identifier			||
||		This function should be called when all processes	||
||		have finished working with and have issued a call	||
||		to CloseSemaphore for that semaphore.			||
||									||
 =======================================================================*/
int	DeleteSemaphore(
			void
			)
{

/*=======================================================================
||									||
||				GET SEMAPHORE NAME			||
||									||
 =======================================================================*/

    sprintf(
	    buffer,
	    "%s/%s",
	    NamedpDir,
	    SemName1
	    );

/*=======================================================================
||			XENIX SEMAPHORES				||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

    unlink( buffer );

    sem_num	= -1;

/*=======================================================================
||			POSIX SEMAPHORES				||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

    if( sem_unlink( buffer ) == -1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't delete semaphore named '%s'..."
		      "Error (%d) %s",
		      buffer,
		      GerrStr
		      );
	return( SEM_DELETE_ERR );
    }
    
    sem_p	= NULL;

/*=======================================================================
||			BSD SEMAPHORES					||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

    ret =
      semctl(
	     sem_num,
	     0,
	     IPC_RMID,
	     0
	     );
    
    if( ret == -1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Error while removing semaphore id '%d'.."
		      "Error ( %d ) %s",
		      sem_num,
		      GerrStr
		      );

	sem_num = -1;
	
	return( SEM_CREAT_ERR );
    }

    sem_num = -1;

    
#endif /* SEMAPHORES */

/*
 * Return ok
 */
    return( MAC_OK );
}

/*=======================================================================
||									||
||				OpenSemaphore()				||
||									||
||		Open a sempahore and return it's id			||
||		This function should be called by son processes		||
||		It accesses an existing pre-created semaphore		||
||									||
 =======================================================================*/
int	OpenSemaphore(
		      void
		      )
{

/*=======================================================================
||									||
||				GET SEMAPHORE NAME			||
||									||
 =======================================================================*/

    sprintf(
	    buffer,
	    "%s/%s",
	    NamedpDir,
	    SemName1
	    );

/*=======================================================================
||			XENIX SEMAPHORES				||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

    /*
     * Check if semaphore already open
     */
    if( sem_num != -1 )
    {
	return( SEM_ALREADY_OPN );
    }

    /*
     * Open semaphore
     */
    sem_num = opensem(
		      buffer
		      );
    /*
     * Test semaphore
     */
    if( sem_num == -1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Error while opening semaphore named '%s'.."
		      "Error ( %d ) %s",
		      buffer,
		      GerrStr
		      );
	return( SEM_OPEN_ERR );
    }

/*=======================================================================
||			POSIX SEMAPHORES				||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

    /*
     * Check if semaphore already open
     */
    if( sem_p != NULL )
    {
	return( SEM_ALREADY_OPN );
    }

    /*
     * Open semaphore
     */
    sem_p = sem_open(
		     buffer,
		     0
		     );

    /*
     * Test semaphore
     */
    if( sem_p == (semaphore_type *)-1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Error while opening semaphore named '%s'.."
		      "Error ( %d ) %s",
		      buffer,
		      GerrStr
		      );
	return( SEM_OPEN_ERR );
    }

/*=======================================================================
||			BSD SEMAPHORES					||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

    sem_num =
      semget(
	     SemKey1,
	     1,
	     IPC_CREAT // 0 Changed by GAL 5/01/05 Linux
	     );

    if( sem_num == -1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Error while opening semaphore .."
		      "Error ( %d ) %s",
		      GerrStr
		      );
	return( SEM_CREAT_ERR );
    }

#endif	/* OLD_SEMAPHORES  --  XENIX specific enhancement */

/*
 * Return ok
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||				CloseSemaphore()			||
||									||
||		Close a  sempahore by it's  identifier			||
||		This function should be called when process finished	||
||		working with this semaphore.				||
||									||
 =======================================================================*/
int	CloseSemaphore(
		       void
		       )
{
		
/*=======================================================================
||			XENIX SEMAPHORES				||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    {
	return( SEM_NOT_OPEN );
    }

    sem_num	= -1;

/*=======================================================================
||			POSIX SEMAPHORES				||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_p == NULL )
    {
	return( SEM_NOT_OPEN );
    }

    /*
     * Close semaphore
     */
    if( sem_close( sem_p ) == -1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't close semaphore ..."
		      "Error (%d) %s",
		      GerrStr
		      );

	sem_p	= NULL;
	return( SEM_CLOSE_ERR );
    }

    sem_p	= NULL;

/*=======================================================================
||			BSD SEMAPHORES					||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    {
	return( SEM_NOT_OPEN );
    }

    sem_num	= -1;

#endif /* OLD_SEMAPHORES */

/*
 * Return ok
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||				WaitForResource()			||
||									||
||	Wait until the requested resource is free and hold that		||
||	resource to prevent concurrent use on that resource.		||
||									||
 =======================================================================*/
int	WaitForResource(
			void
			)
{

/*=======================================================================
||									||
||		WAIT ON SEMAPHORE UNTIL IT'S FREE, THEN HOLD IT		||
||									||
 =======================================================================*/
#if 0
  printf(
	 "waiting for resource pid -> '%d'..\n",
	 getpid()
	 );
  fflush(stdout);
#endif
  
/*
 * Make wait pushable
 */
    if( sem_busy_flg )
    {
	sem_busy_flg++;
	
	return( MAC_OK );
    }

/*=======================================================================
||			XENIX SEMAPHORES				||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    { 
	return( SEM_NOT_OPEN );
    }

    if(

#ifdef NONBLOCKINGWAIT			/* non - blocking operation	*/
	nbwaitsem( sem_num )
#else	/* NONBLOCKINGWAIT */
	waitsem( sem_num )		/* blocking operation		*/
#endif	/* NONBLOCKINGWAIT */

/*=======================================================================
||			POSIX SEMAPHORES				||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_p == NULL )
    {
	return( SEM_NOT_OPEN );
    }

    if(

#ifdef NONBLOCKINGWAIT			/* non - blocking operation	*/
	sem_trywait( sem_p )
#else	/* NONBLOCKINGWAIT */
	sem_wait( sem_p )		/* blocking operation		*/
#endif	/* NONBLOCKINGWAIT */

/*=======================================================================
||			BSD SEMAPHORES					||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    { 
	return( SEM_NOT_OPEN );
    }
#if 0
  printf(
	 "actual waiting for resource pid -> '%d'..\n",
	 getpid()
	 );
  fflush(stdout);
#endif

    if(

#ifdef NONBLOCKINGWAIT			/* non - blocking operation	*/
	semop(
	      sem_num,
	      &WNB,
	      1
	      )
#else	/* NONBLOCKINGWAIT */		/* blocking operation		*/
	semop(
	      sem_num,
	      &W,
	      1
	      )
#endif	/* NONBLOCKINGWAIT */

#endif	/* SEMAPHORES */

	== -1
	
	)
    {
	GerrLogReturn(
		      GerrId,
		      "Error while waiting on semaphore.."
		      "Error ( %d ) %s",
		      GerrStr
		      );
	return( SEM_WAIT_ERR );
    }

/*
 * return -  occupy semaphore
 * --------------------------
 */
    sem_busy_flg	= 1;

    return( MAC_OK );

}

/*=======================================================================
||									||
||				ReleaseResource()			||
||									||
||	Release the requested resource for use by other processes.	||
||									||
 =======================================================================*/
int	ReleaseResource(
			void
			)
{
#if 0
  printf(
	 "releasing resource pid -> '%d'..\n",
	 getpid()
	 );
  fflush(stdout);
#endif
  
/*
 * Make waiting pushable
 */
    if( ! sem_busy_flg )
    {
	return( SEM_RELISED );
    }
    
    if ( 1 < sem_busy_flg )
    {
	sem_busy_flg--;
	
	return( MAC_OK );
    }

/*=======================================================================
||			XENIX SEMAPHORES				||
 =======================================================================*/
#if SEMAPHORES == XENIX_SEM		/* XENIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    {
	return( SEM_NOT_OPEN );
    }

    if(
       sigsem( sem_num )

/*=======================================================================
||			POSIX SEMAPHORES				||
 =======================================================================*/
#elif	SEMAPHORES == POSIX_SEM		/* POSIX semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_p == NULL )
    {
	return( SEM_NOT_OPEN );
    }

    if(
       sem_post( sem_p )

/*=======================================================================
||			BSD SEMAPHORES					||
 =======================================================================*/
#elif	SEMAPHORES == BSD_SEM		/* BSD semaphores		*/

    /*
     * Check if semaphore is open
     */
    if( sem_num == -1 )
    {
	return( SEM_NOT_OPEN );
    }
#if 0
  printf(
	 "actual release for resource pid -> '%d'..\n",
	 getpid()
	 );
  fflush(stdout);
#endif

    if(
       semop(
	     sem_num,
	     &P,
	     1
	     )

#endif	/* SEMAPHORES */

       == -1

       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't release semaphore.."
		      "Error ( %d ) %s",
		      GerrStr
		      );
	return( SEM_RELIS_ERR );
    }

/*
 * return -  release semaphore
 * ---------------------------
 */
    sem_busy_flg	= 0;

    return( MAC_OK );

}
