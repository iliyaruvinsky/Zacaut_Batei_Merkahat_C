/*=======================================================================
||																		||
||				SharedMemory.c											||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Date : 02/05/96														||
|| Written By : Ely Levy ( reshuma )									||
||																		||
||----------------------------------------------------------------------||
||																		||
||  Purpose:															||
||  --------															||
||  	library routines for shared memory handling.					||
||	Shared memory handling should be done								||
||	only with these library routines.									||
||																		||
||----------------------------------------------------------------------||
||																		||
|| 				Include files											||
||																		||
 =======================================================================*/

// DonR 31Jul2013: To avoid 64-bit compiler "const string to char * deprecated" warnings
// (which seem to come up only for .CPP files), suppress that warning; a more elegant
// solution would be nice, but I haven't been able to find one.
#pragma GCC diagnostic ignored "-Wwrite-strings"

static char	*GerrSource = __FILE__;

#define SHARED

// DonR 25Apr2022: Suppress diagnostics that make the "kuku" file huge if the system runs for a long time.
#define LOG_EVERYTHING	0

#ifdef _LINUX_
	#include "../Include/Global.h"
	#include "../Include/CCGlobal.h"
#else
	#include "Global.h"
	#include "CCGlobal.h"
#endif

#define NON_SQL

#ifdef _LINUX_
	#include "../Include/MsgHndlr.h"
#else
	#include "MsgHndlr.h"
#endif

/*=======================================================================
||									||
||			Private parameters				||
||									||
||	CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!	||
||									||
 =======================================================================*/

static MASTER_SHM_HEADER *master_shm_header	= NULL;

typedef	struct
{
  int	shm_id;
  char	*start_addr;
}
EXTENT_ADDR;

static	EXTENT_ADDR
	*ExtentChain;			/* chain of extent addresses	*/

static	int
	ExtentAlloc;			/* allocated items in ext chain	*/

static	int
	ShmMode	=
	0777;				/* Semaphore file mode		*/

static	sharedmem_key_type
	ShmKey1 =			/* key for shared memory        */
	0x000abcde;

    
static	int
	AttachedExtentCount = 0,	/* Attached shared extents count*/
	ParametersRevision = 0;		/* Parameters revision		*/

#define	CONN_MAX	150		/* max connections to tables	*/

static	TABLE
	ConnTab[CONN_MAX];		/* table of connection to tables*/

static	int
	ConnCount	= 0;		/* Connection global count	*/

extern TABLE_DATA TableTab[];

#define	MAXI( a, b )				\
	( (a) > (b) ? (a) : (b) )
/*
 * Special return -- with release resources
 * ----------------------------------------
 */
#define RETURN(a)	{ReleaseResource();return(a);}

/*
 * Validity check
 * --------------
 */
#define	VALIDITY_CHECK()			\
    if( master_shm_header == NULL )		\
    {						\
	return( SHM_NO_INIT );			\
    }

/*
 * Make null extent pointer
 * ------------------------
 */
#define MAKE_NULL_EXT_PTR( extent_ptr )			\
    extent_ptr.ext_no = -1;

/*
 * Check if null extent pointer
 * ----------------------------
 */
#define IS_NULL_EXT_PTR( extent_ptr )			\
    ( ( extent_ptr ).ext_no == -1 )

/*
 * Get absolute address from extent pointer
 * ----------------------------------------
 */
#define GET_ABS_ADDR( extent_ptr )			\
    (							\
     IS_NULL_EXT_PTR( extent_ptr ) ?			\
      NULL				:		\
      (							\
       ExtentChain[( extent_ptr ).ext_no].start_addr +	\
       ( extent_ptr ).offset				\
       )						\
     )

/*
 * Check table pointer
 * -------------------
 */
#define CHECK_TAB_PTR( tablep )				\
{							\
    TABLE_HEADER	*table_header;			\
							\
    if( tablep == NULL )				\
    {							\
	return( TAB_PTR_INVALID );			\
    }							\
							\
    if( tablep->conn_id < 0 ||				\
	tablep->conn_id >= CONN_MAX )			\
    {							\
	return( TAB_PTR_INVALID );			\
    }							\
							\
    if( ConnTab[tablep->conn_id].curr_addr != tablep )	\
    {							\
	return( TAB_PTR_INVALID );			\
    }							\
							\
    table_header =					\
      (TABLE_HEADER*)GET_ABS_ADDR(			\
				  tablep->table_header	\
				  );			\
							\
    if( table_header->table_data.sort_flg		\
	!= NOT_SORTED &&				\
	table_header->table_data.sort_flg		\
	!= SORT_REGULAR )				\
    {							\
	return( NOT_IMPLEMENTED );			\
    }							\
}

/*
 * Get free extent for requested size
 * if new extent is chosen -- update master
 * ----------------------------------------
 */
#define	GET_FREE_EXTENT(					\
			extent_header,				\
			extent_no,				\
			size					\
			)					\
								\
	for(							\
	    extent_no		= 0,				\
	    extent_header	=				\
	    (EXTENT_HEADER*)GET_ABS_ADDR(			\
	       master_shm_header->first_extent			\
				         );			\
								\
	    extent_header != NULL	&&			\
		extent_header->free_space < size;		\
								\
	    extent_header	=				\
	    (EXTENT_HEADER*)GET_ABS_ADDR(			\
		extent_header->next_extent			\
				         ),			\
	    extent_no++						\
	    );							\
	    							\
	if( extent_header == NULL )				\
	{							\
	    RETURN_ON_ERR(					\
		AddNextExtent(					\
			      MAXI( size, SharedSize )		\
			      )					\
			  );					\
								\
	    extent_no					=	\
	    master_shm_header->free_extent.ext_no	=	\
		master_shm_header->extent_count - 1;		\
								\
	    master_shm_header->free_extent.offset	=	\
		0;						\
								\
	    extent_header	=				\
	      (EXTENT_HEADER*)GET_ABS_ADDR(			\
		 master_shm_header->free_extent			\
					   );			\
	}

#define	OLD_GET_FREE_EXTENT(					\
			extent_header,				\
			size					\
			)					\
	extent_header	=					\
	  (EXTENT_HEADER*)GET_ABS_ADDR(				\
				       master_shm_header->free_extent	\
				       );			\
								\
	if( extent_header->free_space < size )			\
	{							\
	    if( IS_NULL_EXT_PTR( extent_header->next_extent ) )	\
	    {							\
	        int						\
		max = ( size > SharedSize ) ?			\
			  size : SharedSize;			\
								\
		state = AddNextExtent(				\
				      max			\
				      );			\
		if( state != MAC_OK )				\
		{						\
		    RETURN( state );				\
		}						\
	    }							\
	    master_shm_header->free_extent	=		\
	      extent_header->next_extent;			\
								\
	    extent_header	=				\
	      (EXTENT_HEADER*)GET_ABS_ADDR(			\
					   extent_header->next_extent	\
					   );			\
	}

/*
 * Attach all newly allocated extents
 * ----------------------------------
 */
#define ATTACH_ALL()				\
    {						\
        int					\
	ret = AttachAllExtents();		\
						\
	if( ret != MAC_OK )			\
	{					\
	    return( ret );			\
	}					\
    }

/*=======================================================================
||									||
||				InitFirstExtent()			||
||									||
||		Allocate & init first extent in shared memory		||
||		This function is called only by Father Process.		||
||									||
 =======================================================================*/
