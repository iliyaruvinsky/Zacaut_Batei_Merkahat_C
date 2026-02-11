// MACSQL.H - Function Prototypes for MACSQL.EC
// Created 13Jun2004 by Don Radlauer.


int	DbOpenAuthSelCursor(int);

int	DbFetchAuthSelCursor( TAuthRecord *Ptr);

int	DbCloseAuthSelCursor(void);

int DbOpenActionSelCursor(void);

int DbFetchActionSelCursor( TActionRecord	*Ptr);

int DbCloseActionSelCursor(void);

int DbOpenTaxSendSelCursor(int);

int DbFetchTaxSendSelCursor( TTaxSendRecord	*Ptr);

int DbCloseTaxSendSelCursor(void);

int DbOpenVisNewSelCursor(void);

int DbFetchVisNewSelCursor( TVisitNewRecord	*Ptr);

int DbCloseVisNewSelCursor(void);

int DbOpenVisitSelCursor(void);

int DbFetchVisitSelCursor( TVisitRecord	*Ptr);

int DbCloseVisitSelCursor(void);

int DbOpenShiftSelCursor(void);

int DbFetchShiftSelCursor( TShiftRecord	*Ptr);

int DbCloseShiftSelCursor(void);

int DbOpenPresenceSelCursor(void);

int DbFetchPresenceSelCursor( TPresenceRecord	*Ptr);

int DbClosePresenceSelCursor(void);

int	DbOpenLabRefSelCursor(void);

int	DbFetchLabRefSelCursor( TLabRefRecord *Ptr);

int	DbCloseLabRefSelCursor(void);

int	DbOpenLabRefCodeSelCursor(void);

int	DbFetchLabRefCodeSelCursor( TLabRefRecord *Ptr);

int	DbCloseLabRefCodeSelCursor(void);

int	DbOpenLabRefObsSelCursor(void);

int	DbFetchLabRefObsSelCursor( TLabRefRecord *Ptr);

int	DbCloseLabRefObsSelCursor(void);

int	DbOpenLabRefObsASelCursor(void);

int	DbFetchLabRefObsASelCursor( TLabRefRecord *Ptr);

int	DbCloseLabRefObsASelCursor(void);

int	DbOpenMedParamSelCursor(void);

int	DbFetchMedParamSelCursor( TMedParamRecord *Ptr);

int	DbCloseMedParamSelCursor(void);

int DbUpdateVisitSelCursor(TVisitRecord	*Ptr);

int DbUpdateActionSelCursor(TActionRecord *Ptr);

int DbUpdateTaxSendSelCursor(TTaxSendRecord	*Ptr);

int DbUpdateShiftSelCursor(TShiftRecord *Ptr);

int DbUpdateVisNewSelCursor(TVisitNewRecord *Ptr);

int DbUpdatePresenceSelCursor( TPresenceRecord	*Ptr);

int DbUpdateAuthSelCursor( TAuthRecord *Ptr);

int DbUpdateLabRefSelCursor(TLabRefRecord	*Ptr);

int DbUpdateLabRefCodeSelCursor(TLabRefRecord	*Ptr);

int DbUpdateLabRefObsSelCursor(TLabRefRecord	*Ptr);

int DbUpdateLabRefObsASelCursor(TLabRefRecord	*Ptr);

int DbUpdateMedParamSelCursor(TMedParamRecord	*Ptr);

int	DbOpenUpdCardSelCursor(void);

int	DbFetchUpdCardSelCursor( TCardRecord *Ptr);

int	DbCloseUpdCardSelCursor(void);

int DbUpdateUpdCardSelCursor( TCardRecord *Ptr);

int	DbInsertLabNoteRecord( TLabNoteRecord	*Ptr);

int	DbInsertLabResultRecord( TLabRecord	*Ptr);

int	DbInsertLabResultOtherRecord( TLabRecord	*Ptr,int group_num_id);

int	DbInsertLabNoteOtherRecord( TLabNoteRecord	*Ptr);

int	DbInsertHospitalRecord( THospitalRecord	*Ptr);

int	DbGetTempMembRecord( 	TIdNumber		CardId,
				TTempMembRecord		*Ptr);

int	DbInsertTempMembRecord( TTempMembRecord	*Ptr);

int	DbUpdateTempMembRecord( TIdNumber		CardId,
				TTempMembRecord		*Ptr);

