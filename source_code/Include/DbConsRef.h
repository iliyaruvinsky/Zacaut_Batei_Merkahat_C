#ifndef	__DB_CONSULTANT_REF_H__
#define __DB_CONSULTANT_REF_H__

	enum Sources
	{
		enCommon,
		enRadioInst // = 1 : Macon Dimut = Institute of Radiology
	};

	typedef struct tagTConsAskMembers 
	{
		struct			tagTConsAskMembers	*next;

		TMemberId		MemberId;
		TProfession		Profession;
		TIdNumber		Consultant;
		short			ConsType;
		TDateYYYYMMDD	ConsDate;
		TTimeHHMMSS		ConsTime;
		
	} TConsAskMembers;

  
	typedef struct 
	{
		int		nContractor;
		short	nContractType;
		int		nTreatmentContr;
		int		nTermId;
		int		nIdNumber;
		short	nIdCode;
		short	nProfession;
		int		nAuthDate;
		int		nAuthTime;
		int		nRrn;

	} ConsRequestRec , *PConsRequestRec;
	 
	BOOL DbInsertConsRequestRecord( TVisitNewRecord *ptr , int );
	
	// BOOL DbOpenConsRequest( int , int , int );
	// BOOL DbConsRequestFetch( PConsRequestRec );
	// BOOL DbCloseConsRequest( void );
  
	BOOL DbGetConsRequestRecord( CINT , CINT , CINT , PConsRequestRec );

	BOOL DbInsertConsRefRecord( TIdNumber		Consultant		,
								TContractType	ConsultantType	,
								// TMemberId*	pMemberId		,
								TDbDateTime*	pConsDateTime	,
								TDateYYYYMMDD	CheckDate		,
								short			nProfession		,
								short			nUrgency	 	,
								short			nTextSize		,
								PCHAR			pszText			,
								short			nSource			,
								PConsRequestRec	pVisitDate		);

  BOOL DbInsertConsRefRecordBlob(	TIdNumber		Consultant		,
									TContractType	ConsultantType	,
									TDbDateTime*	pTimeStamp		,
									TDateYYYYMMDD	CheckDate		,
									short			nProfession		,
									short			nUrgency	 	,
									short			nTextSize		,
									PCHAR			pszText			,
									short           nSource			,
									PConsRequestRec	pVisitData		);

	BOOL DbUpdateConsRequest( CINT , CINT );

	BOOL DbInsertConsSendRecordNew(	TIdNumber		Consultant		,
									TContractType	ConsultantType	,
									TMemberId*		pMemberId		,
									TDbDateTime*	pTimeStamp		,
									TDateYYYYMMDD	CheckDate		,
									short			nProfession		,
									short			nUrgency		,
									short			nTextSize		,
									PCHAR			pszText			);

	BOOL DbInsertConsSendRecordBlob(	TIdNumber		Consultant		,
										TContractType	ConsultantType	,
										TMemberId*		pMemberId		,
										TDbDateTime*    pTimeStamp		,
										TDateYYYYMMDD	CheckDate		,
										short			nProfession		,
										short			nUrgency		,
										short			nTextSize		,
										PCHAR			pszText			,
										TIdNumber       Contractor		,
										int             TermId          ) ;

	BOOL DbOpenConsAskSelCursor(	TIdNumber	nContractor		,	 
									short		nContractType	, 
									TIdNumber	nTreatmentContr	,
									int			nTermId			);
	
	BOOL DbFetchConsAskSelCursor(	TMemberId		*MemberId	,
									short			*Profession	,
									TIdNumber		*Consultant	,
									short			*ConsType	,
									TDateYYYYMMDD	*ConsDate	,				
									TTimeHHMMSS		*ConsTime,
									int				*idcount);

	BOOL	DbCloseConsAskSelCursor( void );
	  
	// BOOL	DbOpenConsAskSelSrcCursor(	TIdNumber	nContractor	,	 
	//									int			nTermId		,
	//									int			nSource		);

	// BOOL DbFetchConsAskSelSrcCursor(	TMemberId		*MemberId	,
	//									short			*Profession	,
	//									TIdNumber		*Consultant	,
	//									short			*ConsType	,
	//									TDateYYYYMMDD	*ConsDate	,
	//									TTimeHHMMSS		*ConsTime	);

    // BOOL DbCloseConsAskSelSrcCursor( void );

	BOOL	DbOpenConsAskSelRadCursor(	TIdNumber	nContractor		,
										TIdNumber	nTreatmentContr	,	 
										int			nTermId			);


	BOOL	DbFetchConsAskSelRadCursor(	TMemberId		*MemberId	,
										short			*Profession	,
										TIdNumber		*Consultant	,
										short			*ConsType	,
										TDateYYYYMMDD	*ConsDate	,
										TTimeHHMMSS		*ConsTime	);

	BOOL DbCloseConsAskSelRadCursor( void );

	BOOL DbOpenConsAskOneCursor(	TMemberId*	pMemberId		,
									short		Profession		,
									TIdNumber	Consultant		,
									short		ConsType		,
									TDateYYYYMMDD ConsDate		,
									TTimeHHMMSS	ConsTime		);

	BOOL DbFetchConsAskOneCursor(	short*			pUrgent		,
									TDateYYYYMMDD*	pCheckDate	,
									char*			ptext		,
									short*			pTextSize	);
	
	BOOL DbCloseConsAskOneCursor( void );

	int DbInsertConsConfRecord(	TIdNumber		Contractor				,
								TContractType	ContractType			,
								TIdNumber		TreatmentContr			,
								TMemberId*		pMemberId				,
								TDbDateTime*	pConsDateTime			,
								TIdNumber		ConsContractor			,
								// TContractType	ConsContractType	,
								TTermId			TermId					);

    BOOL DbDeleteAfterConfirm(	TIdNumber			Contractor			,
								TContractType		ContractType		,
								TIdNumber			TreatmentContr		,
								TMemberId*			pMemberId			,
								TDbDateTime*		pConsDateTime		,
								TIdNumber			ConsContractor		,
								TTermId				TermId				,
								CINT                nSource			);
	
	int	ChangeMonth( CINT , CINT );

#endif