int	InitFirstExtent(
			size_type shm_size	/* shared memory size	*/
			)
{

/*
 * Local variables
 * ---------------
 */
    int				shm_id,
				i;
    void			*shm_start;
    EXTENT_HEADER		*extent_header;

/*=======================================================================
||									||
||			GET THE SHARED MEMORY.				||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

/*
 * Check if first shared memory
 * extent already exists from
 * last execute of this program
 * -- in that case , remove it
 * ----------------------------
 */
    shm_id = shmget(
		    ShmKey1,
		    shm_size,
		    ShmMode
		    );

    if( shm_id != -1 )
    {
	if(
	   shmctl(
		  shm_id,
		  IPC_RMID,
		  NULL
		  )
	   == -1
	   )
	{
	    GerrLogReturn(
			  GerrId,
			  "Shared memory for id '%d' already"
			  "exists and can't remove it..error (%d) %s",
			  shm_id,
			  GerrStr
			  );
	    RETURN( SHM_REMOVE_ERR );
	}
    }

/*
 * Get new shared memory extent
 * ----------------------------
 */
    shm_id = shmget(
		    ShmKey1,
		    shm_size,
		    ShmMode | IPC_CREAT
		    );

    if( shm_id == -1 )
    {
   	GerrLogReturn(
		      GerrId,
		      "Can't get shared memory segment for id '%d'.."
		      "Error ( %d ) %s",
		      shm_id,
		      GerrStr
		      );
	RETURN( SHM_GET_ERR );
    }

/*
 * Attach shared memory extent
 * ---------------------------
 */
    shm_start = shmat(
		      shm_id,
		      (void *)0,
		      0
		      );

    if( shm_start == (void *)-1 )
    {
  	GerrLogReturn(
		      GerrId,
		      "Can't attach to a shared memory"
		      " with id '%d' ..Error (%d) %s",
		      shm_id,
		      GerrStr
		      );
	RETURN( SHM_ATT_ERR );
    }

/*=======================================================================
||									||
||			INITIALIZE MASTER OF SHARED MEMORY.		||
||									||
 =======================================================================*/

/*
 * Initialize shared memory master header
 * --------------------------------------
 */
    master_shm_header = (MASTER_SHM_HEADER*)shm_start;	/* start address*/

    master_shm_header->extent_count		= 1;	/* extent count	*/

    MAKE_NULL_EXT_PTR(
		      master_shm_header->first_table	/* first table	*/
		      );

    MAKE_NULL_EXT_PTR(
		      master_shm_header->last_table	/* last table	*/
		      );

    master_shm_header->first_extent.ext_no	=	/* first extent	*/
      0;

    master_shm_header->first_extent.offset	=
      0;

    master_shm_header->free_extent		=	/* free extent	*/
      master_shm_header->first_extent;

/*
 * Initialize extent addresses chain
 * ---------------------------------
 */

    ExtentAlloc				=
      40;

    ExtentChain				=
      (EXTENT_ADDR*)MemAllocReturn(
				   GerrId,
				   ExtentAlloc * sizeof(EXTENT_ADDR),
				   "Memory for extent addresses chain"
				   );

    AttachedExtentCount			=
      1;

    ExtentChain[0].shm_id		=
      shm_id;
    
    ExtentChain[0].start_addr		=
      (char*)master_shm_header + sizeof(MASTER_SHM_HEADER);
    
/*
 * Initialize first shared memory extent
 * -------------------------------------
 */
    extent_header			=		/* first ext ptr*/
      (EXTENT_HEADER*)GET_ABS_ADDR(
				   master_shm_header->first_extent
				   );

    extent_header->free_space		=		/* free space	*/
      shm_size -
      (
       sizeof(EXTENT_HEADER)+
       sizeof(MASTER_SHM_HEADER)
       );

    extent_header->extent_size		=		/* extent size	*/
      shm_size;

    extent_header->next_extent_id	=		/* next ext id	*/
      -1;

    extent_header->free_addr_off	=		/* free address	*/
      sizeof(EXTENT_HEADER);

    MAKE_NULL_EXT_PTR(
		      extent_header->back_extent	/* back extent	*/
		      );

    MAKE_NULL_EXT_PTR(
		      extent_header->next_extent	/* next extent	*/
		      );

/*
 * Initialize table connections buffer
 * -----------------------------------
 */
    bzero(
	  (char*)ConnTab,
	  sizeof(ConnTab)
	  );

    for(
	i = 0;
	i < CONN_MAX;
	i++
	)
    {
	ConnTab[i].conn_id		= i;
	ConnTab[i].busy_flg		= MAC_FALS;
	ConnTab[i].curr_addr		= ConnTab + i;
    }
    
/*
 * Release shared memory lock and
 * return shared memory start address
 * ----------------------------------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				AddNextExtent()				||
||									||
||		Allocate next extent & init & update rest extents	||
||		This function is called only by Father Process.		||
||									||
 =======================================================================*/
static int	AddNextExtent(
			      size_type	shm_size/* shared memory size	*/
			      )
{

/*
 * Local variables
 * ---------------
 */
    int				shm_id;
    void			*shm_start;
    EXTENT_HEADER		*first_extent_header,
				*extent_header,
				*new_extent_header;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			GET THE SHARED MEMORY.				||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*
 * Get new shared memory extent
 * ----------------------------
 */
    shm_size +=
      sizeof(EXTENT_HEADER);
    
    shm_id =
      shmget(
	     IPC_PRIVATE,
	     shm_size,
	     ShmMode
	     );

    if(
       shm_id == -1
       )
    {
   	GerrLogReturn(
		      GerrId,
		      "Can't get private shared memory segment.."
		      "Error ( %d ) %s",
		      GerrStr
		      );
	RETURN( SHM_GET_ERR );
    }

/*
 * Attach new shared memory extent
 * -------------------------------
 */
    shm_start =
      shmat(
	    shm_id,
	    (void *)0,
	    0
	    );

    if(
       shm_start == (void *)-1
       )
    {
  	GerrLogReturn(
		      GerrId,
		      "Can't attach to a shared memory"
		      " with id '%d' ..Error (%d) %s",
		      shm_id,
		      GerrStr
		      );
	RETURN( SHM_ATT_ERR );
    }

/*=======================================================================
||									||
||			APPEND TO CHAIN OF EXTENTS.			||
||									||
 =======================================================================*/

    /*
     * Go to last extent in chain
     */
    extent_header =
      (EXTENT_HEADER*)ExtentChain[AttachedExtentCount - 1].start_addr;
    
    /*
     * Update global extent count in master header
     */
    master_shm_header->extent_count++;

/*
 * init new extent
 * ---------------
 */
    new_extent_header			=		/* new extent	*/
      (EXTENT_HEADER*)shm_start;

    new_extent_header->free_space	=		/* free space	*/
      shm_size -
      sizeof(EXTENT_HEADER);

    new_extent_header->extent_size	=		/* extent size	*/
      shm_size;

    new_extent_header->next_extent_id	=		/* next ext id	*/
      -1;

    new_extent_header->free_addr_off	=		/* free address	*/
      sizeof(EXTENT_HEADER);

    new_extent_header->back_extent.ext_no	=	/* back extent	*/
      master_shm_header->extent_count - 2;

    new_extent_header->back_extent.offset	=
      0;
    
    MAKE_NULL_EXT_PTR(					/* next extent	*/
		      new_extent_header->next_extent
		      );

    /*
     * Link the chain forward to new extent
     */
    extent_header->next_extent.ext_no		=	/* next extent	*/
      master_shm_header->extent_count - 1;

    extent_header->next_extent.offset		=
      0;

    extent_header->next_extent_id	=		/* next ext id	*/
      shm_id;
    
/*
 * update extent addresses chain
 * -----------------------------
 */

    AttachedExtentCount++;

    if(
       AttachedExtentCount > ExtentAlloc
       )
    {
	ExtentAlloc			+=
	  40;
	
	ExtentChain			=
	  (EXTENT_ADDR*)MemReallocReturn(
					 GerrId,
					 ExtentChain,
					 ExtentAlloc * sizeof(EXTENT_ADDR),
					 "Memory for extent addresses chain"
					 );
    }
    
    ExtentChain[AttachedExtentCount - 1].shm_id =
      shm_id;
    
    ExtentChain[AttachedExtentCount - 1].start_addr =
      (char*)new_extent_header;
    
/*
 * Return ok
 * ---------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				KillAllExtents()			||
||									||
||		Kill all extents when system is going down.		||
||		This function is called only by Father Process.		||
||									||
 =======================================================================*/
int	KillAllExtents(
		       void
		       )
{

/*
 * Local variables
 * ---------------
 */
    int				count,
				kill_len,
				det_len,
				all_killed = MAC_TRUE;
    char			kill_buf[2048],
				det_buf[2048];
    void			*start_address;


/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			KILL ALL EXTENTS				||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*
 * Loop on extents and kill them ( no mercy )
 * ------------------------------------------
 */
    kill_buf[0]			= 0;
    det_buf[0]			= 0;
    kill_len			= 0;
    det_len			= 0;

    for(
	count = 0;
	count < AttachedExtentCount;
	count++
	)
    {
	/*
	 * Get start address of extent
	 */
	if( ! count )
	{
	    start_address	=
	      (void*)master_shm_header;
	}
	else
	{
	    start_address	=
	      (void*)ExtentChain[count].start_addr;
	}

	/*
	 * Detach extent
	 */
	if(
	   shmdt( start_address ) == -1
	   )
	{
	    det_len		+=
	      sprintf(
		      det_buf + det_len,
		      " '%d'",
		      ExtentChain[count].shm_id
		      );
	}

	/*
	 * Kill extent
	 */
	if(
	   shmctl(
		  ExtentChain[count].shm_id,
		  IPC_RMID,
		  NULL
		  )
	    == -1
	   )
	{
	    kill_len		+=
	      sprintf(
		      kill_buf + kill_len,
		      " '%d'",
		      ExtentChain[count].shm_id
		      );
	}

    }

/*
 * Zero extent chain data
 * ----------------------
 */
    master_shm_header		=
      NULL;

    AttachedExtentCount		=
      0;

/*
 * Check for errors
 * ----------------
 */
    if( det_len )
    {
        GerrLogReturn(
		      GerrId,
		      "Can't detach shared memory extents with id : %s"
		      "..Error (%d) %s",
		      det_buf,
		      GerrStr
		      );
    }

    if( kill_len )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't remove shared memory extents with id : %s"
		      "..Error (%d) %s",
		      kill_buf,
		      GerrStr
		      );

	RETURN( SHM_KILL_ERR );
    }
/*
 * Return -- ok
 * ------------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				OpenFirstExtent()			||
||									||
||			Open the first shared extent for access		||
||									||
 =======================================================================*/
int	OpenFirstExtent(
			void
			)
{

/*
 * Local variables
 * ---------------
 */
    int		shm_id,
		i;
    void	*shm_start;

/*=======================================================================
||									||
||			GET THE FIRST SHARED MEMORY ID			||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

/*
 * Get shared memory id
 * --------------------
 */
    shm_id =
      shmget(
	     ShmKey1,
	     0,
	     0
	     );

    if(
       shm_id == -1
       )
    {
  	GerrLogReturn(
		      GerrId,
		      "Can't get shared memory segment id for key '%d'.."
		      "Error ( %d ) %s",
		      ShmKey1,
		      GerrStr
		      );
	RETURN( SHM_GET_ERR );
    }

/*=======================================================================
||									||
||			ATTACH THE FIRST SHARED MEMORY ID		||
||									||
 =======================================================================*/

    shm_start			=
      AttachShMem(
		  shm_id
		  );

    if(
       shm_start == (void*)-1
       )
    {
	RETURN( SHM_ATT_ERR );
    }

    master_shm_header		=
      (MASTER_SHM_HEADER*)shm_start;

/*
 * Initialize table connections buffer
 * -----------------------------------
 */
    bzero(
	  (char*)ConnTab,
	  sizeof(ConnTab)
	  );

    for(
	i = 0;
	i < CONN_MAX;
	i++
	)
    {
	ConnTab[i].conn_id		= i;
	ConnTab[i].busy_flg		= 0;
	ConnTab[i].curr_addr		= ConnTab + i;
    }

/*
 * Initialize extent addresses chain
 * ---------------------------------
 */

    ExtentAlloc				=
      40;

    ExtentChain				=
      (EXTENT_ADDR*)MemAllocReturn(
				   GerrId,
				   ExtentAlloc * sizeof(EXTENT_ADDR),
				   "Memory for extent addresses chain"
				   );

    AttachedExtentCount			=
      1;

    ExtentChain[0].shm_id		=
      shm_id;
    
    ExtentChain[0].start_addr		=
      (char*)master_shm_header + sizeof(MASTER_SHM_HEADER);
    
/*
 * return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				AttachAllExtents()			||
||									||
||		Attach all newly allocated shared extents		||
||									||
 =======================================================================*/
static int	AttachAllExtents(
				 void
				 )
{

/*
 * Local variables
 */
    int				i;
    void			*shm_start;
    EXTENT_HEADER		*extent_header;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			ATTACH ALL NEWLY ALLOCATED EXTENTS		||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 */
    WaitForResource();

/*
 * Return ok -- when syncronized
 */
    if(
       AttachedExtentCount == master_shm_header->extent_count
       )
    {
	RETURN( MAC_OK );
    }

/*
 * Check for first extent attachment
 */
    if(
       ! AttachedExtentCount
       )
    {
	RETURN( NO_FIRST_EXT );
    }

/*
 * Attach all new extents
 */
    for(
	i = AttachedExtentCount - 1;
	i < master_shm_header->extent_count - 1;
	i++
	)
    {
	extent_header		=
	  (EXTENT_HEADER*)ExtentChain[i].start_addr;

	shm_start		=
	  AttachShMem(
		      extent_header->next_extent_id
		      );

        /*
	 * Test attachment
	 */
	if(
	   shm_start == (void*)-1
	   )
	{
	    RETURN( SHM_ATT_ERR );
	}

	/*
	 * Insert into extent addresses chain
	 * ----------------------------------
	 */

	AttachedExtentCount++;

	if(
	   AttachedExtentCount > ExtentAlloc
	   )
	{
	    ExtentAlloc		+=
	      40;

	    ExtentChain		=
	      (EXTENT_ADDR*)MemReallocReturn(
					     GerrId,
					     ExtentChain,
					     ExtentAlloc * sizeof(EXTENT_ADDR),
					     "Memory for extent"
					     " addresses chain"
					     );
	}

	ExtentChain[AttachedExtentCount - 1].shm_id =
	  extent_header->next_extent_id;

	ExtentChain[AttachedExtentCount - 1].start_addr =
	  (char*)shm_start;
    }
    
/*
 *  Release resource & return ok
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				DetachAllExtents()			||
||									||
||			Detach all attached extents			||
||									||
 =======================================================================*/
int	DetachAllExtents(
			 void
			 )
{

/*
 * Local variables
 */
    int				i;
    void			*start_address;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			DETACH ALL EXTENTS				||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 */
    WaitForResource();

/*
 * Loop on attached extents
 */

    buf[0]		=
      0;
    
    len			=
      0;

    for(
	i = 0;
	i < AttachedExtentCount;
	i++
	)
    {
        /*
	 * Get start address of extent
	 */
	if( i )
	{
	    start_address	=
	      (void*)ExtentChain[i].start_addr;
	}
	
	else
	{
	    start_address	=
	      (void*)master_shm_header;
	}

        /*
	 * Detach extent
	 */
	if(
	   shmdt( start_address ) == -1
	   )
	{
	    len	+=
	      sprintf(
		      buf + len,
		      " '%d'",
		      ExtentChain[i].shm_id
		      );
	}
    }


/*
 * Zero extents data
 */
    master_shm_header	= NULL;

    AttachedExtentCount	= 0;

/*
 * Check for errors
 * ----------------
 */
    if( len )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't detach shared memory segments with id : %s.."
		      "Error ( %d ) %s",
		      buf,
		      GerrStr
		      );
	RETURN( SHM_DET_ERR );
    }

/*
 * Return -- ok
 * ------------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				AttachShMem()				||
||									||
||			Attach to a shared memory segment		||
||									||
 =======================================================================*/
static void	*AttachShMem(
			     int shm_id		/* shared memory id	*/
			     )
{

/*
 * Local variables
 * ---------------
 */
    void	*shm_start;

/*=======================================================================
||									||
||			ATTACH THE SHARED MEMORY			||
||									||
 =======================================================================*/

    shm_start =
      shmat(
	    shm_id,
	    (void *)0,
	    0
	    );

    if(
       shm_start == (void *)-1
       )
    {
  	GerrLogReturn(
		      GerrId,
		      "Can't attach to a shared memory"
		      " with id '%d' ..Error (%d) %s",
		      shm_id,
		      GerrStr
		      );
    }

/*
 * return shared memory start address
 * ----------------------------------
 */
    return( shm_start );

}

/*=======================================================================
||									||
||				CreateTable()				||
||									||
||			Create table in shared memory			||
||		This function should be called only by Father process	||
||									||
 =======================================================================*/
int	CreateTable(
		    const char	*table_name,	/* table name		*/
		    TABLE	**tablep	/* table pointer	*/
		    )
{

/*
 * Local variables
 */
    TABLE_DATA			*table_data;	/* table data		*/
    EXTENT_HEADER		*extent_header;	/* extent header	*/
    TABLE_HEADER		*table_header,	/* table header		*/
				temp,
				*tmp_table_header;/* temporary table hdr*/
    EXTENT_PTR			extent_ptr;	/* extent pointer	*/
    int				table_no;
    int				extent_no;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    ATTACH_ALL();

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    table_no = GetTabNumByName( table_name );

    if( table_no < 0 )
    {
	RETURN( TAB_NAM_ERR );
    }
    
    table_data =
      TableTab + table_no;
    
    if( table_data->record_size < 1 )
    {
	RETURN( DATA_LEN_ERR );
    }

    if(
       table_data->sort_flg != NOT_SORTED		&&
       table_data->row_comp == NULL
       )
    {
	RETURN( COMP_FUN_ERR );
    }

/*=======================================================================
||									||
||			GET FREE EXTENT FOR REQUESTED TABLE		||
||									||
 =======================================================================*/

/*
 * Check if table name already exists in memory
 */
    
    for(
	table_header =
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      master_shm_header->first_table
				      );
	table_header != NULL;
	table_header =
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      table_header->next_table
				      )
	)
    {
	if(
	   !strcmp(
		   table_header->table_data.table_name,
		   table_data->table_name
		   )
	   )
	{
	    RETURN( SHM_TAB_EXISTS );
	}
    }

/*
 * Get free extent for requested size
 */
    GET_FREE_EXTENT(
		    extent_header,
		    extent_no,
		    sizeof( TABLE_HEADER )
		    );

/*=======================================================================
||									||
||			ALLOCATE TABLE HEADER IN FREE EXTENT		||
||									||
 =======================================================================*/

    /*
     * Get space for table header & set table header
     */
    extent_ptr.ext_no		=			/* last ext no	*/
      extent_no;

    extent_ptr.offset		=			/* free adr off	*/
      extent_header->free_addr_off;

    table_header		=			/* table header	*/
      (TABLE_HEADER*)GET_ABS_ADDR( extent_ptr );

    temp.table_data		=			/* table data	*/
      *table_data;

    temp.record_count		=
      0;

    temp.free_count		=
      0;

    *table_header		=
      temp;

    MAKE_NULL_EXT_PTR(
		      table_header->next_table
		      );

    MAKE_NULL_EXT_PTR(
		      table_header->first_row		/* first row	*/
		      );

    MAKE_NULL_EXT_PTR(
		      table_header->last_row		/* last row	*/
		      );

    MAKE_NULL_EXT_PTR(
		      table_header->first_free		/* first free rw*/
		      );

    MAKE_NULL_EXT_PTR(
		      table_header->last_free		/* last free row*/
		      );

    /*
     * Update master extent & pointing back
     */
    if(						/* table chain not empty*/
       ! IS_NULL_EXT_PTR( master_shm_header->last_table )
       )
    {
	tmp_table_header		=
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      master_shm_header->last_table
				      );

	tmp_table_header->next_table	=
	  extent_ptr;

	table_header->back_table	=
	  master_shm_header->last_table;
    }
    else					/* table chain empty	*/
    {
	master_shm_header->first_table	=
	  extent_ptr;

	MAKE_NULL_EXT_PTR(
			  table_header->back_table
			  );
    }

    master_shm_header->last_table	=
      extent_ptr;

    /*
     * Update free extent
     */
    extent_header->free_addr_off	+=
      sizeof(TABLE_HEADER);

    extent_header->free_space		-=
      sizeof(TABLE_HEADER);