int	DbDeleteTempMembRecord( TIdNumber	CardId);

int DbCancleAuthVisit( TIdNumber	Contractor,
			TMemberId	*MemberId,
			TSCounter	Quarter,
			TAuthCancelCode CancelCode);

int	DbCheckContractorLabResults( 	TIdNumber   ContractorParam,
				 	TMemberId   *MemberIdParam);

int	DbCheckContractorMedData( 	TIdNumber	Contractor,
					TTermId	TermId);

int	DbDecContractKbdEntryCounter( TContractRecord	*Ptr);

int	DbInsertConfLabRes( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	ReqId,int TermId,	TDbDateTime	LabCnfDate);

int	DbInsertConfRentgen( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	RentgenId,
				int TermId,
				TDbDateTime SesTime);

int	DbDeleteLabResults( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	ReqId);

int	DbGetAuthRecord( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				TSCounter	Quarter,
				TAuthRecord	*Ptr);

int	DbOpenLabResultSelCursor( 	TIdNumber	ContractorParam,
					TMemberId	*MemberIdParam);

int DbFetchLabResultSelCursor( TLabRecord	*Ptr);

int DbCloseLabResultSelCursor(void);

int DbGetLastUpdateDates(char *TableName,TDateYYYYMMDD *LastUpdateDate,TTimeHHMMSS *LastUpdateTime);

int	DbOpenDrugsSelectCursor(TDateYYYYMMDD	LastDate,
							TTimeHHMMSS		LastTime);

