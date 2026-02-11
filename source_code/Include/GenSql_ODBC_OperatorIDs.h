/*============================================================================
||																			||
||				MacODBC_MyOperatorIDs.h										||
||																			||
||==========================================================================||
||																			||
||  PURPOSE:																||
||																			||
||  ENUMerate all ODBC SQL operations available to a particular program.	||
||																			||
||--------------------------------------------------------------------------||
|| PROJECT:	ODBC database conversion										||
||--------------------------------------------------------------------------||
|| PROGRAMMER:	Don Radlauer.												||
|| DATE:		December 2019.												||
||--------------------------------------------------------------------------||
||																			||
|| This file should be in the application's own source-code directory,		||
||	*not* in the standard include directory. Each program should have its	||
||	own version, so we don't need to have all the data for all the many		||
||	SQL operations sitting there in all our programs.						||
||																			||
 ===========================================================================*/


#pragma once

// Having both "#pragma once" and "ifndef" should be redundant, but harmless.
#ifndef GenSql_ODBC_OperatorIDs_H
#define GenSql_ODBC_OperatorIDs_H


		RefreshSeverityTable_cur,
		READ_GET_ERROR_SEVERITY,
		READ_GetComputerShortName,
		READ_GetCurrentDatabaseTime,
		READ_max_messages_details_rec_id,
		READ_max_presc_delete,
		READ_max_prescripton_id,
		READ_presc_per_host,



#endif