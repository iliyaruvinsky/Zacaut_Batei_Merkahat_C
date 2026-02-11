// TreatmentTypes.h
//
// Written 25 August 2003 by Don Radlauer
//
// Contains typedefs for Treatment Types used in tables
// doctor_percents, special_prescs, and drug_extension.


#ifndef TREATMENT_TYPES_H

	#define TREATMENT_TYPES_H

	#define TREATMENT_TYPE_NOCHECK		0
	#define TREATMENT_TYPE_GENERAL		1
	#define TREATMENT_TYPE_FERTILITY	2
	#define TREATMENT_TYPE_DIALYSIS		3
	// Marianna 11May2025: User Story #410137
	#define TREATMENT_TYPE_PREP			20
	#define TREATMENT_TYPE_SMOKING		22
	#define TREATMENT_TYPE_MAX			23	// Adjust as Treatment Types are added!

	#define TREAT_TYPE_AS400_FERTILITY	1
	#define TREAT_TYPE_AS400_DIALYSIS	2

	// Marianna 11May2025: User Story #410137
	#define TREAT_TYPE_AS400_PREP		20
	#define TREAT_TYPE_AS400_SMOKING	22


#endif // TREATMENT_TYPES_H