/*
 * Return - open the table for access
 * ----------------------------------
 */
    ret = OpenTable(
		    table_data->table_name,
		    tablep
		    );
    
	ReleaseResource ();
	return (ret);
}

/*=======================================================================
||									||
||				OpenTable()				||
||									||
||		Open table for access / manipulate it's items		||
||		This function should be run by child processes		||
||									||
 =======================================================================*/
extern "C" int	OpenTable(
		  char		*table_name,	/* table name		*/
		  TABLE		**tablep	/* table pointer	*/
		  )
{

/*
 * Local variables
 */
    int				i;
    TABLE_HEADER		*table_header;	/* table header		*/
    EXTENT_PTR			table_extent_ptr;/* extent pointer	*/

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			ATTACH ALL NEWLY ALLOCATED EXTENTS		||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*=======================================================================
||									||
||			GET CONNECTION TO TABLE				||
||									||
 =======================================================================*/

/*
 * Check if max connections exceeded
 */
    if( ConnCount >= CONN_MAX )
    {
	*tablep	= NULL;

	RETURN( CONN_MAX_EXCEED );
    }

/*
 * Get table header
 */
    
    table_extent_ptr		=
      master_shm_header->first_table;

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  table_extent_ptr
				  );

    while(
	  table_header != NULL	&&
	  strcmp(
		 table_header->table_data.table_name,
		 table_name
		 )
	  )
    {
	table_extent_ptr	=
	  table_header->next_table;

	table_header		=
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      table_extent_ptr
				      );
    }
    
    if( table_header == NULL )
    {
	*tablep	= NULL;

	RETURN( SHM_TAB_NOT_EX );
    }


/*
 * Find free entry in conn tab
 */
    for(
	i = 0;
	i < CONN_MAX			&&
	  ConnTab[i].busy_flg == MAC_TRUE;
	i++
	);

    if( i >= CONN_MAX )
    {
	GerrLogReturn(
		      GerrId,
		      "I have a bug with busy_flg, ConnCount matching"
		      );

	*tablep	= NULL;

	RETURN( CONN_MAX_EXCEED );
    }

/*
 * Occupy free entry for request
 */
    ConnTab[i].busy_flg		=
      MAC_TRUE;

    ConnTab[i].table_header	=
      table_extent_ptr;

    ConnTab[i].curr_row		=
      table_header->first_row;

    ConnCount++;

 /*
 * Return - set table pointer
 * --------------------------
 */
    *tablep	= ConnTab + i;

	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				CloseTable()				||
||									||
||	Close table -- end access to table with current connection	||
||									||
 =======================================================================*/
int	CloseTable(
		   TABLE	*tablep		/* table pointer	*/
		   )
{

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			GET CONNECTION TO TABLE				||
||									||
 =======================================================================*/
/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

/*
 * Free the entry of table pointer
 */
    ConnTab[tablep->conn_id].busy_flg		=
      0;

    MAKE_NULL_EXT_PTR(
		      ConnTab[tablep->conn_id].table_header
		      );

    MAKE_NULL_EXT_PTR(
		      ConnTab[tablep->conn_id].curr_row
		      );

    ConnCount--;

 /*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				AddItem()				||
||									||
||	Add item record to existing opened table.			||
||	This function should be called only by Father process		||
||									||
 =======================================================================*/
int	AddItem(
		TABLE		*tablep,	/* table pointer	*/
		ROW_DATA	row_data	/* table row data	*/
		)
{

/*
 * Local variables
 */
    int				row_ind,	/* row existence flag	*/
				extent_no,
				record_size;
    EXTENT_HEADER		*extent_header;	/* extent header	*/
    TABLE_ROW			*free_table_row,/* free table row	*/
				*tmp_table_row;/*table row for insert	*/
    TABLE_HEADER		*table_header;	/* table header		*/
    EXTENT_PTR			row_extent_ptr;	/* extent pointer	*/

    EXTENT_PTR			save_ext;	/* extent header	*/
/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			GET FREE SPACE FOR NEW TABLE ROW		||
||									||
 =======================================================================*/
/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    row_extent_ptr		=
      table_header->first_free;

    free_table_row		=
      (TABLE_ROW*)GET_ABS_ADDR(
			       row_extent_ptr
			       );

    if(
       free_table_row		== NULL
       )
    {
	/*
	 * Get free extent for requested size
	 */
	record_size		=
	  sizeof(TABLE_ROW) +
	  table_header->table_data.record_size;

	/*
	 * Round up requested size to machine word
	 */
	state			=
	  record_size % sizeof(int);

	if( state )
	{
	    record_size		+=
	      ( sizeof(int) - state );
	}

	save_ext =
	    master_shm_header->free_extent;

	GET_FREE_EXTENT(
			extent_header,
			extent_no,
			record_size
			);

	// DonR 25Apr2022: Replace "#if 1" with "#if LOG_EVERYTHING" to reduce size of "kuku" file.
	if (LOG_EVERYTHING)
	{
		if( save_ext.ext_no !=
			master_shm_header->free_extent.ext_no )
		{
			printf(
			   "---------------------------------------\n"
			   "---------------< ADDITEM >-------------\n"
			   " table : (%s) record no (%d) size (%d) "
			   " free space (%d) after address offset (%d).\n"
			   "---------------< ADDITEM >-------------\n"
			   "---------------------------------------\n",
			   table_header->table_data.table_name,
			   table_header->record_count,
			   record_size,
			   extent_header->free_space,
			   extent_header->free_addr_off
			   );
			fflush(stdout);
		}
	}

	/*
	 * Take space from free extent
	 */
	row_extent_ptr.ext_no	=			/* extent ptr	*/
	  extent_no;

	row_extent_ptr.offset	=
	  extent_header->free_addr_off;

	free_table_row		=			/* free table rw*/
	  (TABLE_ROW*)GET_ABS_ADDR(
				   row_extent_ptr
				   );

	free_table_row->row_data.offset =		/* free row	*/
	  row_extent_ptr.offset	+
	  sizeof(TABLE_ROW);

	free_table_row->row_data.ext_no =
	  row_extent_ptr.ext_no;

	/*
	 * Update free extent
	 */
	extent_header->free_addr_off		+=
	  record_size;

	extent_header->free_space		-=
	  record_size;
    }
    else
    {
	/*
	 * Take out first free row from free rows chain
	 */
	table_header->first_free		=
	  free_table_row->next_row;

	if( IS_NULL_EXT_PTR( table_header->first_free ) )
	{
	    MAKE_NULL_EXT_PTR( table_header->last_free	);
	}
	else
	{
	    tmp_table_row =
	      (TABLE_ROW*)GET_ABS_ADDR(
				       table_header->first_free
				       );

	    MAKE_NULL_EXT_PTR( tmp_table_row->back_row );
	}

	/*
	 * Update free rows count for table
	 */
	table_header->free_count--;
    }

    /*
     * Insert new row into rows chain
     */
    free_table_row->back_row	=
      table_header->last_row;

    if( ! IS_NULL_EXT_PTR( table_header->last_row ) )
    {
	tmp_table_row	=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   table_header->last_row
				   );

	tmp_table_row->next_row	=
	  row_extent_ptr;
    }
    else
    {
	table_header->first_row	=
	  row_extent_ptr;
    }

    MAKE_NULL_EXT_PTR(
		      free_table_row->next_row
		      );

    table_header->last_row	=
      row_extent_ptr;

    /*
     * Update table pointer
     */
    tablep->curr_row	=
      row_extent_ptr;

    /*
     * Update table record count
     */
    table_header->record_count++;

/*=======================================================================
||									||
||			PUT DATA INTO TABLE ROW	DATA			||
||									||
 =======================================================================*/

    memcpy(
	   (char*)GET_ABS_ADDR(
			       free_table_row->row_data
			       ),
	   row_data,
	   table_header->table_data.data_size
	   );

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				DeleteItem()				||
||									||
||	Delete item record from existing opened table.			||
||	This function should be called only by Father process		||
||									||
 =======================================================================*/
int	DeleteItem(
		   TABLE	*tablep,	/* table pointer	*/
		   ROW_DATA	row_data	/* table row data	*/
		   )
{

/*
 * Local variables
 */
    TABLE_DATA			*table_data;	/* table data		*/
    TABLE_ROW			*table_row,	/* table row		*/
				*temp;		/* temporary row pointer*/
    EXTENT_HEADER		*extent_header;	/* extent header	*/
    TABLE_HEADER		*table_header;	/* table header		*/
    EXTENT_PTR			row_extent_ptr;	/* extent pointer	*/

    ROW_DATA			curr_row;	/* current row of table	*/
    int				table_no;
    
/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			FIND ROW LOCATION IN TABLE			||
||									||
 =======================================================================*/
/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*
 * Find row in table
 * -----------------
 */
    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    table_no = GetTabNumByName( table_header->table_data.table_name );

    if( table_no < 0 )
    {
	RETURN( TAB_NAM_ERR );
    }
    
    table_data =
      TableTab + table_no;

    if(
       table_data->row_comp == NULL
       )
    {
	RETURN( NO_COMP_FUN );
    }

    /*
     * Loop on table rows
     */
    row_extent_ptr		=		/* Go to first row	*/
      table_header->first_row;

    table_row		=
      (TABLE_ROW*)GET_ABS_ADDR(
			       row_extent_ptr
			       );

    while( table_row != NULL )
    {
	curr_row		=		// get row data
	  (ROW_DATA)GET_ABS_ADDR(
				 table_row->row_data
				 );

	if(					// compare
	   ! table_data->row_comp(
			        curr_row,
			        row_data
			        )
	    )
	{
	    break;
	}

	row_extent_ptr		=		/* goto next row	*/
	  table_row->next_row;

	table_row		=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   row_extent_ptr
				   );
    }

/*
 * Check if row was found
 */
    if(
       table_row		== NULL
       )
    {
	RETURN( ROW_NOT_FOUND );
    }

/*=======================================================================
||									||
||			DELETE ROW FROM TABLE				||
||									||
 =======================================================================*/

    /*
     * Remove row from rows chain
     */
    if( ! IS_NULL_EXT_PTR( table_row->back_row ) )
    {
	temp			=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   table_row->back_row
				   );

	temp->next_row		=
	  table_row->next_row;
    }

    else
    {
	table_header->first_row	=
	  table_row->next_row;
    }

    if( ! IS_NULL_EXT_PTR( table_row->next_row ) )
    {
	temp			=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   table_row->next_row
				   );

	temp->back_row		=
	  table_row->back_row;
    }

    else
    {
	table_header->last_row	=
	  table_row->back_row;
    }

    /*
     * Insert row at the end of free rows chain
     */
    MAKE_NULL_EXT_PTR(
		      table_row->next_row
		      );

    if( ! IS_NULL_EXT_PTR( table_header->last_free ) )
    {
	temp			=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   table_header->last_free
				   );

	temp->next_row		=
	  row_extent_ptr;

	table_row->back_row	=
	  table_header->last_free;

	table_header->last_free	=
	  row_extent_ptr;
    }
    else
    {
	table_header->last_free	=
	  row_extent_ptr;

	table_header->first_free	=
	  row_extent_ptr;

	MAKE_NULL_EXT_PTR(
			  table_row->back_row
			  );
    }

    /*
     * Update table record count
     */
    table_header->record_count--;

    /*
     * Update free rows count for table
     */
    table_header->free_count++;

    RewindTable( tablep );

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				DeleteTable()				||
||									||
||	Delete all item records from existing opened table.		||
||	This function should be called only by Father process		||
||									||
 =======================================================================*/