int DbFetchDrugsSelectCursor( TDrugRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugsSelectCursor(void);

int	DbOpenGenCompSelectCursor (TDateYYYYMMDD	LastDate,
							   TTimeHHMMSS		LastTime);

int DbFetchGenCompSelectCursor( TGenCompRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseGenCompSelectCursor(void);

int	DbOpenDrugGenCompSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime);

int DbFetchDrugGenCompSelectCursor( TDrugGenCompRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugGenCompSelectCursor(void);

int	DbOpenGenInterSelectCursor (TDateYYYYMMDD	LastDate,
								TTimeHHMMSS		LastTime);

int DbFetchGenInterSelectCursor( TGenInterRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseGenInterSelectCursor(void);

int	DbOpenGenInterTextSelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime);

int DbFetchGenInterTextSelectCursor( TGenInterTextRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseGenInterTextSelectCursor(void);

int	DbOpenGnrlDrugNotesSelectCursor (TDateYYYYMMDD	LastDate,
									 TTimeHHMMSS		LastTime);

int DbFetchGnrlDrugNotesSelectCursor( TGnrlDrugNotesRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseGnrlDrugNotesSelectCursor(void);

int	DbOpenDrugNotesSelectCursor (TDateYYYYMMDD	LastDate,
								 TTimeHHMMSS	LastTime);

int DbFetchDrugNotesSelectCursor( TDrugNotesRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugNotesSelectCursor(void);

int	DbOpenDrugDoctorProfSelectCursor (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS		LastTime);

int DbFetchDrugDoctorProfSelectCursor( TDrugDoctorProfRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugDoctorProfSelectCursor(void);

int	DbOpenEconomyPriSelectCursor (TDateYYYYMMDD	LastDate,
								  TTimeHHMMSS	LastTime);

int DbFetchEconomyPriSelectCursor( TEconomyPriRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseEconomyPriSelectCursor(void);

int	DbOpenEconomyPriDeleteCursor (TDateYYYYMMDD	LastDate,
								  TTimeHHMMSS	LastTime);

int DbFetchEconomyPriDeleteCursor( TEconomyPriRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseEconomyPriDeleteCursor(void);

int	DbOpenPharmacologySelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime);

int DbFetchPharmacologySelectCursor( TPharmacologyRecord	*Ptr,int *LastDate,int *LastTime );

int DbClosePharmacologySelectCursor(void);

// DonR 29Mar2004 New transactions for Clicks 8.5 begin.
int	DbOpenDrugsSelectCursor85 (TDateYYYYMMDD	LastDate,
							   TTimeHHMMSS		LastTime);

int DbFetchDrugsSelectCursor85 (TDrugRecord85	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugsSelectCursor85 (void);

int	DbOpenGenInterTextSelectCursor85 (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS	LastTime);

int DbFetchGenInterTextSelectCursor85 (TGenInterTextRecord85 *Ptr,int *LastDate,int *LastTime );

int DbCloseGenInterTextSelectCursor85(void);

int	DbOpenGnrlDrugNotesSelectCursor85 (TDateYYYYMMDD	LastDate,
									   TTimeHHMMSS		LastTime);

int DbFetchGnrlDrugNotesSelectCursor85 (TGnrlDrugNotesRecord85 *Ptr,int *LastDate,int *LastTime );

int DbCloseGnrlDrugNotesSelectCursor85 (void);

int	DbOpenDrugNotesSelectCursor85 (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime);

int DbFetchDrugNotesSelectCursor85 (TDrugNotesRecord85 *Ptr,int *LastDate,int *LastTime );

int DbCloseDrugNotesSelectCursor85 (void);

int	DbOpenEconomyPriSelectCursor85 (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime);

int DbFetchEconomyPriSelectCursor85 (TEconomyPriRecord85 *Ptr,int *LastDate,int *LastTime );

int DbCloseEconomyPriSelectCursor85 (void);

int	DbOpenClicksDiscountSelectCursor (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS	LastTime);

int DbFetchClicksDiscountSelectCursor( TClicksDiscountRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseClicksDiscountSelectCursor(void);

int	DbOpenUseInstructSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime);

int DbFetchUseInstructSelectCursor( TUseInstructRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseUseInstructSelectCursor(void);

int	DbOpenDrugFormsSelectCursor (TDateYYYYMMDD	LastDate,
								 TTimeHHMMSS		LastTime);

int DbFetchDrugFormsSelectCursor( TDrugFormsRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugFormsSelectCursor(void);

int	DbOpenTreatmentGrpSelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime);

int DbFetchTreatmentGrpSelectCursor( TTreatmentGrpRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseTreatmentGrpSelectCursor(void);

int	DbOpenPrescPeriodSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime);

int DbFetchPrescPeriodSelectCursor( TPrescPeriodRecord	*Ptr,int *LastDate,int *LastTime );

int DbClosePrescPeriodSelectCursor(void);

int	DbOpenDrugExtensionSelectCursor (TDateYYYYMMDD	LastDate,
									 TTimeHHMMSS		LastTime);

int DbFetchDrugExtensionSelectCursor( TDrugExtensionRecord	*Ptr,int *LastDate,int *LastTime );

int DbCloseDrugExtensionSelectCursor(void);

// DonR 12Jun2004: Add four new transactions for real-time query and
// ishur insertion.
int DbCheckForSpecialistPr (long	DocTZ_in,
							long	MemberID_in,
							long	Largo_in,
							short	*SpecialistPrStatus_out,
							short	*SpecialtyCode_out,
							long	*ParticipationPercent_out);

int DbInsertSpecLetterSeenIshur (long	Contractor_in,
								 short	MemberIDCode_in,
								 long	MemberID_in,
								 long	Largo_in,
								 long	SpecialistID_in,
								 char	*SpecName_in,
								 short	SpecialtyGroupCode_in,
								 long	DateOfLetter_in,
								 short	*RequestStatus_out);

int DbCheckForAS400Ishur (long	DocTZ_in,
						  long	MemberID_in,
						  long	Largo_in,
						  short	*IshurStatus_out,
						  long	*IshurOP_out,
						  long	*IshurUnits_out,
						  short	*IshurRenewFreq_out,
						  long	*PharmacyCode_out,
						  short	*PrivPharmSale_out,
						  long	*StopUseDate_out,
						  short	*PtnPercent_out,
						  long	*FixedPrice_out,
						  long	*MessageCode_out);

int DbInsertInteractionOkIshur (long	Contractor_in,
								short	MemberIDCode_in,
								long	MemberID_in,
								long	Largo_1_in,
								long	Largo_2_in,
								long	DocPrDate_in,
								long	DocPrID_in,
								short	*RequestStatus_out);
// DonR 29Mar2004 New transactions for Clicks 8.5 end.


int	DbOpenLabResOtherListCursor( TMemberId	*MemberIdParam);

int DbFetchLabResOtherListCursor( TLabContrOtherListReq	*Ptr,int *Prof_par);

int DbCloseLabResOtherListCursor(void);

int	DbOpenRentgenCursor( 	TIdNumber	ContractorParam,
							TMemberId	*MemberIdParam);

int DbFetchRentgenCursor( TRentgenRecord	*Ptr);

int DbCloseRentgenCursor(void);

/*20020619*/
int	DbOpenHospitCursor( 	TIdNumber	ContractorParam,
							TMemberId	*MemberIdParam,
							short int	FromSystem,
							short int	ToSystem);

int DbFetchHospitCursor( THospitalRecord	*Ptr);

int DbCloseHospitCursor(void);
/*20020619end*/

int	DbDeleteRentgen( TIdNumber	Contractor,
					TMemberId		*MemberId,
					TRentgenId		RentgenId);

int	DbCheckContractorRentgenData( 	TIdNumber	Contractor_p);
/* end rentgen 04.05.2000 */

// DonR 16Feb2005
int	DbCheckContractorUnseenIshurim (TIdNumber);


int	DbOpenLabResOtherSelCursor( 	TIdNumber	ContractorParam,
				        TMemberId	*MemberIdParam,
					TLabReqId	ReqIdMParam,
					int profParam);

int DbFetchLabResOtherSelCursor( TLabOtherRecord	*Ptr,
					int profParam);

int DbCloseLabResOtherSelCursor(int profParam);
/* end 04.05.2000 */

/* Yulia add 10.97 3-th parameter to procedure for different notetype select */
int	DbOpenLabNoteSelCursor(
			        TLabRecord	*Ptr ,
					short		NoteTypeParamFrom,
					short		NoteTypeParamTo);

int	DbFetchLabNoteSelCursor( TLabNoteRecord	  *Ptr);

int	DbCloseLabNoteSelCursor(void);

/* begin 38 LabNoteOther */
/*changed by Yulia 20040222*/
int	DbOpenLabNoteOtherSelCursor	( TLabOtherRecord	*Ptr ,
					              short		NoteTypeParamFrom,
					              short		NoteTypeParamTo);

int	DbFetchLabNoteOtherSelCursor( TLabNoteOtherRecord	  *Ptr);

int	DbCloseLabNoteOtherSelCursor(void);
/* end 38 LabNoteOther */
  
int	DbInsertTerminalRecord( TTerminalRecord *Ptr);

int	DbGetTerminalRecord( 	TTermId 		TermId,
				 TTerminalRecord 	*Ptr);

int	DbUpdateTerminalVersion( TTermId 	TermId,
				 TTerminalRecord *Ptr);

int	DbUpdateTerminalRecord( TTermId 	TermId,
				TTerminalRecord *Ptr);

int	DbDeleteTerminalRecord( TTermId TermId);

int DbGetSysParamsRecord( TSysParamsRecord	*Ptr);

int DbInsertSysParamsRecord( TSysParamsRecord	*Ptr);

int DbUpdateSysParamsRecord( TSysParamsRecord	*Ptr);

int DbDeleteContermRecord( 	TTermId		Location,
				TIdNumber	Contractor,
				TContractType	ContractType);

int DbUpdateTerminalConterms(	TTermId OldTermIdParam,
				TTermId NewTermIdParam);

int DbUpdateContermRecord( 	TTermId		NewTermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TContermRecord	*Ptr);

int DbGetContermRecord( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TContermRecord	*Ptr);

int DbUpdateContermDrug( 	TContermRecord	*Ptr);

int DbInsertContermRecord( TContermRecord	*Ptr);

int	DbInsertConStatRecord( TConStatRecord	*Ptr);

int	DbDeleteConStatRecord( 	TIdNumber	Contractor,
			   	TContractType	ContractType);

int	DbGetConStatRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TConStatRecord	*Ptr);

int	DbUpdateConStatRecord( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				short 		ReqCode,
				TLocationId 	Location,
				TDbDateTime	LastDate);

int	DbUpdateConStatLabReq( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TDbDateTime	LabDate);

int	DbUpdateConStatLabConf( TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TDbDateTime	LabCnfDate);

int	DbCloseContractorShift( TIdNumber	Contractor,
				TContractType	ContractType);

int DbGetContractor( TIdNumber Contractor );

int DbGetContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr);

int DbGetNextContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr);

int DbGetContractorDetails( 	TIdNumber	Contractor,
				TContractRecord	*Ptr);

int	DbContractorExists( TIdNumber	Contractor);

int DbUpdateContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr);

int DbInsertContractRecord( TContractRecord	*Ptr);

int DbDeleteContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType);

int DbGetMemberRecord( 	TMemberId	*MemberId,
			TMemberRecord	*Ptr);

int	DbUpdateMemberAuthorizeBit( 	TMemberId	*MemberId,
					short	Authorize);

int	DbUpdateMemberUrgentOnlyGroup( 	TMemberId	*MemberId,
					short	Urgent);

int	DbGetProfessionRecord( 	TProfession 	Profession,
				TProfessionRecord	*Ptr);

int	DbInsertProfessionRecord( TProfessionRecord	*Ptr);

int	DbUpdateProfessionRecord( 	TProfession 	Profession,
					TProfessionRecord	*Ptr);

int	DbDeleteProfessionRecord( TProfession 	Profession);

int	DbOpenMemberVisitsCursor(	TMemberId	*MemberId,
					TSCounter	Quarter,
					TAuthCancelCode CancelCode);

int	DbFetchMemberVisitsCursor( TAuthRecord *Ptr);

int	DbCloseMemberVisitsCursor(void);

int	DbOpenFirstVisitsCursor( 	TIdNumber	Contractor,
					TContractType	ContractType,
					TSCounter	Quarter);

int	DbFetchFirstVisitsCursor( TAuthRecord *Ptr);

int	DbCloseFirstVisitsCursor(void);

int	DbInsertAuthRecord( TAuthRecord *Ptr);

int	DbDeleteAuthRecord( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				TSCounter	Quarter);

int	DbGetSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor,
				TSubstRecord	*Ptr);

int	DbDeleteSubstRecordsByType( 	TIdNumber	Contractor,
					TContractType	ContractType,
					TSubstType	SubstType);

int	DbInsertSubstRecord( TSubstRecord	*Ptr);

int	DbUpdateSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor,
				TSubstRecord	*Ptr);

int	DbDeleteSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor);

int DbOpenVisitsInsCursor( TVisitRecord	*Ptr);

int DbPutVisitsInsCursor(void);

int DbCloseVisitsInsCursor(void);

int DbDeleteCurrentShiftsSelCursor(void);

int DbOpenShiftsInsCursor( TShiftRecord	*Ptr);

int DbPutShiftsInsCursor(void);

int DbCloseShiftsInsCursor(void);

int DbDeleteCurrentPresenceSelCursor(void);

int DbOpenPresenceInsCursor( TPresenceRecord	*Ptr);

int DbPutPresenceInsCursor(void);

int DbClosePresenceInsCursor(void);

int	DbDeletePcStatRecord( 	TTermId TermIdPar,
				TIdNumber	ContractorPar,
				TContractType	ContractTypePar);

int	DbInsertPcStatRecord( TPcStatRecord	*Ptr);

int	DbGetPcStatRecord( 	TTermId 	TermId,
				TPcStatRecord	*Ptr);

int	DbOpenContractLocCursor( TIdNumber	Contractor);

int	DbFetchContractLocCursor( TContermRecord	*Ptr);

int	DbCloseContractLocCursor(void);

int	DbOpenContractSubstCursor( TIdNumber	Contractor);

int	DbFetchContractSubstCursor( TSubstRecord	*Ptr);

int	DbCloseContractSubstCursor(void);

int	DbOpenProfGroupsCursor(void);

int DbFetchProfsGroupCursor( TProfessionRecord	*Ptr);

int	DbCloseProfGroupsCursor(void);

int	DbOpenTerminalLocCursor( TTermId TermId);

int	DbFetchTerminalLocCursor( TContermRecord	*Ptr);

int	DbCloseTerminalLocCursor(void);

int	DbOpenTermPhoneCursor( 	TTermId	TermId,
				int	Phone);

int	DbFetchTermPhoneCursor( TTerminalRecord	*Ptr);

int	DbCloseTermPhoneCursor(void);

int	DbUpdateTermPhone( TTerminalRecord *Ptr);

int	DbOpenTempMembCursor( 	TMemberId	*MemberId,
				TDateYYYYMMDD	ValidUntil);

int	DbFetchTempMembCursor( TTempMembRecord	*Ptr);

int	DbCloseTempMembCursor(void);

int DbOpenLogDInsCursor( TLogRecord	*Ptr);

int DbPutLogDInsCursor(void);

int DbCloseLogDInsCursor(void);

int DbOpenTempMembDInsCursor( TTempMembRecord	*Ptr);

int DbPutTempMembInsCursor(void);

int DbCloseTempMembInsCursor(void);

TSCounter   DbGetContractVisitsCountByLocation( TIdNumber	ContractorPar,
						TContractType	ContractTypePar,
						TLocationId	LocationPar,
						TSCounter	QuarterPar);

int DbOpenAuthInsCursor( TAuthRecord	*Ptr);

int DbPutAuthInsCursor(void);

int DbCloseAuthInsCursor(void);

int DbDeleteConMembRecord( 	TMemberId	*MemberId,
				TProfession 	Profession);

int DbUpdateConMembRecord( 	TMemberId	*MemberId,
				TProfession 	Profession,
				TConMembRecord	*Ptr);

int DbGetConMembRecord( 	TMemberId	*MemberIdParam,
				TProfession 	ProfessionParam,
				TConMembRecord	*Ptr,short LinkTypeGl);

int DbInsertConMembRecord( TConMembRecord	*Ptr);

int	DbOpenConMembSelCursor( TMemberId	*MemberIdParam);

int	DbFetchConMembSelCursor( TConMembRecord	*Ptr);

int	DbCloseConMembSelCursor(void);

int DbSetContermPharmacyStatus( 	TLocationId	LocationParam,
					short	HasPharmacyParam);

int DbInsertAddInsRecord( TAddInsRecord	*Ptr);

int DbGetAddInsRecord( TMemberId	*MemberId,
			TAddInsRecord	*Ptr);

int DbDeleteAddInsRecord( TMemberId	*MemberId);

int DbUpdateAddInsRecord( 	TMemberId	*MemberId,
				TAddInsRecord	*Ptr);

int DbOpenSendrecSelCursor(void);

int	DbFetchSendrecSelCursor( TSendrecRecord	*Ptr);

int DbCloseSendrecSelCursor(void);

int DbOpenSendrecInsCursor( TSendrecRecord	*Ptr);

int DbPutSendrecInsCursor(void);

int DbCloseSendrecInsCursor(void);

int	DbUpdateSndrecRecord( 	TCTablName	Tablname,
				int	Recnumb,
				TSendrecRecord	*Ptr,
				TDbDateTime CurrDateTime);

/*20020109 DonR*/
int DBAddPrescrsFromVisNewRow (TVisitNewRecord *VN_arg);
/*20020109 DonR end*/

int DbOpenVisNewInsCursor( TVisitNewRecord	*Ptr);

int DbPutVisNewInsCursor(void);

int DbCloseVisNewInsCursor(void);

/*Yulia 20030415 begin*/
int DbInsertConsultantRecord(TVisitNewRecord	*Ptr,int prof);

int DbGetConsName	(TIdNumber	Consultant,	TContractType ConsType,char *ConsName);
/*Yulia 20030415 end*/

int DbPutMedParamInsCursor( TMedParamRecord	*Ptr,	TDbDateTime	MedParamDate);

int DbPutLabRefInsCursor( TLabRefRecord	*Ptr,	TDbDateTime	LabRefDate);

int DbPutLabRefCodeInsCursor( TLabRefRecord	*Ptr,int Code,	TDbDateTime	LabRefDate,int seq);

int DbPutLabRefObservInsCursor( TLabRefRecord	*Ptr,TCObservationCode *Observ,	TDbDateTime	LabRefDate);

int DbPutLabRefObservActInsCursor( TLabRefRecord	*Ptr,TCObservationCode *Observ,	TDbDateTime	LabRefDate);

int DbOpenNurseEntranceInsCursor(TNurseEntranceRecord	*Ptr);

int DbPutNurseEntranceInsCursor(void);

int DbCloseNurseEntranceInsCursor(void);

int DbOpenNurseDailyInsCursor(TNurseDailyRecord	*Ptr);

int DbPutNurseDailyInsCursor(void);

int DbCloseNurseDailyInsCursor(void);

int DbOpenNurseReleaseInsCursor(TNurseReleaseRecord	*Ptr);

int DbPutNurseReleaseInsCursor(void);

int DbCloseNurseReleaseInsCursor(void);

int DbUpdateMemberCreditType( TMemberId *parMemberId, int parCreditType );

int DbOpenActionInsCursor( TActionRecord	*Ptr);

int DbPutActionInsCursor(void);

int DbCloseActionInsCursor(void);

int DbOpenMedDataInsCursor( TMedicalDataRecord	*Ptr);

int DbPutMedicalDataInsCursor(void);

int DbCloseMedicalDataInsCursor(void);

int DbOpenMedAnswerInsCursor( TMedAnswerRecord	*Ptr);

int DbPutMedAnswerInsCursor(void);

int DbCloseMedAnswerInsCursor(void);

int DbOpenMedDataSelCursor( 	int		RefContractor,
				int		RefTermId,
				TMemberId	*RefMemberId);

int DbFetchMedDataSelCursor( TMedicalDataRecord	*Ptr);

int DbCloseMedDataSelCursor(void);

int	DbUpdateMedData( TMedicalDataRecord	*Ptr);

int	DbUpdateMedAnswer( TMedAnswerRecord	*Ptr);

int	DbUpdateMedDataTable( 	int		Contractor,
				int		TermId,
				TMemberId	*MemberId,
				int		IdFile);

int	DbUpdateMedAnswerTable( TMemberId	*MemberId,
				int		IdFile);

int	DbOpenMedDataSelRrnCursor( TRecRelNumber RecRelNumb);

int	DbFetchMedDataSelRrnCursor( TMedicalDataRecord *Ptr);

int	DbCloseMedDataSelRrnCursor(void);

int	DbOpenMedAnswerSelRrnCursor( TRecRelNumber RecRelNumb);

int	DbFetchMedAnswerSelRrnCursor( TMedAnswerRecord *Ptr);

int	DbCloseMedAnswerSelRrnCursor(void);

int DbOpenMedAnsSelCursor( TMedicalDataRecord	*Ptr);

int DbFetchMedAnsSelCursor( TMedAnswerRecord	*Ptr);

int DbCloseMedAnsSelCursor(void);

int	DbAuthCheckLocation( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TMemberId	*MemberId,
				TLocationId	Location);

int DbGetMahozStatus( short mahoz_0, short mahoz_1 , short *status );

int DbGetMahozContractor( TAuthRecord *Auth , short *mahoz);

int DbDeleteConMembRecordNotUnique(TMemberId *MemberId);

int	DbOpenDatabase( char *DbName);

int	DbSetExplain(int	OnOff);

void	DbBeginWork(void);

void	DbEndWork(void);
   
void TraceBuffer ( 	Int16 a,
			void * b,
			TBufSize c);

size_t TruncSpace ( 	void *a,
		size_t b,
	 	short c);

int DbInsertTaxRecord(  TIdNumber   Contractor,
			TContractType   ContractType,
			TIdNumber       Number,
			Int16           Code,
			TDateYYYYMMDD   AuthDate,
			Int32           AuthTime,
			TTermId         TermId,
			Int16           TaxAmount,
			Int16           TaxCalc);

int DbUpdateAuthTaxRecord(  TIdNumber   Contractor,
			TContractType   ContractType,
			TIdNumber       Number,
			Int16           Code,
			TDateYYYYMMDD   AuthDate,
			Int32           AuthTime,
			TTermId         TermId,
			Int16           TaxAmount);

int DbOpenMsgTxtSelCursor( int MsgID);

int DbFetchMsgTxtSelCursor( char *pline);

int DbCloseMsgTxtSelCursor(void);

short DbGetCalendarDay(short StUrgent,short DayOfYear,short Year);

int DbGetLabTerm(TLabRecord	*Ptr);

int DbGetLastClicksVersDate(TDateYYYYMMDD *LastClicksVersionDate);

int DbOpenMembDrugCursor(	TMemberId	*MemberId);

int DbFetchMembDrugCursor(	int			*LargoMemb,
							TIdNumber	*ContractorMemb,
							char		*FirstNameMemb,
							char		*LastNameMemb,
							char		*PhoneMemb,
							TDateYYYYMMDD	*LargoDlvDateMemb,
							char		*DrType
							);

int	DbCloseMembDrugCursor(void);

int	DbGetInteractionStatus(	int MinLargo,	int MaxLargo,
							short *IntCode,	char *IntText);

int	DbInsertDrugNotPrioryRecord( TDrugNotPrioryRecord	*Ptr);

void	DbBeginAuditRecord(short MsgCode,
						   int   ProcId,
						   int	 MsgCount_in);

void	DbEndAuditRecord(  int   ProcId,int MacErrno);