int	DeleteTable(
		    TABLE	*tablep		/* table pointer	*/
		    )
{

/*
 * Local variables
 */
    int				row_ind;	/* row existence flag	*/
    TABLE_ROW			*table_row,	/* table row		*/
				*temp;		/* temporary row pointer*/
    EXTENT_HEADER		*extent_header;	/* extent header	*/
    TABLE_HEADER		*table_header;	/* table header		*/
    EXTENT_PTR			row_extent_ptr;	/* extent pointer	*/

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			FIND ROW LOCATION IN TABLE			||
||									||
 =======================================================================*/
/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*=======================================================================
||									||
||			DELETE ALL ROWS FROM TABLE			||
||									||
 =======================================================================*/

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    /*
     * Check if table isn't empty
     */
    if( ! IS_NULL_EXT_PTR( table_header->first_row ) )
    {
	row_extent_ptr		=
	  table_header->first_row;

	table_row		=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   row_extent_ptr
				   );

	/*
	 * Check if free rows chain isn't empty
	 */
	if( ! IS_NULL_EXT_PTR( table_header->last_free ) )
	{
	    temp		=
	      (TABLE_ROW*)GET_ABS_ADDR(
				       table_header->last_free
				       );

	    /*
	     * Append table rows to free rows chain
	     */
	    temp->next_row	=
	      row_extent_ptr;

	    table_row->back_row	=
	      table_header->last_free;

	    table_header->last_free	=
	      table_header->last_row;
	}

	else
	{
	    table_header->last_free	=
	      table_header->last_row;

	    table_header->first_free	=
	      table_header->first_row;
	}
    }

    /*
     * Truncate table
     */
    MAKE_NULL_EXT_PTR(
		      table_header->first_row
		      );

    MAKE_NULL_EXT_PTR(
		      table_header->last_row
		      );

/*
 * Zero table record count
 * -----------------------
 */
    table_header->free_count +=
      table_header->record_count;

    table_header->record_count =
      0;

    RewindTable( tablep );

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				ListTables()				||
||									||
||			List all table names in memory			||
||									||
 =======================================================================*/
void	ListTables(
		   void
		   )
{
    TABLE_HEADER	*table_header;

    puts("----------------< TABLE SUMMARY (START) >----------------------");
    for(
	table_header		=
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      master_shm_header->first_table
				      );
	 table_header		!= NULL;
	 table_header		=
	  (TABLE_HEADER*)GET_ABS_ADDR(
				      table_header->next_table
				      )
	)
    {
	printf( "table_name '%s' rows count '%d' free count '%d'..\n",
		table_header->table_data.table_name,
		(int)table_header->record_count,
		(int)table_header->free_count
		);
    }
    puts("----------------< TABLE SUMMARY (STOP) >----------------------");
    fflush(stdout);
}

/*=======================================================================
||									||
||				ParmComp()				||
||									||
||		Compare two table rows in Parm table			||
||									||
 =======================================================================*/
int	ParmComp(
		 const void     *row_data1,	/* one table row	*/
		 const void	*row_data2	/* another table row	*/
		 )
{
    PARM_DATA	*parm_data1,
		*parm_data2;

    parm_data1 =
      (PARM_DATA*)row_data1;
    
    parm_data2 =
      (PARM_DATA*)row_data2;
    
    return(
	   strcmp(
		  parm_data1->par_key,
		  parm_data2->par_key
		  )
	   );

}    

/*=======================================================================
||									||
||				ProcComp()				||
||									||
||		Compare two table rows in Proc table			||
||									||
 =======================================================================*/
int	ProcComp(
		 const void	*row_data1,	/* one table row	*/
		 const void	*row_data2	/* another table row	*/
		 )
{
    PROC_DATA	*proc_data1,
		*proc_data2;

    proc_data1 =
      (PROC_DATA*)row_data1;
    
    proc_data2 =
      (PROC_DATA*)row_data2;
    
    return(
	   proc_data1->pid	-
	   proc_data2->pid
	   );

}    

/*=======================================================================
||									||
||				UpdtComp()				||
||									||
||		Compare two table rows in Updt table			||
||									||
 =======================================================================*/
int	UpdtComp(
		 const void	*row_data1,	/* one table row	*/
		 const void	*row_data2	/* another table row	*/
		 )
{
    UPDT_DATA	*updt_data1,
		*updt_data2;

    updt_data1 =
      (UPDT_DATA*)row_data1;
    
    updt_data2 =
      (UPDT_DATA*)row_data2;
    
    return(
	   strcmp(
		  updt_data1->table_name,
		  updt_data2->table_name
		  )
	   );

}    

/*=======================================================================
||									||
||				ActItems()				||
||									||
||			Act on record items in a table			||
||									||
 =======================================================================*/
int	ActItems(
		 TABLE		*tablep,	/* table pointer	*/
		 int		rewind_flg,	/* rewind table flag	*/
		 OPERATION	func,		/* operation on a row	*/
		 OPER_ARGS	args		/* arguments 4 operation*/
		 )
{

    TABLE_ROW			*table_row;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			LOOP ON ITEM RECORDS OF TABLE			||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*
 * Go to first row in table
 * ------------------------
 */
    if( rewind_flg )
    {
	RewindTable( tablep );
    }

/*
 * Act on record items of table
 * ----------------------------
 */
    while( ! IS_NULL_EXT_PTR( tablep->curr_row ) )
    {

	table_row			=
	  (TABLE_ROW*)GET_ABS_ADDR(
				   tablep->curr_row
				   );

	switch(
	       (*func)(
		       tablep,
		       args
		       )
	       )
	{
	    case OPER_FORWARD:
		tablep->curr_row	=
		  table_row->next_row;
		continue;

	    case OPER_STOP:
		tablep->curr_row	=
		  table_row->next_row;
		RETURN( MAC_OK );

	    case OPER_BACKWARD:
		tablep->curr_row	=
		  table_row->back_row;
		continue;
	}
    }

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (NO_MORE_ROWS);
}

/*=======================================================================
||									||
||				SetRecordSize()				||
||									||
||			Set record size of table			||
||									||
 =======================================================================*/
int	SetRecordSize(
		      TABLE	*tablep,	/* table pointer	*/
		      int	data_size	/* data size		*/
		      )
{
    TABLE_HEADER	*table_header;
  
/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			LOOP ON ITEM RECORDS OF TABLE			||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    WaitForResource();

    ATTACH_ALL();

/*
 * Use current record if enough space
 * Or truncate table elsewhere
 * ---------------------------
 */
    table_header =
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );
	
    MAKE_NULL_EXT_PTR( table_header->first_free );

    MAKE_NULL_EXT_PTR( table_header->last_free );

    table_header->free_count = 0;

    //
    // Set new record size if needed
    // Inflate when new size needed
    //
    if(
       table_header->table_data.record_size <
       data_size + 1
       )
    {

	//
	// Inflate record size to keep bigger records
	// in the future
	//
	table_header->table_data.record_size =
	  (int)ceil( data_size * 2.5 );

	table_header->record_count =
	  0;

	MAKE_NULL_EXT_PTR( table_header->first_row );
	
	MAKE_NULL_EXT_PTR( table_header->last_row );
	
    }
    else
    {
	DeleteTable( tablep );
    }
    
    //
    // Set new data size
    //
    table_header->table_data.data_size =
      data_size;

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				RewindTable()				||
||									||
||			Rewind table ( Move to first row )		||
||									||
 =======================================================================*/
int	RewindTable(
		    TABLE	*tablep		/* table pointer	*/
		    )
{

    TABLE_HEADER		*table_header;

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

    CHECK_TAB_PTR( tablep );

/*=======================================================================
||									||
||			REWIND TABLE					||
||									||
 =======================================================================*/

/*
 * Lock shared memory
 * ------------------
 */
    RETURN_ON_ERR( WaitForResource() );

    ATTACH_ALL();

/*
 * Rewind table
 * ------------
 */
    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    tablep->curr_row		=
      table_header->first_row;

/*
 * Return - ok
 * -----------
 */
	ReleaseResource ();
	return (MAC_OK);
}

/*=======================================================================
||									||
||				GetItem()				||
||									||
||			Get all table record items			||
||		Third argument is space to get data			||
||									||
 =======================================================================*/
int	GetItem(
		 TABLE		*tablep,	/* table pointer	*/
		 OPER_ARGS	args		/* argument to get data	*/
		 )
{

    TABLE_ROW			*table_row;
    TABLE_HEADER		*table_header;
    ROW_DATA			row_data;

/*=======================================================================
||									||
||			GET DATA OF ROW IN TABLE			||
||									||
 =======================================================================*/

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    if( table_row == NULL )
    {
	return( OPER_STOP );
    }
    
    row_data			=
      (ROW_DATA)GET_ABS_ADDR(
			     table_row->row_data
			     );
    
    memcpy(
	   args,
	   row_data,
	   table_header->table_data.data_size
	   );

    return( OPER_STOP );

}

// DonR 18Jan2023: SeqFindItem() is NIU, but I'm leaving it enabled for now.
/*=======================================================================
||									||
||				SeqFindItem()				||
||									||
||		Find item in the table by sequential search		||
||				Alexey Tulin				||
||									||
 =======================================================================*/

int SeqFindItem( TABLE	*tablep,	// table pointer
		 OPER_ARGS args )
{  
    TABLE_ROW			*table_row;
    TABLE_HEADER		*table_header;
    ROW_DATA			row_data;
    PFINDITEMPARAMS		pFindItemParams = (PFINDITEMPARAMS)args;

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    if( table_row == NULL )
    {
	return( OPER_STOP );
    }

    row_data			=
      (ROW_DATA)GET_ABS_ADDR(
			     table_row->row_data
			     );

    // Comparing
    if ( (memcmp( pFindItemParams->pKeyLower, 
	          row_data + pFindItemParams->sStartKey, 
		  pFindItemParams->sKeyLength ) <= 0) &&
	 (memcmp( row_data + pFindItemParams->sStartKey, 
	          pFindItemParams->pKeyUpper, 
		  pFindItemParams->sKeyLength ) <= 0) ) 
	 return  ( OPER_STOP ); 
    else
         return ( OPER_FORWARD );


}

/*=======================================================================
||									||
||				SetItem()				||
||									||
||			List all table record items			||
||		Third argument is space to get data			||
||									||
 =======================================================================*/
int	SetItem(
		 TABLE		*tablep,	/* table pointer	*/
		 OPER_ARGS	args		/* argument to get data	*/
		 )
{

    TABLE_ROW			*table_row;
    TABLE_HEADER		*table_header;
    ROW_DATA			row_data;

/*=======================================================================
||									||
||			GET DATA OF ROW IN TABLE			||
||									||
 =======================================================================*/

    table_header		=
      (TABLE_HEADER*)GET_ABS_ADDR(
				  tablep->table_header
				  );

    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    if( table_row == NULL )
    {
	return( OPER_STOP );
    }
    
    row_data			=
      (ROW_DATA)GET_ABS_ADDR(
			     table_row->row_data
			     );
    
    memcpy(
	   row_data,
	   args,
	   table_header->table_data.data_size
	   );

    return( OPER_STOP );

}


/*=======================================================================
||									||
||				GetProc()				||
||									||
||			Get process entry				||
||		Third argument is space to get data			||
||									||
 =======================================================================*/
int	GetProc(
		TABLE		*tablep,	/* table pointer	*/
		OPER_ARGS	args		/* argument to get data	*/
		)
{

/*
 * Local variables
 */
    PROC_DATA			*proc_data,	/* space for data of row*/
				*table_proc_data;
    TABLE_ROW			*table_row;

/*=======================================================================
||									||
||			GET DATA OF ROW IN TABLE			||
||									||
 =======================================================================*/

    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    table_proc_data		=
      (PROC_DATA*)GET_ABS_ADDR(
			       table_row->row_data
			       );

    proc_data			=
      (PROC_DATA*)args;

    if(
       table_proc_data->pid	==
       proc_data->pid
       )
    {

	memcpy(
	       (char*)proc_data,
	       (char*)table_proc_data,
	       sizeof(PROC_DATA)
	       );

	return( OPER_STOP );
    }

    return( OPER_FORWARD );

}

/*=======================================================================
||									||
||				get_sys_status()			||
||									||
||		Get system status from shared memory table		||
||									||
 =======================================================================*/
int	get_sys_status(
		  STAT_DATA	*stat_data	/* status data record	*/
		  )
{
    TABLE	* stat_tablep;
/*=======================================================================
||									||
||			GET DATA OF ROW IN TABLE			||
||									||
 =======================================================================*/

    RETURN_ON_ERR( OpenTable( STAT_TABLE, & stat_tablep ) );

    state =
      ActItems(
	       stat_tablep,
	       1,
	       GetItem,
	       (OPER_ARGS) stat_data
	       );

    CloseTable( stat_tablep );

    return( state );

}
/*=======================================================================
||									||
||				AddCurrProc()				||
||									||
||	Add process  record to processes table.				||
||	This function should be called  by process that creates another	||
||									||
 =======================================================================*/
int	AddCurrProc(
		    int		retrys,		/* retrys for process	*/
		    short int	proc_type,	/* process type		*/
		    int		eicon_port,	/* eicon port for tsserv*/
		    short int	system		/* pharmacy / doctors	*/
		    )
{
/*
 * Local variables
 */
    PROC_DATA	proc_data;
    TABLE	*proc_tablep;
    struct tm	*tm_ptr;
	
	memset (&proc_data, 0, sizeof(PROC_DATA));

/*=======================================================================
||									||
||			VALIDITY CHECK					||
||									||
 =======================================================================*/

    VALIDITY_CHECK();

/*=======================================================================
||									||
||			OPEN PROCESSES TABLE				||
||									||
 =======================================================================*/

    RETURN_ON_ERR( OpenTable( PROC_TABLE, &proc_tablep) );

/*=======================================================================
||									||
||			SET PROCESS ATTRIBUTES				||
||									||
 =======================================================================*/

    strcpy(
	   proc_data.proc_name,
	   CurrProgName
	   );

    strcpy(
	   proc_data.named_pipe,
	   CurrNamedPipe
	   );

    strcpy(
	   proc_data.log_file,
	   LogFile
	   );

    proc_data.proc_type			= proc_type;
    proc_data.busy_flg			= MAC_FALS;
    proc_data.pid			= getpid();
    proc_data.father_pid		= getppid();
    proc_data.start_time		= time( NULL );
    proc_data.retrys			= retrys + 1;
    proc_data.eicon_port		= eicon_port;
    proc_data.system			= system;

    tm_ptr = localtime(
		       &proc_data.start_time
		       );

    proc_data.s_year			= 1900 + tm_ptr->tm_year;
    proc_data.s_mon			= tm_ptr->tm_mon + 1;
    proc_data.s_day			= tm_ptr->tm_mday;
    proc_data.s_hour			= tm_ptr->tm_hour;	  
    proc_data.s_min			= tm_ptr->tm_min;
    proc_data.s_sec			= tm_ptr->tm_sec;

    proc_data.l_year			= proc_data.s_year;
    proc_data.l_mon			= proc_data.s_mon;
    proc_data.l_day			= proc_data.s_day;
    proc_data.l_hour			= proc_data.s_hour;
    proc_data.l_min			= proc_data.s_min;
    proc_data.l_sec			= proc_data.s_sec;

/*=======================================================================
||									||
||			PUT ATTRIBUTES INTO PROCESSES TABLE		||
||									||
 =======================================================================*/

    RETURN_ON_ERR(
		  AddItem(
			  proc_tablep,
			  (ROW_DATA)&proc_data
			  )
		  );
	    
/*=======================================================================
||									||
||			CLOSE PROCESSES TABLE				||
||									||
 =======================================================================*/

    return( CloseTable( proc_tablep) );
}

/*=======================================================================
||									||
||				UpdateTimeStat()			||
||									||
||		Update time statistics for sql processes		||
||									||
 =======================================================================*/
int	UpdateTimeStat(
		       int	system,		/* pharmacy / doctors	*/
		       void	*data		/* statistics data area	*/
		       )
{
    TABLE	* tablep;
    char	arguments[256];
    int		message_index;

    message_index = system == PHARM_SYS ?
	((SSMD_DATA*)data)->msg_idx :
	((DSMD_DATA*)data)->msg_idx ;

    if( GetMessageIndex( system, message_index ) == -1 )
    {
	return( MSG_NUM_ERR );
    }

    memcpy(
	   arguments,
	   (char*)&data,
	   sizeof(void*)
	   );

    RETURN_ON_ERR( OpenTable(
		(char *)(system == PHARM_SYS ? TSTT_TABLE : DSTT_TABLE), //Linux
		& tablep ) );

    state =
	ActItems(
		 tablep,
		 1,
		 system == PHARM_SYS ?
		 IncrementCounters : IncrementDoctorCounters,
		 (OPER_ARGS)arguments
		 );

    CloseTable( tablep );

    return( state );
    
}

/*=======================================================================
||									||
||				IncrementCounters()			||
||									||
||		Update time statistics for sql processes		||
||									||
 =======================================================================*/
int	IncrementCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  )
{

/*
 * Local variables
 */
    time_t			curr_time[TOTAL_INTERVALS];
    int			curr_message_index,
				interval,
				entry,
				sum_message_index,
				stop;

    TSTT_DATA			*tstt_data;
    struct timeval 		difftime;
    TABLE_ROW			*table_row;
    SSMD_DATA			*ssmd_data;

/*=======================================================================
||									||
||			OBTAIN CURRENT TIME , DAY & PARAMETERS		||
||									||
 =======================================================================*/

    table_row			=
	(TABLE_ROW*)GET_ABS_ADDR(
				 tablep->curr_row
				 );

    tstt_data =
	(TSTT_DATA*)GET_ABS_ADDR(
				 table_row->row_data
				 );

    memcpy(
	   (char*)&ssmd_data,
	   args,
	   sizeof(SSMD_DATA*)
	   );

    curr_message_index =
	GetMessageIndex(
			PHARM_SYS,
			ssmd_data->msg_idx
			);

    sum_message_index =
	GetMessageIndex(
			PHARM_SYS,
			ALL_MSGS
			);
    
    curr_time[SECOND_INTERVAL] =
	time( NULL );

    curr_time[MINUTE_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 60;

    curr_time[HOUR_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 3600;
    
    curr_time[DAY_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 86400;

/*=======================================================================
||									||
||			UPDATE MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    for(
	interval = DAY_INTERVAL;
	interval <= SECOND_INTERVAL;
	interval++
	)
    {

	/*
	 * Handle current message
	 */
	if(
	   curr_time[interval]			<
	   tstt_data->last_time[curr_message_index][interval] +
	   TOTAL_ENTRIES
	   )
	{
	    entry =
		(tstt_data->last_time[curr_message_index][interval] + 1)
		% TOTAL_ENTRIES;

	    stop =
		(curr_time[interval] + 1)
		% TOTAL_ENTRIES;

	    for(;
		entry != stop;
		entry = (entry + 1)
		    % TOTAL_ENTRIES
		)
	    {
		tstt_data->msg_count
		    [curr_message_index]
		    [interval]
		    [entry] = 0;
	    }
	}

	else

	{
	    bzero(
		  (char*)tstt_data->msg_count
		  [curr_message_index]
		  [interval],
		  sizeof(
			 tstt_data->msg_count
			 [curr_message_index]
			 [interval]
			 )
		  );
	}


	/*
	 * Handle sum of all messages
	 */
	if(
	   curr_time[interval]			<
	   tstt_data->last_time[sum_message_index][interval] +
	   TOTAL_ENTRIES
	   )
	{
	    entry =
		(tstt_data->last_time[sum_message_index][interval] + 1)
		% TOTAL_ENTRIES;

	    stop =
		(curr_time[interval] + 1)
		% TOTAL_ENTRIES;

	    for(;
		entry != stop;
		entry = (entry + 1)
		    % TOTAL_ENTRIES
		)
	    {
		tstt_data->msg_count
		    [sum_message_index]
		    [interval]
		    [entry] = 0;
	    }
	}

	else

	{
	    bzero(
		  (char*)tstt_data->msg_count
		  [sum_message_index]
		  [interval],
		  sizeof(
			 tstt_data->msg_count
			 [sum_message_index]
			 [interval]
			 )
		  );
	}

	tstt_data->last_time[sum_message_index][interval] =
	    tstt_data->last_time[curr_message_index][interval] =
	    curr_time[interval];

	tstt_data->msg_count
	    [curr_message_index]
	    [interval]
	    [curr_time[interval] % TOTAL_ENTRIES]++;

	tstt_data->msg_count
	    [sum_message_index]
	    [interval]
	    [curr_time[interval] % TOTAL_ENTRIES]++;

    }

/*=======================================================================
||									||
||			UPDATE TIME STATISTICS				||
||									||
 =======================================================================*/
	
    gettimeofday( &difftime, NULL );

    difftime.tv_usec -=
	ssmd_data->proc_time.tv_usec;

    difftime.tv_sec -=
	ssmd_data->proc_time.tv_sec;

    if(
       difftime.tv_usec < 0
       )
    {
	difftime.tv_usec += 1000000;

	difftime.tv_sec--;
    }

    ssmd_data->proc_time = difftime;

    /*
     * Current message
     */
    tstt_data->time_count			/* total message count	*/
	[curr_message_index]
	[MSGCNT_ENTRY]++;

    tstt_data->time_count			/* total message wait	*/
	[curr_message_index]
	[MSGWAIT_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	(int)difftime.tv_usec / 10000;

    tstt_data->time_count			/* total message work	*/
	[curr_message_index]
	[MSGWORK_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	 (int)difftime.tv_usec / 10000;


    /*
     * Sum message
     */
    tstt_data->time_count			/* total message count	*/
	[sum_message_index]
	[MSGCNT_ENTRY]++;

    tstt_data->time_count			/* total message wait	*/
	[sum_message_index]
	[MSGWAIT_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	(int)difftime.tv_usec / 10000;

    tstt_data->time_count			/* total message work	*/
	[sum_message_index]
	[MSGWORK_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	 (int)difftime.tv_usec / 10000;

/*=======================================================================
||									||
||			UPDATE LAST MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    entry =
	tstt_data->total_msg_count % TOTAL_LAST;

    tstt_data->last_mesg[entry] =
	*ssmd_data;

    tstt_data->total_msg_count++;

    return( OPER_STOP );

}

/*=======================================================================
||									||
||				IncrementDoctorCounters()		||
||									||
||		Update time statistics for sql processes		||
||									||
 =======================================================================*/
int	IncrementDoctorCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  )
{

/*
 * Local variables
 */
    time_t			curr_time[TOTAL_DOC_INTERVALS];
    int			curr_message_index,
				interval,
				entry,
				sum_message_index,
				stop;

    DSTT_DATA			*dstt_data;
    struct timeval 		difftime;
    TABLE_ROW			*table_row;
    DSMD_DATA			*dsmd_data;

/*=======================================================================
||									||
||			OBTAIN CURRENT TIME , DAY & PARAMETERS		||
||									||
 =======================================================================*/

    table_row			=
	(TABLE_ROW*)GET_ABS_ADDR(
				 tablep->curr_row
				 );

    dstt_data =
	(DSTT_DATA*)GET_ABS_ADDR(
				 table_row->row_data
				 );

    memcpy(
	   (char*)&dsmd_data,
	   args,
	   sizeof(DSMD_DATA*)
	   );

    curr_message_index =
	GetMessageIndex(
			DOCTOR_SYS,
			dsmd_data->msg_idx
			);

    sum_message_index =
	GetMessageIndex(
			DOCTOR_SYS,
			ALL_MSGS
			);
    
    curr_time[SECOND_INTERVAL] =
	time( NULL );

    curr_time[MINUTE_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 60;

    curr_time[HOUR_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 3600;
    
    curr_time[DAY_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 86400;

/*=======================================================================
||									||
||			UPDATE MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    for(
	interval = DAY_INTERVAL;
	interval <= SECOND_INTERVAL;
	interval++
	)
    {

	/*
	 * Handle current message
	 */
	if(
	   curr_time[interval]			<
	   dstt_data->last_time[curr_message_index][interval] +
	   TOTAL_DOC_ENTRIES
	   )
	{
	    entry =
		(dstt_data->last_time[curr_message_index][interval] + 1)
		% TOTAL_DOC_ENTRIES;

	    stop =
		(curr_time[interval] + 1)
		% TOTAL_DOC_ENTRIES;

	    for(;
		entry != stop;
		entry = (entry + 1)
		    % TOTAL_DOC_ENTRIES
		)
	    {
		dstt_data->msg_count
		    [curr_message_index]
		    [interval]
		    [entry] = 0;
	    }
	}

	else

	{
	    bzero(
		  (char*)dstt_data->msg_count
		  [curr_message_index]
		  [interval],
		  sizeof(
			 dstt_data->msg_count
			 [curr_message_index]
			 [interval]
			 )
		  );
	}


	/*
	 * Handle sum of all messages
	 */
	if(
	   curr_time[interval]			<
	   dstt_data->last_time[sum_message_index][interval] +
	   TOTAL_DOC_ENTRIES
	   )
	{
	    entry =
		(dstt_data->last_time[sum_message_index][interval] + 1)
		% TOTAL_DOC_ENTRIES;

	    stop =
		(curr_time[interval] + 1)
		% TOTAL_DOC_ENTRIES;

	    for(;
		entry != stop;
		entry = (entry + 1)
		    % TOTAL_DOC_ENTRIES
		)
	    {
		dstt_data->msg_count
		    [sum_message_index]
		    [interval]
		    [entry] = 0;
	    }
	}

	else

	{
	    bzero(
		  (char*)dstt_data->msg_count
		  [sum_message_index]
		  [interval],
		  sizeof(
			 dstt_data->msg_count
			 [sum_message_index]
			 [interval]
			 )
		  );
	}

	dstt_data->last_time[sum_message_index][interval] =
	    dstt_data->last_time[curr_message_index][interval] =
	    (int)curr_time[interval];

	dstt_data->msg_count
	    [curr_message_index]
	    [interval]
	    [curr_time[interval] % TOTAL_DOC_ENTRIES]++;

	dstt_data->msg_count
	    [sum_message_index]
	    [interval]
	    [curr_time[interval] % TOTAL_DOC_ENTRIES]++;

    }

/*=======================================================================
||									||
||			UPDATE TIME STATISTICS				||
||									||
 =======================================================================*/
	
    gettimeofday( &difftime, NULL );

    difftime.tv_usec -=
	dsmd_data->proc_time.tv_usec;

    difftime.tv_sec -=
	dsmd_data->proc_time.tv_sec;

    if(
       difftime.tv_usec < 0
       )
    {
	difftime.tv_usec += 1000000;

	difftime.tv_sec--;
    }

    dsmd_data->proc_time = difftime;

    /*
     * Current message
     */
    dstt_data->time_count			/* total message count	*/
	[curr_message_index]
	[MSGCNT_ENTRY]++;

    dstt_data->time_count			/* total message wait	*/
	[curr_message_index]
	[MSGWAIT_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	(int)difftime.tv_usec / 10000;

    dstt_data->time_count			/* total message work	*/
	[curr_message_index]
	[MSGWORK_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	 (int)difftime.tv_usec / 10000;


    /*
     * Sum message
     */
    dstt_data->time_count			/* total message count	*/
	[sum_message_index]
	[MSGCNT_ENTRY]++;

    dstt_data->time_count			/* total message wait	*/
	[sum_message_index]
	[MSGWAIT_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	(int)difftime.tv_usec / 10000;

    dstt_data->time_count			/* total message work	*/
	[sum_message_index]
	[MSGWORK_ENTRY] +=
	(int)difftime.tv_sec * 100 +
	 (int)difftime.tv_usec / 10000;

/*=======================================================================
||									||
||			UPDATE LAST MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    entry = dstt_data->total_msg_count % TOTAL_DOC_LAST;

    dstt_data->last_mesg[entry] = *dsmd_data;

    dstt_data->total_msg_count++;

    return( OPER_STOP );

}

/*=======================================================================
||									||
||				RescaleTimeStat()			||
||									||
||		Rescale time statistics for sql processes		||
||									||
 =======================================================================*/
int	RescaleTimeStat(
			int	system		/* pharmacy / doctors	*/
			)
{

    TABLE	*tablep;

    RETURN_ON_ERR( OpenTable(
		(char *)(system == PHARM_SYS ? TSTT_TABLE : DSTT_TABLE), //Linux
		& tablep ) );

    state =
	ActItems(
		 tablep,
		 1,
		 system == PHARM_SYS ?
		 RescaleCounters : RescaleDoctorCounters,
		 (OPER_ARGS)NULL
		 );

    CloseTable( tablep );

    return( state );
    
}

/*=======================================================================
||									||
||				RescaleCounters()			||
||									||
||		Rescale time statistics for sql processes		||
||									||
 =======================================================================*/
int	RescaleCounters(
			TABLE		*tablep,/* table pointer	*/
			OPER_ARGS	args	/* argument to get data	*/
			)
{

/*
 * Local variables
 */
    time_t			curr_time[TOTAL_INTERVALS];
    int			interval,
				message_idx,
				entry,
				stop;

    TSTT_DATA			*tstt_data;
    TABLE_ROW			*table_row;

/*=======================================================================
||									||
||			OBTAIN CURRENT TIME , DAY & PARAMETERS		||
||									||
 =======================================================================*/

    table_row			=
	(TABLE_ROW*)GET_ABS_ADDR(
				 tablep->curr_row
				 );

    tstt_data =
	(TSTT_DATA*)GET_ABS_ADDR(
				 table_row->row_data
				 );
    
    curr_time[SECOND_INTERVAL] =
	time( NULL );

    curr_time[MINUTE_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 60;

    curr_time[HOUR_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 3600;
    
    curr_time[DAY_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 86400;

/*=======================================================================
||									||
||			RESCALE MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    for(
	interval = DAY_INTERVAL;
	interval <= SECOND_INTERVAL;
	interval++
	)
    {
	       
	for(
	    message_idx = 0;
	    message_idx < TOTAL_MSGS;
	    message_idx++
	    )
	{
	    if(
	       curr_time[interval]			<
	       tstt_data->last_time[message_idx][interval] +
	       TOTAL_ENTRIES
	       )
	    {
		entry =
		    (tstt_data->last_time[message_idx][interval] + 1)
		    % TOTAL_ENTRIES;

		stop =
		    (curr_time[interval] + 1)
		    % TOTAL_ENTRIES;

		for(;
		    entry != stop;
		    entry = (entry + 1)
			% TOTAL_ENTRIES
		    )
		{
		    tstt_data->msg_count
			[message_idx]
			[interval]
			[entry] = 0;
		}
	    }
	    else
	    {
		bzero(
		      (char*)tstt_data->msg_count
		      [message_idx]
		      [interval],
		      sizeof(
			     tstt_data->msg_count
			     [message_idx]
			     [interval]
			     )
		      );
	    }

	    tstt_data->last_time[message_idx][interval] =
		curr_time[interval];

	}
    }

    return( OPER_STOP );

}

/*=======================================================================
||									||
||				RescaleDoctorCounters()			||
||									||
||		Rescale time statistics for sql processes		||
||									||
 =======================================================================*/
int	RescaleDoctorCounters(
			TABLE		*tablep,/* table pointer	*/
			OPER_ARGS	args	/* argument to get data	*/
			)
{

/*
 * Local variables
 */
    time_t			curr_time[TOTAL_DOC_INTERVALS];
    int			interval,
				message_idx,
				entry,
				stop;

    DSTT_DATA			*dstt_data;
    TABLE_ROW			*table_row;

/*=======================================================================
||									||
||			OBTAIN CURRENT TIME , DAY & PARAMETERS		||
||									||
 =======================================================================*/

    table_row			=
	(TABLE_ROW*)GET_ABS_ADDR(
				 tablep->curr_row
				 );

    dstt_data =
	(DSTT_DATA*)GET_ABS_ADDR(
				 table_row->row_data
				 );
    
    curr_time[SECOND_INTERVAL] =
	time( NULL );

    curr_time[MINUTE_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 60;

    curr_time[HOUR_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 3600;
    
    curr_time[DAY_INTERVAL] =
	curr_time[SECOND_INTERVAL] / 86400;

/*=======================================================================
||									||
||			RESCALE MESSAGES STATISTICS			||
||									||
 =======================================================================*/

    for(
	interval = DAY_INTERVAL;
	interval <= SECOND_INTERVAL;
	interval++
	)
    {
	       
	for(
	    message_idx = 0;
	    message_idx < TOTAL_DOC_MSGS;
	    message_idx++
	    )
	{
	    if(
	       curr_time[interval]			<
	       dstt_data->last_time[message_idx][interval] +
	       TOTAL_DOC_ENTRIES
	       )
	    {
		entry =
		    (dstt_data->last_time[message_idx][interval] + 1)
		    % TOTAL_DOC_ENTRIES;

		stop =
		    (curr_time[interval] + 1)
		    % TOTAL_DOC_ENTRIES;

		for(;
		    entry != stop;
		    entry = (entry + 1)
			% TOTAL_DOC_ENTRIES
		    )
		{
		    dstt_data->msg_count [message_idx] [interval] [entry] = 0;
		}
	    }
	    else
	    {
		bzero(
		      (char*)dstt_data->msg_count [message_idx] [interval],
		      sizeof( dstt_data->msg_count [message_idx] [interval])
		      );
	    }

	    dstt_data->last_time[message_idx][interval] = (int)curr_time[interval];

	}
    }

    return( OPER_STOP );

}

/*=======================================================================
||									||
||				GetMessageIndex()			||
||									||
||				Get message index			||
 =======================================================================*/
int	GetMessageIndex(
			int system,		/* pharmacy / doctors	*/
			int message_num		/* message number	*/
			)
{
    int		counter;

    int		*message_arr =
		system == PHARM_SYS ?
		PharmMessages : DoctorMessages ;

    for(
	counter = 0;
	message_arr[counter] &&
	  message_arr[counter] != message_num;
	counter++
	);

    if(
       message_arr[counter]
       )
    {
	return( counter );
    }

    return( -1 );
}

/*=======================================================================
||									||
||				IncrementRevision()			||
||									||
||		Increment revision of parameters in shm 		||
||									||
 =======================================================================*/
int	IncrementRevision(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  )
{

    TABLE_ROW		*table_row;
    STAT_DATA		*stat_data;
    
/*=======================================================================
||									||
||			INCREMENT PARAMETERS REVISION			||
||									||
 =======================================================================*/

    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    stat_data =
      (STAT_DATA*)GET_ABS_ADDR(
			       table_row->row_data
			       );

    stat_data->param_rev++;

	ReleaseResource ();
	return (OPER_STOP);
}

/*=======================================================================
||									||
||				UpdateShmParams()			||
||									||
||			Update parameters if needed			||
||									||
 =======================================================================*/
int	UpdateShmParams( void )
{
    TABLE	*stat_tablep;
    STAT_DATA	stat_data;
    static time_t	last_time = 0;

    if( time(NULL) - last_time < SharedMemUpdateInterval )
    {
	return MAC_OK;
    }

    last_time = time(NULL);

    RETURN_ON_ERR( OpenTable( STAT_TABLE, & stat_tablep ) );

    state =
      ActItems(
	       stat_tablep,
	       1,
	       GetItem,
	       (OPER_ARGS)&stat_data
	       );

    CloseTable( stat_tablep );

    /*
     * Compare process revision with current revision
     */
    if( stat_data.param_rev <= ParametersRevision )
    {
	return( MAC_OK );
    }

    /*
     * Get new parameters from shm
     */
    state =
      ShmGetParamsByName(
			 SonProcParams,
			 PAR_RELOAD
			 );

    ParametersRevision = stat_data.param_rev;

    return( state );
}


/*=======================================================================
||																		||
||				GetFreeSqlServer()										||
||																		||
||			Get free sql server to connect								||
||																		||
 =======================================================================*/
int	GetFreeSqlServer	(	int		*pid,			// pid of sql server
							char	*named_pipe,	// named pipe of sql server
							short	proc_type		// SqlServer or DocSqlServer
						)
{
	// Local variables
	TABLE		*proc_tablep;
	PROC_DATA	proc_data;

	// Zero parameters
	*pid			= 0;
	named_pipe [0]	= 0;

	// Loop on processes to find free sql server
	RETURN_ON_ERR (OpenTable (PROC_TABLE, &proc_tablep));

	// Set proc_type as lookup parameter (see LockSqlServer)
	proc_data.proc_type = proc_type;

	// Look for free SqlServer for given system
	state = ActItems (proc_tablep, 1, LockSqlServer, (OPER_ARGS)&proc_data);

	CloseTable (proc_tablep);

	if (state == MAC_OK)
	{
		*pid = proc_data.pid;
		strcpy( named_pipe, proc_data.named_pipe );
	}

	// Set parameters
	return (state);
}

/*=======================================================================
||																		||
||				LockSqlServer()											||
||																		||
||			Lock sql server for connection	 							||
||																		||
 =======================================================================*/
int	LockSqlServer	(	TABLE		*tablep,	// table pointer
						OPER_ARGS	args		// argument to get data
					)
{
	TABLE_ROW	*table_row;
	PROC_DATA	*proc_data;
    
	// GET & LOCK FREE SQL SERVER
	table_row	= (TABLE_ROW*) GET_ABS_ADDR (tablep->curr_row);
	proc_data	= (PROC_DATA*) GET_ABS_ADDR (table_row->row_data);

	if ((proc_data->proc_type	!= ((PROC_DATA*)args)->proc_type)	||
		(proc_data->busy_flg	!= MAC_FALS))
	{
		RETURN (OPER_FORWARD);
	}

	proc_data->busy_flg = MAC_TRUE;
    
	memcpy (args, (char*)proc_data, sizeof(PROC_DATA));

	return (OPER_STOP);
}

/*=======================================================================
||																		||
||				SetFreeSqlServer()										||
||																		||
||			Set Free sql server to connect								||
||																		||
 =======================================================================*/
int	SetFreeSqlServer (int	pid		/* pid of sql server	*/	)
{
	// Local variables
	TABLE		*proc_tablep;
	PROC_DATA	proc_data;

	// Set pid of sql server
	proc_data.pid = pid;

	// Loop on processes to free occupied sql server
	RETURN_ON_ERR (OpenTable (PROC_TABLE, &proc_tablep));

	state = ActItems (proc_tablep, 1, ReleaseSqlServer, (OPER_ARGS)&proc_data);

	CloseTable (proc_tablep);

	// Return result.
	return (state);
}

/*=======================================================================
||																		||
||				ReleaseSqlServer()										||
||																		||
||			Release sql server for connection							||
||																		||
 =======================================================================*/
int	ReleaseSqlServer	(	TABLE		*tablep,	// table pointer
							OPER_ARGS	args		// argument to get data
						)
{
	TABLE_ROW	*table_row;
	PROC_DATA	*curr_proc_data, *proc_data;

	// RELEASE FREE SQL SERVER
	table_row			= (TABLE_ROW *)GET_ABS_ADDR (tablep->curr_row);
	proc_data			= (PROC_DATA *)GET_ABS_ADDR (table_row->row_data);
	curr_proc_data		= (PROC_DATA *)args;

	if (proc_data->pid != curr_proc_data->pid)
	{
	 RETURN (OPER_FORWARD);
	}

	proc_data->busy_flg = MAC_FALS;

	return (OPER_STOP);
}


/*=======================================================================
||																		||
||				ShmGetParamsByName()									||
||																		||
||			Get parameters from shared memory by name					||
||																		||
 =======================================================================*/
int	ShmGetParamsByName (	ParamTab	*prog_params,	/* params descr struct */
							int			reload_flg		/* load or reload requested */	)
{
	TABLE		*parm_tablep;
	PARM_DATA	parm_data;
	int			i, log_file_set;
	char		prog_parm	[255],
				all_parm	[255],
				prog_log_key[255],
				all_log_key	[255];


    VALIDITY_CHECK();

	// Open shared-memory parameters table.
	RETURN_ON_ERR (OpenTable (PARM_TABLE, &parm_tablep));

	// Clear "touched" flag for all params
	for (i = 0; prog_params[i].param_val != NULL; i++)
	{
		prog_params[i].touched_flg = MAC_FALS;
	}

	// Get parameter keys for log file
	sprintf (prog_log_key,	"%s.%s", CurrProgName,	LOG_FILE_KEY);
	sprintf (all_log_key,	"%s.%s", ALL_PREFIX,	LOG_FILE_KEY);

	// Get parameter values from shared memory.
    log_file_set = 0;

	while (ActItems (parm_tablep, 0, GetItem, (OPER_ARGS)&parm_data) == MAC_OK)
	{
		if (log_file_set)	// log file already set
		{
			if (!strcmp (parm_data.par_key, all_log_key))
			{
				continue;
			}
		}
		else	// log file is NOT already set
		{
			if (!strcmp (parm_data.par_key, prog_log_key))
			{
				log_file_set = 1;
			}
		}

		// Look in params descr buffer if param need load/reload
		for (i = 0; prog_params[i].param_name != NULL; i++)
		{
			sprintf (prog_parm,	"%s.%s", CurrProgName,	prog_params[i].param_name);
			sprintf (all_parm,	"%s.%s", ALL_PREFIX,	prog_params[i].param_name);

			if (	(!strcmp (parm_data.par_key, prog_parm))	||
					(!strcmp (parm_data.par_key, all_parm))			)
			{
				if (	(reload_flg == PAR_LOAD)	||
						((reload_flg == PAR_RELOAD) && (prog_params[i].reload_flg == PAR_RELOAD))	)
				{
					switch (prog_params[i].param_type)
					{
						case PAR_INT:
						case PAR_LONG:		*(int *)prog_params[i].param_val = atoi (parm_data.par_val);
											break;

						case PAR_CHAR:		strcpy ((char*)prog_params[i].param_val, parm_data.par_val);
											break;

						case PAR_DOUBLE:	*(double *)prog_params[i].param_val = atof (parm_data.par_val);
											break;

						default:			GerrLogReturn (GerrId, "Got unknown parameter type: %d", prog_params[i].param_type);
											break;
					}	// Switch on parameter type.
				}	// Either an initial load or a reload of a reload-enabled parameter.

				prog_params[i].touched_flg = MAC_TRUE;
				break;	// Param was found.
			}	// Parameter name matched.
	    }	// Loop through parameter array to find a match.
	}	// Loop through the shared-memory parameter table.

	// Close params table
	RETURN_ON_ERR (CloseTable (parm_tablep));

	// Check if we got all the parameters we (supposedly) need.
	state = MAC_OK;	// Default = no problem.

	for (i = 0; prog_params[i].param_val != NULL; i++)
	{
		if ((reload_flg == PAR_LOAD)	||
			((reload_flg == PAR_RELOAD) && (prog_params[i].reload_flg == PAR_RELOAD)))
		{
			if (prog_params[i].touched_flg == MAC_FALS)
			{
				GerrLogReturn (GerrId, "Parameter '%s' not %sloaded!", prog_params[i].param_name, (reload_flg == PAR_RELOAD) ? "re" : "");
				state = SQL_PARAM_MISS;
			}
		}
	}

	return (state);
}



/*=======================================================================
||																		||
||			GetPrescriptionId()											||
||																		||
||		Get free prescription id from shared memory						||
||																		||
 =======================================================================*/
int	GetPrescriptionId (TABLE *prsc_tablep, OPER_ARGS args)
{
	TABLE_HEADER		*table_header;
	TABLE_ROW			*table_row;
	ROW_DATA			row_data;
	RAW_DATA_HEADER		*raw_data_header;
	PRESCRIPTION_ROW	*prescription_row;
	PRESCRIPTION_ROW	*cur_prescription_row;


	RewindTable (prsc_tablep);

	table_header		= (TABLE_HEADER *)	GET_ABS_ADDR (prsc_tablep->table_header);
	table_row			= (TABLE_ROW *)		GET_ABS_ADDR (prsc_tablep->curr_row);

	if (table_row == NULL)
	{
		return (OPER_STOP);
	}

	row_data			= (ROW_DATA) GET_ABS_ADDR (table_row->row_data);

	raw_data_header		= (RAW_DATA_HEADER *)	row_data;

	cur_prescription_row= (PRESCRIPTION_ROW *)	args;

	prescription_row	= (PRESCRIPTION_ROW *)	(row_data + sizeof(RAW_DATA_HEADER));

	*cur_prescription_row = *prescription_row;

	prescription_row->prescription_id++;

	return (OPER_STOP);
}



/*=======================================================================
||																		||
||			GetMessageRecId()											||
||																		||
||		Get free message rec_id from shared memory						||
||																		||
 =======================================================================*/
int	GetMessageRecId (TABLE *recid_tablep, OPER_ARGS args)
{
	TABLE_HEADER		*table_header;
	TABLE_ROW			*table_row;
	ROW_DATA			row_data;
	RAW_DATA_HEADER		*raw_data_header;
	MESSAGE_RECID_ROW	*msg_recid_row;
	MESSAGE_RECID_ROW	*cur_msg_recid_row;


	RewindTable (recid_tablep);

	table_header		= (TABLE_HEADER *)	GET_ABS_ADDR (recid_tablep->table_header);
	table_row			= (TABLE_ROW *)		GET_ABS_ADDR (recid_tablep->curr_row);

	if (table_row == NULL)
	{
		return (OPER_STOP);
	}

	row_data			= (ROW_DATA) GET_ABS_ADDR (table_row->row_data);

	raw_data_header		= (RAW_DATA_HEADER *)	row_data;

	cur_msg_recid_row	= (MESSAGE_RECID_ROW *)	args;

	msg_recid_row		= (MESSAGE_RECID_ROW *)	(row_data + sizeof(RAW_DATA_HEADER));

	*cur_msg_recid_row = *msg_recid_row;

	msg_recid_row->message_rec_id++;

	return (OPER_STOP);
}



/*=======================================================================
||									||
||				DiprComp()				||
||									||
||		Compare two rows of died processes table		||
||									||
 =======================================================================*/
int	DiprComp(
		 const void	*row_data1,
		 const void	*row_data2
		 )
{
    DIPR_DATA
	*dipr_data1,
	*dipr_data2;
    

    int
      pid_diff;
    
    dipr_data1 =
      (DIPR_DATA*)row_data1;
    
    dipr_data2 =
      (DIPR_DATA*)row_data2;

    pid_diff =
      dipr_data1->pid -
      dipr_data2->pid;

    if( pid_diff )
    {
	return( pid_diff );
    }

    return(
	   dipr_data1->status -
	   dipr_data2->status
	   );
}



/*=======================================================================
||									||
||				GetSonsCount()				||
||									||
||			Get son proccesses count			||
||									||
 =======================================================================*/
int	GetSonsCount(
		     int		*sons	/* son proccesses count	*/
		     )
{

/*
 * Local variables
 */
    TABLE			*stat_tablep;
    STAT_DATA			stat_data;	/* space for data of row*/

/*=======================================================================
||									||
||			GET DATA OF ROW IN TABLE			||
||									||
 =======================================================================*/

    RETURN_ON_ERR( OpenTable( STAT_TABLE, & stat_tablep ) );

    state =
      ActItems(
	       stat_tablep,
	       1,
	       GetItem,
	       (OPER_ARGS)&stat_data
	       );

    CloseTable( stat_tablep );

    * sons	= stat_data.sons_count;

    return( state );

}

/*=======================================================================
||									||
||				AddToSonsCount()			||
||									||
||			Add to son processes count			||
||									||
 =======================================================================*/
int	AddToSonsCount(
			int		addition/* addition to son count*/
			)
{
    TABLE		*stat_tablep;
    STAT_DATA		stat_data;

    RETURN_ON_ERR( OpenTable( STAT_TABLE, & stat_tablep ) );

    //
    // Lock shared memory
    //
    RETURN_ON_ERR( WaitForResource() );

    ATTACH_ALL();

    state =
      ActItems(
	       stat_tablep,
	       1,
	       GetItem,
	       (OPER_ARGS)&stat_data
	       );

	if (LOG_EVERYTHING)
	{
	    printf ("raising sons_count from '%d' by '%d' in pid (%d)\n", stat_data.sons_count, addition, getpid());
		fflush (stdout);
	}

    stat_data.sons_count += addition;

    state =
      ActItems(
	       stat_tablep,
	       1,
	       SetItem,
	       (OPER_ARGS)&stat_data
	       );

    CloseTable( stat_tablep );

    //
    // Release lock & return
    //
	ReleaseResource ();
	return (state);
}

/*=======================================================================
||									||
||				GetTabNumByName()			||
||									||
||			Get table number by table name			||
||									||
 =======================================================================*/
int	GetTabNumByName(
			const char	*table_name
			)
{
    int	table_num;

    for(
	table_num = 0;
	TableTab[table_num].table_name[0]		&&
	    strcmp( TableTab[table_num].table_name, table_name );
	table_num++
	);

    if( TableTab[table_num].table_name[0] )
    {
	return( table_num );
    }

    return( -1 );
}



/*=======================================================================
||									||
||				UpdateLastAccessTime()			||
||									||
||			Update last access time for process		||
||									||
 =======================================================================*/
int	UpdateLastAccessTime( void )
{
    TABLE	*proc_tablep;

    RETURN_ON_ERR( OpenTable( PROC_TABLE, & proc_tablep ) );

    state =
      ActItems(
	       proc_tablep,
	       1,
	       UpdateLastAccessTimeInternal,
	       (OPER_ARGS)NULL
	       );

    CloseTable( proc_tablep );

    return( state );

}

/*=======================================================================
||									||
||			UpdateLastAccessTimeInternal()			||
||									||
||			Update last access time for process		||
||									||
 =======================================================================*/
int	UpdateLastAccessTimeInternal(
				     TABLE	*tablep,/* table pointer*/
				     OPER_ARGS	args	/* arguments	*/
				     )
{

    TABLE_ROW			*table_row;
    PROC_DATA
				*proc_data;
    time_t			curr_time;
    struct tm			*tm_ptr;
    
/*=======================================================================
||									||
||			GET PARAMETERS REVISION				||
||									||
 =======================================================================*/
    
    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    proc_data =
      (PROC_DATA*)GET_ABS_ADDR(
			       table_row->row_data
			       );

    if( proc_data->pid == getpid() )
    {
	time( &curr_time );

	tm_ptr = localtime( &curr_time );

	proc_data->l_year	= tm_ptr->tm_year + 1900;

	proc_data->l_mon	= tm_ptr->tm_mon + 1;

	proc_data->l_day	= tm_ptr->tm_mday;

	proc_data->l_hour	= tm_ptr->tm_hour;

	proc_data->l_min	= tm_ptr->tm_min;

	proc_data->l_sec	= tm_ptr->tm_sec;

	return( OPER_STOP );
    }

    return( OPER_FORWARD );

}

/*=======================================================================
||									||
||				UpdateTable()				||
||									||
||			Update record of table				||
||									||
 =======================================================================*/
int	UpdateTable(
		    TABLE	*tablep,		/* table pointer*/
		    OPER_ARGS	args			/* arguments	*/
		    )
{

    TABLE_ROW			*table_row;
    UPDT_DATA
				*updt_data,
				*curr_updt_data;
    
/*=======================================================================
||									||
||			GET PARAMETERS REVISION				||
||									||
 =======================================================================*/
    
    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    curr_updt_data =
      (UPDT_DATA*)GET_ABS_ADDR(
			       table_row->row_data
			       );

    updt_data =
      (UPDT_DATA*)args;

    if( ! strcmp( updt_data->table_name, curr_updt_data->table_name ) )
    {
	curr_updt_data->update_date = 
	    updt_data->update_date;

	curr_updt_data->update_time = 
	    updt_data->update_time;

	return( OPER_STOP );
    }

    return( OPER_FORWARD );

}

/*=======================================================================
||									||
||				GetTable()				||
||									||
||			Get record of table				||
||									||
 =======================================================================*/
int	GetTable(
		 TABLE		*tablep,		/* table pointer*/
		 OPER_ARGS	args			/* arguments	*/
		 )
{

    TABLE_ROW			*table_row;
    UPDT_DATA
				*updt_data,
				*curr_updt_data;
    
/*=======================================================================
||									||
||			GET PARAMETERS REVISION				||
||									||
 =======================================================================*/
    
    table_row			=
      (TABLE_ROW*)GET_ABS_ADDR(
			       tablep->curr_row
			       );

    curr_updt_data =
      (UPDT_DATA*)GET_ABS_ADDR(
			       table_row->row_data
			       );

    updt_data =
      (UPDT_DATA*)args;

    if( ! strcmp( updt_data->table_name, curr_updt_data->table_name ) )
    {
	updt_data->update_date =
	    curr_updt_data->update_date;

	updt_data->update_time =
	    curr_updt_data->update_time;

	return( OPER_STOP );
    }

    return( OPER_FORWARD );

}

/*=======================================================================
||									||
||				set_sys_status()			||
||									||
||			Set system status in shared memory		||
||									||
 =======================================================================*/
int	set_sys_status(
                        int	status,
                        int	pharm_status,
                        int	doctor_status
			)
{

    TABLE	*stat_tablep;
    STAT_DATA	stat_data;
/*=======================================================================
||			UPDATE STAT TABLE IN SHM			||
 =======================================================================*/

    RETURN_ON_ERR( OpenTable( STAT_TABLE, & stat_tablep ) );

    //
    // Lock shared memory
    //
    WaitForResource();

    ATTACH_ALL();

    //
    // Get stat row
    //
    state =
      ActItems(
	       stat_tablep,
	       1,
	       GetItem,
	       (OPER_ARGS)& stat_data
	       );

    //
    // Update what needed...
    //
    if( status )	stat_data.status = status;
    if( pharm_status )	stat_data.pharm_status = pharm_status;
    if( doctor_status )	stat_data.doctor_status = doctor_status;

    //
    // Put stat row
    //
    state =
      ActItems(
	       stat_tablep,
	       1,
	       SetItem,
	       (OPER_ARGS)& stat_data
	       );

    CloseTable( stat_tablep );

    //
    // Release shared memory
    //
	ReleaseResource ();
	return (state);
}
