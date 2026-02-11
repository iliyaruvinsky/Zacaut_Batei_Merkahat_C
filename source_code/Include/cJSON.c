/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* cJSON */
/* JSON parser in C. */

// Having both "#pragma once" and "ifndef" should be redundant, but harmless.
#pragma once
#ifndef cJSON_C
#define cJSON_C

/* disable warnings about old C89 functions in MSVC */
#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
/* disable warning about single line comments in system headers */
#pragma warning (disable : 4001)
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>

#ifdef ENABLE_LOCALES
#include <locale.h>
#endif

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#include "cJSON.h"

/* define our own boolean type */
// DonR 10Apr2022: Note that at the end of cJSON.c, we now #undefine true and false;
// this is necessary to avoid compilation errors, since other code uses true and false
// as enum variables.
#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)


/* define isnan and isinf for ANSI C, if in C99 or above, isnan and isinf has been defined in math.h */
#ifndef isinf
#define isinf(d) (isnan((d - d)) && !isnan(d))
#endif
#ifndef isnan
#define isnan(d) (d != d)
#endif

#ifndef NAN
#ifdef _WIN32
#define NAN sqrt(-1.0)
#else
#define NAN 0.0/0.0
#endif
#endif

typedef struct {
    const unsigned char *json;
    size_t position;
} error;
static error global_error = { NULL, 0 };

// DonR 14Jun2021: Added prototype so get_array_item() can be called from code that
// comes before its definition.
static cJSON* get_array_item(const cJSON *array, size_t index);


CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void)
{
    return (const char*) (global_error.json + global_error.position);
}

CJSON_PUBLIC(char *) cJSON_GetStringValue(const cJSON * const item) 
{
    if (!cJSON_IsString(item)) 
    {
        return NULL;
    }

    return item->valuestring;
}

CJSON_PUBLIC(double) cJSON_GetNumberValue(const cJSON * const item) 
{
    if (!cJSON_IsNumber(item)) 
    {
        return (double) NAN;
    }

    return item->valuedouble;
}

// DonR 18Apr2021: Add separate Get functions for various forms of integer.
CJSON_PUBLIC(int) cJSON_GetIntValue (const cJSON * const item, int *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsInt (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (int)item->valueint;
	}

    return Global_JSON_Error;
}

CJSON_PUBLIC(int) cJSON_GetUnsignedIntValue (const cJSON * const item, unsigned int *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsUnsignedInt (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (unsigned int)item->valueint;
	}

    return Global_JSON_Error;
 }

CJSON_PUBLIC(int) cJSON_GetShortValue (const cJSON * const item, short *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsShort (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (short)item->valueint;
	}

    return Global_JSON_Error;
}

CJSON_PUBLIC(int) cJSON_GetUnsignedShortValue (const cJSON * const item, unsigned short *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsUnsignedShort (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (unsigned short)item->valueint;
	}

    return Global_JSON_Error;
}

CJSON_PUBLIC(int) cJSON_GetLongValue (const cJSON * const item, long *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsLong (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (long)item->valueint;	// Typecast should be redundant for this one.
	}

    return Global_JSON_Error;
}

CJSON_PUBLIC(int) cJSON_GetUnsignedLongValue (const cJSON * const item, unsigned long *ValueReturned)
{
	if (item == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (!cJSON_IsUnsignedLong (item)) 
    {
        Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
    }
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{
		Global_JSON_Error = JSON_NO_ERROR;
		*ValueReturned = (unsigned long)item->valueint;
	}

    return Global_JSON_Error;
}


// DonR 19Apr2021: Add a set of routines to retrieve values by name from a parent object.
CJSON_PUBLIC(int) cJSON_GetStringByName (const cJSON * const parent, char * tag, const int MaxLength, const cJSON_bool IsMandatory, char *ValueReturned)
{
	cJSON	*child			= NULL;
	int		SourceLength;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else if (MaxLength < 0)	// Length == 0 is valid, although its use would be a bit odd.
	{
		Global_JSON_Error = JSON_MAX_LENGTH_NEGATIVE;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just set the return value to "".
			if (IsMandatory)
			{
				Global_JSON_Error = JSON_TAG_NOT_FOUND;
			}
			else
			{
				*ValueReturned = (char)0;
				Global_JSON_Error = JSON_NO_DATA_NOT_MANDATORY;
			}
		}
		else
		{
			if (!cJSON_IsString (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the string, using the lesser of the source string length or the maximum length of the destination.
				SourceLength = strlen (child->valuestring);
				if (SourceLength > MaxLength)
					SourceLength = MaxLength;

				memcpy (ValueReturned, child->valuestring, SourceLength);
				ValueReturned [SourceLength] = (char)0;
			}	// Child is in fact a string.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetCharByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, char *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsString (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the first byte of the string.
				*ValueReturned = *child->valuestring;
			}	// Child is in fact a string.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetIntByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, int *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsInt (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (int)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetUintByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned int *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsUnsignedInt (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (unsigned int)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetShortByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, short *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsShort (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (short)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetUshortByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned short *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsUnsignedShort (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (unsigned short)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetLongByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, long *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsLong (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (long)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetUlongByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned long *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsUnsignedLong (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the integer value.
				*ValueReturned = (unsigned long)child->valueint;
			}	// Child is in fact the right kind of integer.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetFloatByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, float *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsNumber (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the double value.
				*ValueReturned = (float)child->valuedouble;
			}	// Child is in fact any kind of number.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetDoubleByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, double *ValueReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ValueReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsNumber (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the double value.
				*ValueReturned = (double)child->valuedouble;
			}	// Child is in fact any kind of number.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


CJSON_PUBLIC(int) cJSON_GetArrayByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, cJSON **ArrayPointerReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ArrayPointerReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			if (!cJSON_IsArray (child))
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}
			else
			{
				// Copy the array's address.
				*ArrayPointerReturned = child;
			}	// Child is in fact an array.
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


// Look for a "container" object - don't bother validating its type.
CJSON_PUBLIC(int) cJSON_GetObjectByName (const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, cJSON **ObjectPointerReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error	= JSON_NO_ERROR;
	Global_JSON_Tag		= tag;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((tag == NULL) || (*tag == (char)0))
	{
		Global_JSON_Error = JSON_TAG_NOT_SUPPLIED;
	}
	else if (ObjectPointerReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = cJSON_GetObjectItemCaseSensitive (parent, tag);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			// Copy the object's address.
			*ObjectPointerReturned = child;
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}


// Look for a "container" object - don't bother validating its type.
CJSON_PUBLIC(int) cJSON_GetArrayItemByNumber (const cJSON * const parent, const int ItemNumber, const cJSON_bool IsMandatory, cJSON	**ObjectPointerReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error = JSON_NO_ERROR;

	if (parent == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (ObjectPointerReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = get_array_item (parent, ItemNumber);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = (IsMandatory) ? JSON_TAG_NOT_FOUND : JSON_NO_DATA_NOT_MANDATORY;
		}
		else
		{
			// Copy the object's address.
			*ObjectPointerReturned = child;
		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}

// DonR 21Jul2025: Add a couple of new functions to streamline access to simple integer arrays.
// cJSON_GetIntegerArrayItem() gets a single integer from an integer array by Item Number.
CJSON_PUBLIC(int) cJSON_GetIntegerArrayItem (const cJSON * const array, const int ItemNumber, int *IntegerReturned)
{
	cJSON	*child = NULL;

	Global_JSON_Error = JSON_NO_ERROR;

	if (array == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if (IntegerReturned == NULL)
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		child = get_array_item (array, ItemNumber);
		if (child == NULL)
		{
			// If the tag is non-mandatory and is not found, just leave the return value
			// alone - the calling routine is responsible for setting default values.
			// DonR 06Jun2021: If the tag is non-mandatory, set a new "error" flag so that
			// the calling routine can know that no value was set.
			Global_JSON_Error = JSON_TAG_NOT_FOUND;
		}
		else
		{
			if ((child->IntegerType & cJSON_signed_int) == cJSON_signed_int)
			{
				// All good - copy the integer to the value-return pointer.
				*IntegerReturned = (int)child->valueint;
			}
			else
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
			}

		}	// Child tag was found.
	}	// Input parameters were OK.

	return Global_JSON_Error;
}

// cJSON_GetIntegerArray copies the entire contents of a JSON integer array into a C integer array.
// Note that if MaxSize elements are loaded, the function will stop but will NOT return an error! (That's
// something we might want to revisit later...)
CJSON_PUBLIC(int) cJSON_GetIntegerArray (const cJSON * const array, const int MaxSize, int *IntegerArrayPtr, int *SizeOut)
{
	cJSON	*current_child	= NULL;
	int		NumFound		= 0;

	Global_JSON_Error = JSON_NO_ERROR;

	if (array == NULL)
	{
		Global_JSON_Error = JSON_PARENT_NULL;
	}
	else if ((IntegerArrayPtr == NULL) || (SizeOut == NULL))
	{
		Global_JSON_Error = JSON_MISSING_VALUE_POINTER;
	}
	else if (MaxSize < 1)
	{
		Global_JSON_Error = JSON_MAX_LENGTH_NEGATIVE;
	}
	else
	{	// Input parameters were OK, so now we can actually try to do something.
		// NOTE: We check each array item for integer-hood. This is perhaps
		// slightly inefficient, but most of the obvious solutions I can think
		// of would be even worse. Note also that an empty array *is* valid!
	    current_child = array->child;

		// If we've reached the maximum size of our output array, stop looping
		// but do *not* return an error - at least for now.
		while ((current_child != NULL) && (NumFound < MaxSize))
	    {
			if ((current_child->IntegerType & cJSON_signed_int) == cJSON_signed_int)
			{
				// All good - copy the integer to the value-return pointer.
				IntegerArrayPtr[NumFound++] = (int)current_child->valueint;
			}
			else
			{
				Global_JSON_Error = JSON_VALUE_TYPE_MISMATCH;
				break;
			}

	        current_child = current_child->next;
	    }
	}	// Input parameters were OK.

	*SizeOut = NumFound;

	return Global_JSON_Error;
}
// DonR 21Jul2025 end.


/* This is a safeguard to prevent copy-pasters from using incompatible C and header files */
#if (CJSON_VERSION_MAJOR != 1) || (CJSON_VERSION_MINOR != 7) || (CJSON_VERSION_PATCH != 14)
    #error cJSON.h and cJSON.c have different versions. Make sure that both have the same.
#endif

CJSON_PUBLIC(const char*) cJSON_Version(void)
{
    static char version[15];
    sprintf(version, "%i.%i.%i", CJSON_VERSION_MAJOR, CJSON_VERSION_MINOR, CJSON_VERSION_PATCH);

    return version;
}

/* Case insensitive string comparison, doesn't consider two NULL pointers equal though */
static int case_insensitive_strcmp(const unsigned char *string1, const unsigned char *string2)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

typedef struct internal_hooks
{
    void *(CJSON_CDECL *allocate)(size_t size);
    void (CJSON_CDECL *deallocate)(void *pointer);
    void *(CJSON_CDECL *reallocate)(void *pointer, size_t size);
} internal_hooks;

#if defined(_MSC_VER)
/* work around MSVC error C2322: '...' address of dllimport '...' is not static */
static void * CJSON_CDECL internal_malloc(size_t size)
{
    return malloc(size);
}
static void CJSON_CDECL internal_free(void *pointer)
{
    free(pointer);
}
static void * CJSON_CDECL internal_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}
#else
#define internal_malloc malloc
#define internal_free free
#define internal_realloc realloc
#endif

/* strlen of character literals resolved at compile time */
#define static_strlen(string_literal) (sizeof(string_literal) - sizeof(""))

static internal_hooks global_hooks = { internal_malloc, internal_free, internal_realloc };

static unsigned char* cJSON_strdup(const unsigned char* string, const internal_hooks * const hooks)
{
    size_t length = 0;
    unsigned char *copy = NULL;
	unsigned char *UTF8_string;
	unsigned char *source;

    if (string == NULL)
    {
        return NULL;
    }

	// DonR 22Nov2021: If the new global "translate Hebrew to UTF-8" switch is set ON,
	// get the translation and store that instead of the original Win-1255 text.
	if (Global_JSON_TranslateWin1255ToUTF8)
	{
		WinHebToUTF8 ((unsigned char *)string, &UTF8_string, &length);
		length++;	// So we allocate including room for the final null character.
		source = UTF8_string;
	}
	else
	{
		length = strlen((const char*)string) + sizeof("");
		source = (unsigned char *)string;
	}

    copy = (unsigned char*)hooks->allocate(length);
    if (copy == NULL)
    {
        return NULL;
    }
//  memcpy(copy, string, length);
    memcpy(copy, source, length);

    return copy;
}

CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (hooks == NULL)
    {
        /* Reset hooks */
        global_hooks.allocate = malloc;
        global_hooks.deallocate = free;
        global_hooks.reallocate = realloc;
        return;
    }

    global_hooks.allocate = malloc;
    if (hooks->malloc_fn != NULL)
    {
        global_hooks.allocate = hooks->malloc_fn;
    }

    global_hooks.deallocate = free;
    if (hooks->free_fn != NULL)
    {
        global_hooks.deallocate = hooks->free_fn;
    }

    /* use realloc only if both free and malloc are used */
    global_hooks.reallocate = NULL;
    if ((global_hooks.allocate == malloc) && (global_hooks.deallocate == free))
    {
        global_hooks.reallocate = realloc;
    }
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(const internal_hooks * const hooks)
{
    cJSON* node = (cJSON*)hooks->allocate(sizeof(cJSON));
    if (node)
    {
        memset(node, '\0', sizeof(cJSON));
    }

    return node;
}

/* Delete a cJSON structure. */
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{
    cJSON *next = NULL;
    while (item != NULL)
    {
        next = item->next;
        if (!(item->type & cJSON_IsReference) && (item->child != NULL))
        {
            cJSON_Delete(item->child);
        }
        if (!(item->type & cJSON_IsReference) && (item->valuestring != NULL))
        {
            global_hooks.deallocate(item->valuestring);
        }
        if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
        {
            global_hooks.deallocate(item->string);
        }
        global_hooks.deallocate(item);
        item = next;
    }
}

/* get the decimal point character of the current locale */
static unsigned char get_decimal_point(void)
{
#ifdef ENABLE_LOCALES
    struct lconv *lconv = localeconv();
    return (unsigned char) lconv->decimal_point[0];
#else
    return '.';
#endif
}

typedef struct
{
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth; /* How deeply nested (in arrays/objects) is the input at the current offset. */
    internal_hooks hooks;
} parse_buffer;

/* check if the given size is left to read in a given parse buffer (starting with 1) */
#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
/* check if the buffer can be accessed at the given index (starting with 0) */
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
/* get a pointer to the buffer at the position */
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

/* Parse the input text to generate a number, and populate the result into item. */
// DonR 19Apr2021: Added logic to set specific statuses for integers.
static cJSON_bool parse_number(cJSON * const item, parse_buffer * const input_buffer)
{
    double			number					= 0;
    unsigned char	*after_end				= NULL;
    unsigned char	number_c_string[64];
    unsigned char	decimal_point			= get_decimal_point();
    size_t			i						= 0;
	cJSON_bool		FoundDecimalPoint		= 0;
	cJSON_bool		SomethingAfterDecimal	= 0;

    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false;
    }

    /* copy the number into a temporary buffer and replace '.' with the decimal point
     * of the current locale (for strtod)
     * This also takes care of '\0' not necessarily being available for marking the end of the input */
    for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++)
    {
        switch (buffer_at_offset(input_buffer)[i])
        {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
				// DonR 19Apr2021: If we see a digit other than zero after the decimal point,
				// we know that we're dealing with a non-integer. We'll still store the value
				// in item->valueint, but we'll leave the new integer-found flags FALSE.
				// NOTE: There is no "break" here!
				if (FoundDecimalPoint)
					SomethingAfterDecimal = 1;

            case '0':
            case '+':
            case '-':
            case 'e':
            case 'E':
                number_c_string[i] = buffer_at_offset(input_buffer)[i];
                break;

            case '.':
				FoundDecimalPoint	= 1;
                number_c_string[i]	= decimal_point;
                break;

            default:
                goto loop_end;
        }
    }
loop_end:
    number_c_string[i] = '\0';

    number = strtod((const char*)number_c_string, (char**)&after_end);
    if (number_c_string == after_end)
    {
        return false; /* parse_error */
    }

    item->valuedouble = number;

    /* use saturation in case of overflow */
	// DonR 19Apr2021: Change to handle 64-byte integers. Note that at least for now, we
	// are *not* supporting unsigned long integers with values above LONG_MAX. This is
	// unlikely to be a real-world issue, as LONG_MAX is pretty damn' big.
    if (number >= LONG_MAX)
    {
        item->valueint = LONG_MAX;
    }
    else if (number <= (double)LONG_MIN)
    {
        item->valueint = LONG_MIN;
    }
    else
    {
        item->valueint = (long)number;
    }

    item->type = cJSON_Number;

	// DonR 19Apr2021: Set specific integer flags based on presence (or absence, rather)
	// of decimal data and the size of the number parsed.
	item->IntegerType = 0;	// Redundant re-initialization - the whole structure is set to zeroes on creation.

	if (!SomethingAfterDecimal)
	{
		item->IntegerType = item->IntegerType | cJSON_signed_long;	// All integers qualify as a long - we're not worrying about huge unsigned long values.

		if (item->valueint < 0)
		{
			if (item->valueint >= INT_MIN)
			{
				item->IntegerType = item->IntegerType | cJSON_signed_int;

				if (item->valueint >= SHRT_MIN)
				{
					item->IntegerType = item->IntegerType | cJSON_signed_short;
				}	// Qualified negative short integer.
			}	// Qualified negative integer.
		}	// Negative numbers.
		else
		{	// Natural numbers (including zero).
			item->IntegerType = item->IntegerType | cJSON_unsigned_long;	// Any integer >= 0 qualifies as an unsigned long.

			if (item->valueint <= UINT_MAX)
			{
				item->IntegerType = item->IntegerType | cJSON_unsigned_int;

				if (item->valueint <= INT_MAX)
				{
					item->IntegerType = item->IntegerType | cJSON_signed_int;

					if (item->valueint <= USHRT_MAX)
					{
						item->IntegerType = item->IntegerType | cJSON_unsigned_short;

						if (item->valueint <= SHRT_MAX)
						{
							item->IntegerType = item->IntegerType | cJSON_signed_short;
						}	// Qualified signed short integer.
					}	// Qualified unsigned short integer.
				}	// Qualified signed integer.
			}	// Qualified unsigned integer.
		}	// item->valueint >= 0.
	}	// Number does not have any meaningful digits after the decimal.


    input_buffer->offset += (size_t)(after_end - number_c_string);
    return true;
}

/* don't ask me, but the original cJSON_SetNumberValue returns an integer or double */
CJSON_PUBLIC(double) cJSON_SetNumberHelper(cJSON *object, double number)
{
	double DecimalPortion;

    if (number >= LONG_MAX)
    {
        object->valueint = LONG_MAX;
    }
    else if (number <= (double)LONG_MIN)
    {
        object->valueint = LONG_MIN;
    }
    else
    {
        object->valueint = (long)number;
    }

	// DonR 19Apr2021: Set specific integer flags based on presence (or absence, rather)
	// of decimal data and the size of the number parsed. Since we're not parsing from
	// text here, we'll decide whether the number qualifies as an integer based on a
	// "reasonable tolerance" test of 1/10,000.
	// DonR 24Nov2025: We need to be more sensitive to decimal-ness, since we now have
	// to deal with opioid drugs with ingredients in micrograms that are reported to
	// the Ministry of Health in grams. If the number of micrograms is 100 or more,
	// we were already OK - but dosages of < 100 micrograms were being reported as
	// zero grams. So the new "reasonable tolerance" test will be 1,000 times more
	// sensitive - 1 in 10,000,000!
	DecimalPortion = number - (double)object->valueint;
	object->IntegerType = 0;	// Redundant re-initialization - the whole structure is set to zeroes on creation.

//	if ((DecimalPortion > -0.0001) && (DecimalPortion < 0.0001))
	if ((DecimalPortion > -0.0000001) && (DecimalPortion < 0.0000001))	// 1/10,000,000
	{
		object->IntegerType = object->IntegerType | cJSON_signed_long;	// All integers qualify as a long - we're not worrying about huge unsigned long values.

		if (object->valueint < 0)
		{
			if (object->valueint >= INT_MIN)
			{
				object->IntegerType = object->IntegerType | cJSON_signed_int;

				if (object->valueint >= SHRT_MIN)
				{
					object->IntegerType = object->IntegerType | cJSON_signed_short;
				}	// Qualified negative short integer.
			}	// Qualified negative integer.
		}	// Negative numbers.
		else
		{	// Natural numbers (including zero).
			object->IntegerType = object->IntegerType | cJSON_unsigned_long;	// Any integer >= 0 qualifies as an unsigned long.

			if (object->valueint <= UINT_MAX)
			{
				object->IntegerType = object->IntegerType | cJSON_unsigned_int;

				if (object->valueint <= INT_MAX)
				{
					object->IntegerType = object->IntegerType | cJSON_signed_int;

					if (object->valueint <= USHRT_MAX)
					{
						object->IntegerType = object->IntegerType | cJSON_unsigned_short;

						if (object->valueint <= SHRT_MAX)
						{
							object->IntegerType = object->IntegerType | cJSON_signed_short;
						}	// Qualified signed short integer.
					}	// Qualified unsigned short integer.
				}	// Qualified signed integer.
			}	// Qualified unsigned integer.
		}	// item->valueint >= 0.
	}	// Number does not have any meaningful digits after the decimal.

    return object->valuedouble = number;
}

CJSON_PUBLIC(char*) cJSON_SetValuestring(cJSON *object, const char *valuestring)
{
    char *copy = NULL;
    /* if object's type is not cJSON_String or is cJSON_IsReference, it should not set valuestring */
    if (!(object->type & cJSON_String) || (object->type & cJSON_IsReference))
    {
        return NULL;
    }
    if (strlen(valuestring) <= strlen(object->valuestring))
    {
        strcpy(object->valuestring, valuestring);
        return object->valuestring;
    }
    copy = (char*) cJSON_strdup((const unsigned char*)valuestring, &global_hooks);
    if (copy == NULL)
    {
        return NULL;
    }
    if (object->valuestring != NULL)
    {
        cJSON_free(object->valuestring);
    }
    object->valuestring = copy;

    return copy;
}

typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth; /* current nesting depth (for formatted printing) */
    cJSON_bool noalloc;
    cJSON_bool format; /* is this print a formatted print */
    internal_hooks hooks;
} printbuffer;

/* realloc printbuffer if necessary to have at least "needed" bytes more */
static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        /* make sure that offset is valid */
        return NULL;
    }

    if (needed > INT_MAX)
    {
        /* sizes bigger than INT_MAX are currently not supported */
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    /* calculate new buffer size */
    if (needed > (INT_MAX / 2))
    {
        /* overflow of int, use INT_MAX if possible */
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }

    if (p->hooks.reallocate != NULL)
    {
        /* reallocate with realloc if available */
        newbuffer = (unsigned char*)p->hooks.reallocate(p->buffer, newsize);
        if (newbuffer == NULL)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }
    }
    else
    {
        /* otherwise reallocate manually */
        newbuffer = (unsigned char*)p->hooks.allocate(newsize);
        if (!newbuffer)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }
        
        memcpy(newbuffer, p->buffer, p->offset + 1);
        p->hooks.deallocate(p->buffer);
    }
    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

/* calculate the new length of the string in a printbuffer and update the offset */
static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/* Render the number nicely from the given item into a string. */
static cJSON_bool print_number(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    double d = item->valuedouble;
    int length = 0;
    size_t i = 0;
    unsigned char number_buffer[26] = {0}; /* temporary buffer to print the number into */
    unsigned char decimal_point = get_decimal_point();
    double test = 0.0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* This checks for NaN and Infinity */
    if (isnan(d) || isinf(d))
    {
        length = sprintf((char*)number_buffer, "null");
    }
    else
    {
        /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
        length = sprintf((char*)number_buffer, "%1.15g", d);

        /* Check whether the original double can be recovered */
        if ((sscanf((char*)number_buffer, "%lg", &test) != 1) || !compare_double((double)test, d))
        {
            /* If not, print with 17 decimal places of precision */
            length = sprintf((char*)number_buffer, "%1.17g", d);
        }
    }

    /* sprintf failed or buffer overrun occurred */
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return false;
    }

    /* reserve appropriate space in the output */
    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return false;
    }

    /* copy the printed number to the output and replace locale
     * dependent decimal point with '.' */
    for (i = 0; i < ((size_t)length); i++)
    {
        if (number_buffer[i] == decimal_point)
        {
            output_pointer[i] = '.';
            continue;
        }

        output_pointer[i] = number_buffer[i];
    }
    output_pointer[i] = '\0';

    output_buffer->offset += (size_t)length;

    return true;
}

/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a';
        }
        else /* invalid */
        {
            return 0;
        }

        if (i < 3)
        {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}

/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        /* input ends unexpectedly */
        goto fail;
    }

    /* get the first utf16 sequence */
    first_code = parse_hex4(first_sequence + 2);

    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    /* UTF16 surrogate pair */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12; /* \uXXXX\uXXXX */

        if ((input_end - second_sequence) < 6)
        {
            /* input ends unexpectedly */
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            /* missing second half of the surrogate pair */
            goto fail;
        }

        /* get the second utf16 sequence */
        second_code = parse_hex4(second_sequence + 2);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            /* invalid second half of the surrogate pair */
            goto fail;
        }


        /* calculate the unicode codepoint from the surrogate pair */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6; /* \uXXXX */
        codepoint = first_code;
    }

    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80)
    {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    }
    else if (codepoint < 0x10000)
    {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    }
    else if (codepoint <= 0x10FFFF)
    {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    }
    else
    {
        /* invalid unicode codepoint */
        goto fail;
    }

    /* encode as utf8 */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 10xxxxxx */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}

/* Parse the input text into an unescaped cinput, and populate item. */
static cJSON_bool parse_string(cJSON * const item, parse_buffer * const input_buffer)
{
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;

    /* not a string */
    if (buffer_at_offset(input_buffer)[0] != '\"')
    {
        goto fail;
    }

    {
        /* calculate approximate size of the output (overestimate) */
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;
        while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            /* is escape sequence */
            if (input_end[0] == '\\')
            {
                if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length)
                {
                    /* prevent buffer overflow when last input character is a backslash */
                    goto fail;
                }
                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
        {
            goto fail; /* string ended unexpectedly */
        }

        /* This is at most how much we need for the output */
        allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = (unsigned char*)input_buffer->hooks.allocate(allocation_length + sizeof(""));
        if (output == NULL)
        {
            goto fail; /* allocation failure */
        }
    }

    output_pointer = output;
    /* loop through the string literal */
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\')
        {
            *output_pointer++ = *input_pointer++;
        }
        /* escape sequence */
        else
        {
            unsigned char sequence_length = 2;
            if ((input_end - input_pointer) < 1)
            {
                goto fail;
            }

            switch (input_pointer[1])
            {
                case 'b':
                    *output_pointer++ = '\b';
                    break;
                case 'f':
                    *output_pointer++ = '\f';
                    break;
                case 'n':
                    *output_pointer++ = '\n';
                    break;
                case 'r':
                    *output_pointer++ = '\r';
                    break;
                case 't':
                    *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/':
                    *output_pointer++ = input_pointer[1];
                    break;

                /* UTF-16 literal */
                case 'u':
                    sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                    if (sequence_length == 0)
                    {
                        /* failed to convert UTF16-literal to UTF-8 */
                        goto fail;
                    }
                    break;

                default:
                    goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    /* zero terminate the output */
    *output_pointer = '\0';

    item->type = cJSON_String;
    item->valuestring = (char*)output;

    input_buffer->offset = (size_t) (input_end - input_buffer->content);
    input_buffer->offset++;

    return true;

fail:
    if (output != NULL)
    {
        input_buffer->hooks.deallocate(output);
    }

    if (input_pointer != NULL)
    {
        input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    }

    return false;
}

/* Render the cstring provided to an escaped version that can be printed. */
static cJSON_bool print_string_ptr(const unsigned char * const input, printbuffer * const output_buffer)
{
    const unsigned char *input_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* empty string */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return false;
        }
        strcpy((char*)output, "\"\"");

        return true;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                /* one character escape sequence */
                escape_characters++;
                break;
            default:
                if (*input_pointer < 32)
                {
                    /* UTF-16 escape sequence uXXXX */
                    escape_characters += 5;
                }
                break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return false;
    }

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return true;
    }

    output[0] = '\"';
    output_pointer = output + 1;
    /* copy the string */
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return true;
}


// Render the cstring provided to an escaped version and print it to file.
static cJSON_bool PrintStringPtrToFile (char * StringToPrint, FILE *fp)
{
	if (fp == NULL)
		return (-1);

	if (StringToPrint == NULL)
	{
		// Treat NULL as a zero-length string.
		fprintf (fp, "\"\"");
	}
	else
	{	// Input string is NOT NULL.
		unsigned char	*input					= (unsigned char *)StringToPrint;
		unsigned char	*input_pointer;
		cJSON_bool		FoundEscapeCharacter	= 0;

		for (input_pointer = input; *input_pointer; input_pointer++)
		{
			if ((strchr ("\"\\\b\f\n\r\t", *input_pointer)) || (*input_pointer < 32))
			{
				FoundEscapeCharacter = true;
				break;
			}
		}	// Loop through string character-by-character.

		if (!FoundEscapeCharacter)
		{
			// No escape characters, so just execute a simple fprintf.
			fprintf (fp, "\"%s\"", StringToPrint);
		}
		else
		{
			char	OutBuf [501];	// = 100 input characters "maximally escaped" plus closing NULL character.
			char	*output_pointer;
			int		InputCharsRead		= 0;

			fprintf (fp, "\"");	// Open quote.

			for (input_pointer = input, output_pointer = &OutBuf[0]; *input_pointer; input_pointer++, InputCharsRead++)
			{
				if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
				{
					// Normal character - just copy it.
					*output_pointer++ = *input_pointer;
				}
				else
				{	// Character needs to be escaped.
					*output_pointer++ = '\\';

					switch (*input_pointer)
					{
						case '\\':
							*output_pointer++ = '\\';
							break;
						case '\"':
							*output_pointer++ = '\"';
							break;
						case '\b':
							*output_pointer++ = 'b';
							break;
						case '\f':
							*output_pointer++ = 'f';
							break;
						case '\n':
							*output_pointer++ = 'n';
							break;
						case '\r':
							*output_pointer++ = 'r';
							break;
						case '\t':
							*output_pointer++ = 't';
							break;

						default:
							/* escape and print as unicode codepoint */
							sprintf((char*)output_pointer, "u%04x", *input_pointer);
							output_pointer += 4;
							break;
					}	// Switch on character that needs to be escaped.
				}	// Current character in input buffer needs to be escaped.

				// To run faster, write to file in chunks of 100 input characters (which will
				// be longer to the extent that we've got escape characters - thus the 501-
				// character output buffer).
				if (InputCharsRead >= 100)	// Should never get beyond 100!
				{
					*output_pointer = (char)0;

					fprintf (fp, OutBuf);

					// Reset for next chunk of input/output.
					InputCharsRead = 0;
					output_pointer = &OutBuf[0];
				}

			}	// Loop through input buffer copying and escaping characters as necessary.

			// Output last chunk.
			if (InputCharsRead > 0)
			{
				*output_pointer = (char)0;

				fprintf (fp, OutBuf);
			}


			fprintf (fp, "\"");	// Close quote.
		}	// Found at least one character that needs to be escaped.
	}	// item->valuestring is NOT NULL.

	return 0;
}




/* Invoke print_string_ptr (which is useful) on an item. */
static cJSON_bool print_string(const cJSON * const item, printbuffer * const p)
{
    return print_string_ptr((unsigned char*)item->valuestring, p);
}

/* Predeclare these prototypes. */
static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer);
static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer);
static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer);

/* Utility to jump whitespace and cr/lf */
static parse_buffer *buffer_skip_whitespace(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL))
    {
        return NULL;
    }

    if (cannot_access_at_index(buffer, 0))
    {
        return buffer;
    }

    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
    {
       buffer->offset++;
    }

    if (buffer->offset == buffer->length)
    {
        buffer->offset--;
    }

    return buffer;
}

/* skip the UTF-8 BOM (byte order mark) if it is at the beginning of a buffer */
static parse_buffer *skip_utf8_bom(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0))
    {
        return NULL;
    }

    if (can_access_at_index(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0))
    {
        buffer->offset += 3;
    }

    return buffer;
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    size_t buffer_length;

    if (NULL == value)
    {
        return NULL;
    }

    /* Adding null character size due to require_null_terminated. */
    buffer_length = strlen(value) + sizeof("");

    return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_null_terminated);
}

/* Parse an object - create a new root, and populate. */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    parse_buffer buffer = { 0, 0, 0, 0, { 0, 0, 0 } };
    cJSON *item = NULL;

    /* reset error position */
    global_error.json = NULL;
    global_error.position = 0;

    if (value == NULL || 0 == buffer_length)
    {
        goto fail;
    }

    buffer.content = (const unsigned char*)value;
    buffer.length = buffer_length; 
    buffer.offset = 0;
    buffer.hooks = global_hooks;

    item = cJSON_New_Item(&global_hooks);
    if (item == NULL) /* memory fail */
    {
        goto fail;
    }

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        /* parse failure. ep is set. */
        goto fail;
    }

    /* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
    if (require_null_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
        {
            goto fail;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)buffer_at_offset(&buffer);
    }

    return item;

fail:
    if (item != NULL)
    {
        cJSON_Delete(item);
    }

    if (value != NULL)
    {
        error local_error;
        local_error.json = (const unsigned char*)value;
        local_error.position = 0;

        if (buffer.offset < buffer.length)
        {
            local_error.position = buffer.offset;
        }
        else if (buffer.length > 0)
        {
            local_error.position = buffer.length - 1;
        }

        if (return_parse_end != NULL)
        {
            *return_parse_end = (const char*)local_error.json + local_error.position;
        }

        global_error = local_error;
    }

    return NULL;
}

/* Default options for cJSON_Parse */
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
    return cJSON_ParseWithOpts(value, 0, 0);
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithLength(const char *value, size_t buffer_length)
{
    return cJSON_ParseWithLengthOpts(value, buffer_length, 0, 0);
}

#define cjson_min(a, b) (((a) < (b)) ? (a) : (b))

static unsigned char *print(const cJSON * const item, cJSON_bool format, const internal_hooks * const hooks)
{
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    unsigned char *printed = NULL;

    memset(buffer, 0, sizeof(buffer));

    /* create buffer */
    buffer->buffer = (unsigned char*) hooks->allocate(default_buffer_size);
    buffer->length = default_buffer_size;
    buffer->format = format;
    buffer->hooks = *hooks;
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    /* print the value */
    if (!print_value(item, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    /* check if reallocate is available */
    if (hooks->reallocate != NULL)
    {
        printed = (unsigned char*) hooks->reallocate(buffer->buffer, buffer->offset + 1);
        if (printed == NULL) {
            goto fail;
        }
        buffer->buffer = NULL;
    }
    else /* otherwise copy the JSON over to a new buffer */
    {
        printed = (unsigned char*) hooks->allocate(buffer->offset + 1);
        if (printed == NULL)
        {
            goto fail;
        }
        memcpy(printed, buffer->buffer, cjson_min(buffer->length, buffer->offset + 1));
        printed[buffer->offset] = '\0'; /* just to be sure */

        /* free the buffer */
        hooks->deallocate(buffer->buffer);
    }

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        hooks->deallocate(buffer->buffer);
    }

    if (printed != NULL)
    {
        hooks->deallocate(printed);
    }

    return NULL;
}

// DonR 22Apr2021: Render a cJSON entity to file - formatted version.
CJSON_PUBLIC(int) cJSON_PrintToFP(const cJSON *item, FILE *fp)
{
	if (fp == NULL)
		return -1;

	return (PrintToFP (item, fp, true, 0));
}

// DonR 22Apr2021: Render a cJSON entity to file - unformatted version.
CJSON_PUBLIC(int) cJSON_PrintUnformattedToFP(const cJSON *item, FILE *fp)
{
	if (fp == NULL)
		return -1;

	return (PrintToFP (item, fp, false, 0));
}


int PrintToFP (const cJSON *item, FILE *fp, cJSON_bool formatted, int depth)
{
	cJSON	*current_element	= NULL;
	cJSON	*current_item		= NULL;
	char	tabs [102];	// Assume that 100 is more than enough potential nesting depth.

	if (fp == NULL)
		return -1;

	if (item == NULL)
		return -2;

	switch ((item->type) & 0xFF)
	{
		case cJSON_NULL:
			fprintf (fp, "null");
			break;

		case cJSON_False:
			fprintf (fp, "false");
			break;

		case cJSON_True:
			fprintf (fp, "true");
			break;


		case cJSON_Number:
			// Integer printing is nice an simple, so try that first.
			if (((item->IntegerType & cJSON_signed_long)	== cJSON_signed_long)		||
				((item->IntegerType & cJSON_unsigned_long)	== cJSON_unsigned_long))
			{
				fprintf (fp, "%ld", item->valueint);
			}
			else
			{	// Print the double-precision value.
				double d						= item->valuedouble;

				/* This checks for NaN and Infinity */
				if (isnan(d) || isinf(d))
				{
					fprintf (fp, "null");
				}
				else
				{
					// DonR 24Nov2025: When we output very small numbers (e.g. the ingredient
					// amount expressed in grams when the actual ingredient is in micrograms)
					// the "%g" printf mask is giving us exponential notation - which we do
					// *not* want. So I'm switching the %g's to plain old %f's.

					unsigned char	number_buffer[50]	= {0}; /* temporary buffer to print the number into */
					unsigned char	decimal_point		= get_decimal_point ();
					double			test				= 0.0;
					int				length				= 0;
					size_t			i					= 0;

					// Try 15 decimal places of precision to avoid nonsignificant nonzero digits.
//					length = sprintf ((char*)number_buffer, "%1.15g", d);
					length = sprintf ((char*)number_buffer, "%1.15f", d);

					// Check whether 15 decimal places was enough.
//					if ((sscanf ((char *)number_buffer, "%lg", &test) != 1) || !compare_double ((double)test, d))
					if ((sscanf ((char *)number_buffer, "%lf", &test) != 1) || !compare_double ((double)test, d))
					{
						// If not, print with 17 decimal places of precision.
//						length = sprintf ((char *)number_buffer, "%1.17g", d);
						length = sprintf ((char *)number_buffer, "%1.17f", d);
					}

					// Replace locale-dependent decimal point with '.' (is this really necessary?)
					if (decimal_point != '.')
					{
						for (i = 0; i < (size_t)length; i++)
						{
							if (number_buffer[i] == decimal_point)
							{
								number_buffer[i] = '.';
								break;
							}
						}
					}	// Need to substitute '.' for locale-specific decimal point.

					// DonR 24Nov2025: Switching from %g to %f gives us excessive trailing zeroes -
					// so strip them off before outputting them.
					while ((length > 4) && (number_buffer[length - 1] == '0') && (number_buffer[length - 2] != '.'))
					{
						length--;
					}
					number_buffer[length] = (char)0;

					fprintf (fp, (char *)number_buffer);
				}	// Double value is a valid number.
			}	// Numeric value is not any kind of integer.

			break;	// cJSON_Number.


		case cJSON_Raw:
			// Outputs as a string with no quotes, escape characters, or formatting - just a data-dump.
			fprintf (fp, item->valuestring);
			break;


		case cJSON_String:
			// Send string output with quotes, and with special characters escaped.
			PrintStringPtrToFile (item->valuestring, fp);
			break;	// case cJSON_String.


		case cJSON_Array:
			current_element = item->child;

			// Set up tabs as item prefix, *including* a leading <CR> if we're in formatted mode.
			if (formatted)
			{
				memset (tabs, '\t', sizeof (tabs));
				tabs[0] = '\n';	// Start off with a <CR>.
				tabs[depth + 1] = (char)0;
			}
			else
			{
				tabs[0] = (char)0;
			}

			// Opening bracket.
			fprintf (fp, "%s[", tabs);

			// Now that we're inside the opening brace, increase depth by 1.
			if (depth < 100)
				depth++;
				
			// Loop through object children.
			while (current_element)
			{
				// Print the child item with a recursed call to PrintToFP().
				PrintToFP (current_element, fp, formatted, depth);

				if (current_element->next)
					fprintf (fp, ",");

				current_element = current_element->next;
			}	// Loop through children of the current object.

			// Closing bracket.
			fprintf (fp, "%s]", tabs);

			// DonR 24Nov2025: Writing excessive amounts to file without occasionally
			// flushing the buffer appears to cause lost data. Doing the flush once
			// at the end of each object and array *should* be adequate, I think...
			fflush (fp);

			// Now that we're inside the opening brace, decrease depth by 1.
			if (depth > 0)
				depth--;

			break;	// case cJSON_Array.


		case cJSON_Object:
			current_item = item->child;

			// Set up tabs as item prefix, *including* a leading <CR> if we're in formatted mode.
			if (formatted)
			{
				memset (tabs, '\t', sizeof (tabs));
				tabs[0] = '\n';	// Start off with a <CR>.
				tabs[depth + 1] = (char)0;
			}
			else
			{
				tabs[0] = (char)0;
			}

			// Opening brace.
			fprintf (fp, "%s{", tabs);

			// Now that we're inside the opening brace, increase depth by 1.
			if (depth < 100)
				depth++;
				
			// Loop through object children.
			while (current_item)
			{
				// Print key, with <CR> and leading tabs if we're in formatted mode. Note that we
				// add one tab, since "tabs" represents the depth at the beginning of the current
				// object and now we're inside that object.
				if (formatted)
					fprintf (fp, "%s\t", tabs);

				PrintStringPtrToFile (current_item->string, fp);	// current_item->string == the key (or tag, or whatever we're calling it).

				// Print colon and (if appropriate) a tab.
				// Arrays and objects start off with "[" or "{" on a new line, so skip the trailing tab on the old line.
				fprintf (fp, ":%s", ((formatted) && (current_item->type != cJSON_Array) && (current_item->type != cJSON_Object)) ? "\t" : "");

				// Now print the actual child item with a recursed call to PrintToFP().
				PrintToFP (current_item, fp, formatted, depth);

				if (current_item->next)
					fprintf (fp, ",");

				current_item = current_item->next;
			}	// Loop through children of the current object.

			// Closing brace.
			fprintf (fp, "%s}", tabs);

			// DonR 24Nov2025: Writing excessive amounts to file without occasionally
			// flushing the buffer appears to cause lost data. Doing the flush once
			// at the end of each object and array *should* be adequate, I think...
			fflush (fp);

			// Now that we're inside the opening brace, decrease depth by 1.
			if (depth > 0)
				depth--;

			break;	// case cJSON_Object.


		// We shouldn't ever see an unknown item type - but just in case...
        default:
            return -3;	// Shouldn't ever happen.
    }

	return 0;
}


/* Render a cJSON item/entity/structure to text. */
CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item)
{
    return (char*)print(item, true, &global_hooks);
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
    return (char*)print(item, false, &global_hooks);
}

// DonR 10Feb2022: Added BufferLength_out parameter. If this is non-NULL, we'll send back the allocated buffer length;
// this will allow us to make a better guess next time if the initial buffer-size guess was too low.
CJSON_PUBLIC(char *) cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt, int *BufferLength_out)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if (prebuffer < 0)
    {
        return NULL;
    }

    p.buffer = (unsigned char*)global_hooks.allocate((size_t)prebuffer);
    if (!p.buffer)
    {
        return NULL;
    }

    p.length = (size_t)prebuffer;
    p.offset = 0;
    p.noalloc = false;
    p.format = fmt;
    p.hooks = global_hooks;

    if (!print_value(item, &p))
    {
        global_hooks.deallocate(p.buffer);
        return NULL;
    }

	// DonR 10Feb2022: Tell the calling routine how big the allocated buffer is after
	// the print_value() call.
	if (BufferLength_out != NULL)
	{
		*BufferLength_out = p.length;
	}

    return (char*)p.buffer;
}

CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if ((length < 0) || (buffer == NULL))
    {
        return false;
    }

    p.buffer = (unsigned char*)buffer;
    p.length = (size_t)length;
    p.offset = 0;
    p.noalloc = true;
    p.format = format;
    p.hooks = global_hooks;

    return print_value(item, &p);
}

/* Parser core - when encountering text, process appropriately. */
static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer)
{
    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false; /* no input */
    }

    /* parse the different types of values */
    /* null */
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "null", 4) == 0))
    {
        item->type = cJSON_NULL;
        input_buffer->offset += 4;
        return true;
    }
    /* false */
	// DonR 19Apr2021: Set IntegerType = all types valid for true/false.
    if (can_read(input_buffer, 5) && (strncmp((const char*)buffer_at_offset(input_buffer), "false", 5) == 0))
    {
        item->type				=  cJSON_False;
        item->valueint			=  0;	// DonR 19Apr2021: Added just to be anal-retentive and not trust initial memset initialization.
		item->IntegerType		=  0xFFFF;	// Could really be 0x00FF, but let's be over-thorough.
        input_buffer->offset	+= 5;
        return true;
    }
    /* true */
	// DonR 19Apr2021: Set IntegerType = all types valid for true/false.
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "true", 4) == 0))
    {
        item->type				=  cJSON_True;
        item->valueint			=  1;
		item->IntegerType		=  0xFFFF;	// Could really be 0x00FF, but let's be over-thorough.
        input_buffer->offset	+= 4;
        return true;
    }
    /* string */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"'))
    {
        return parse_string(item, input_buffer);
    }
    /* number */
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9'))))
    {
        return parse_number(item, input_buffer);
    }
    /* array */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '['))
    {
        return parse_array(item, input_buffer);
    }
    /* object */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{'))
    {
        return parse_object(item, input_buffer);
    }

    return false;
}

/* Render a value to text. */
static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output = NULL;

    if ((item == NULL) || (output_buffer == NULL))
    {
        return false;
    }

    switch ((item->type) & 0xFF)
    {
        case cJSON_NULL:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "null");
            return true;

        case cJSON_False:
            output = ensure(output_buffer, 6);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "false");
            return true;

        case cJSON_True:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "true");
            return true;

        case cJSON_Number:
            return print_number(item, output_buffer);

        case cJSON_Raw:
        {
            size_t raw_length = 0;
            if (item->valuestring == NULL)
            {
                return false;
            }

            raw_length = strlen(item->valuestring) + sizeof("");
            output = ensure(output_buffer, raw_length);
            if (output == NULL)
            {
                return false;
            }
            memcpy(output, item->valuestring, raw_length);
            return true;
        }

        case cJSON_String:
            return print_string(item, output_buffer);

        case cJSON_Array:
            return print_array(item, output_buffer);

        case cJSON_Object:
            return print_object(item, output_buffer);

        default:
            return false;
    }
}

/* Build an array from input text. */
static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer)
{
    cJSON *head = NULL; /* head of the linked list */
    cJSON *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false; /* too deeply nested */
    }
    input_buffer->depth++;

    if (buffer_at_offset(input_buffer)[0] != '[')
    {
        /* not an array */
        goto fail;
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']'))
    {
        /* empty array */
        goto success;
    }

    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse next value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail; /* failed to parse value */
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
    {
        goto fail; /* expected end of array */
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = cJSON_Array;
    item->child = head;

    input_buffer->offset++;

    return true;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return false;
}

/* Render an array to text */
static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_element = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output array. */
    /* opening square bracket */
    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    while (current_element != NULL)
    {
        if (!print_value(current_element, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);
        if (current_element->next)
        {
            length = (size_t) (output_buffer->format ? 2 : 1);
            output_pointer = ensure(output_buffer, length + 1);
            if (output_pointer == NULL)
            {
                return false;
            }
            *output_pointer++ = ',';
            if(output_buffer->format)
            {
                *output_pointer++ = ' ';
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
        current_element = current_element->next;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

/* Build an object from the text. */
static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer)
{
    cJSON *head = NULL; /* linked list head */
    cJSON *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false; /* to deeply nested */
    }
    input_buffer->depth++;

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
    {
        goto fail; /* not an object */
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}'))
    {
        goto success; /* empty object */
    }

    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse the name of the child */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer))
        {
            goto fail; /* failed to parse name */
        }
        buffer_skip_whitespace(input_buffer);

        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
        {
            goto fail; /* invalid object */
        }

        /* parse the value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail; /* failed to parse value */
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
    {
        goto fail; /* expected end of object */
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = cJSON_Object;
    item->child = head;

    input_buffer->offset++;
    return true;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return false;
}

/* Render an object to text. */
static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_item = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output: */
    length = (size_t) (output_buffer->format ? 2 : 1); /* fmt: {\n */
    output_pointer = ensure(output_buffer, length + 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer++ = '{';
    output_buffer->depth++;
    if (output_buffer->format)
    {
        *output_pointer++ = '\n';
    }
    output_buffer->offset += length;

    while (current_item)
    {
        if (output_buffer->format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return false;
            }
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += output_buffer->depth;
        }

        /* print key */
        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        length = (size_t) (output_buffer->format ? 2 : 1);
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return false;
        }
        *output_pointer++ = ':';
        if (output_buffer->format)
        {
            *output_pointer++ = '\t';
        }
        output_buffer->offset += length;

        /* print value */
        if (!print_value(current_item, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        /* print comma if not last */
        length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(current_item->next ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return false;
        }
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }

        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

/* Get Array size/item / object item. */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
    cJSON *child = NULL;
    size_t size = 0;

    if (array == NULL)
    {
        return 0;
    }

    child = array->child;

    while(child != NULL)
    {
        size++;
        child = child->next;
    }

    /* FIXME: Can overflow here. Cannot be fixed without breaking the API */

    return (int)size;
}

static cJSON* get_array_item(const cJSON *array, size_t index)
{
    cJSON *current_child = NULL;

	// DonR 14Jun2021: Just to avoid complexity in calling routines, return NULL
	// if the incoming array index is less than zero.
    if ((array == NULL) || (index < 0))
    {
        return NULL;
    }

    current_child = array->child;
    while ((current_child != NULL) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index)
{
    if (index < 0)
    {
        return NULL;
    }

    return get_array_item(array, (size_t)index);
}

static cJSON *get_object_item(const cJSON * const object, const char * const name, const cJSON_bool case_sensitive)
{
    cJSON *current_element = NULL;

    if ((object == NULL) || (name == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    if (case_sensitive)
    {
        while ((current_element != NULL) && (current_element->string != NULL) && (strcmp(name, current_element->string) != 0))
        {
            current_element = current_element->next;
        }
    }
    else
    {
        while ((current_element != NULL) && (case_insensitive_strcmp((const unsigned char*)name, (const unsigned char*)(current_element->string)) != 0))
        {
            current_element = current_element->next;
        }
    }

    if ((current_element == NULL) || (current_element->string == NULL)) {
        return NULL;
    }

    return current_element;
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, false);
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev, cJSON *item)
{
    prev->next = item;
    item->prev = prev;
}

/* Utility for handling references. */
static cJSON *create_reference(const cJSON *item, const internal_hooks * const hooks)
{
    cJSON *reference = NULL;
    if (item == NULL)
    {
        return NULL;
    }

    reference = cJSON_New_Item(hooks);
    if (reference == NULL)
    {
        return NULL;
    }

    memcpy(reference, item, sizeof(cJSON));
    reference->string = NULL;
    reference->type |= cJSON_IsReference;
    reference->next = reference->prev = NULL;
    return reference;
}

static cJSON_bool add_item_to_array(cJSON *array, cJSON *item)
{
    cJSON *child = NULL;

    if ((item == NULL) || (array == NULL) || (array == item))
    {
        return false;
    }

    child = array->child;
    /*
     * To find the last item in array quickly, we use prev in array
     */
    if (child == NULL)
    {
        /* list is empty, start new one */
        array->child = item;
        item->prev = item;
        item->next = NULL;
    }
    else
    {
        /* append to the end */
        if (child->prev)
        {
            suffix_object(child->prev, item);
            array->child->prev = item;
        }
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	item->ParentPtr = array;

    return true;
}

/* Add item to array/object. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    return add_item_to_array(array, item);
}

#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic push
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
/* helper function to cast away const */
static void* cast_away_const(const void* string)
{
    return (void*)string;
}
#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic pop
#endif


static cJSON_bool add_item_to_object(cJSON * const object, const char * const string, cJSON * const item, const internal_hooks * const hooks, const cJSON_bool constant_key)
{
    char *new_key = NULL;
    int new_type = cJSON_Invalid;

    if ((object == NULL) || (string == NULL) || (item == NULL) || (object == item))
    {
        return false;
    }

    if (constant_key)
    {
        new_key = (char*)cast_away_const(string);
        new_type = item->type | cJSON_StringIsConst;
    }
    else
    {
        new_key = (char*)cJSON_strdup((const unsigned char*)string, hooks);
        if (new_key == NULL)
        {
            return false;
        }

        new_type = item->type & ~cJSON_StringIsConst;
    }

    if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
    {
        hooks->deallocate(item->string);
    }

    item->string = new_key;
    item->type = new_type;

    return add_item_to_array(object, item);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, false);
}

/* Add an item to an object with constant string as key */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)
{
    if (array == NULL)
    {
        return false;
    }

    return add_item_to_array(array, create_reference(item, &global_hooks));
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    if ((object == NULL) || (string == NULL))
    {
        return false;
    }

    return add_item_to_object(object, string, create_reference(item, &global_hooks), &global_hooks, false);
}

CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name)
{
    cJSON *null = cJSON_CreateNull();
    if (add_item_to_object(object, name, null, &global_hooks, false))
    {
        return null;
    }

    cJSON_Delete(null);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name)
{
    cJSON *true_item = cJSON_CreateTrue();
    if (add_item_to_object(object, name, true_item, &global_hooks, false))
    {
        return true_item;
    }

    cJSON_Delete(true_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name)
{
    cJSON *false_item = cJSON_CreateFalse();
    if (add_item_to_object(object, name, false_item, &global_hooks, false))
    {
        return false_item;
    }

    cJSON_Delete(false_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
{
    cJSON *bool_item = cJSON_CreateBool(boolean);
    if (add_item_to_object(object, name, bool_item, &global_hooks, false))
    {
        return bool_item;
    }

    cJSON_Delete(bool_item);
    return NULL;
}


CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
{
    cJSON *number_item = cJSON_CreateNumber(number);
    if (add_item_to_object(object, name, number_item, &global_hooks, false))
    {
        return number_item;
    }

    cJSON_Delete(number_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
    cJSON *string_item = cJSON_CreateString(string);
    if (add_item_to_object(object, name, string_item, &global_hooks, false))
    {
        return string_item;
    }

    cJSON_Delete(string_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw)
{
    cJSON *raw_item = cJSON_CreateRaw(raw);
    if (add_item_to_object(object, name, raw_item, &global_hooks, false))
    {
        return raw_item;
    }

    cJSON_Delete(raw_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
{
    cJSON *object_item = cJSON_CreateObject();
    if (add_item_to_object(object, name, object_item, &global_hooks, false))
    {
        return object_item;
    }

    cJSON_Delete(object_item);
    return NULL;
}

// DonR 22Apr2021: Add new convenience function cJSON_AddNewObjectToArray(). This combines
// the functionality of cJSON_CreateObject() and cJSON_AddItemToArray().
CJSON_PUBLIC(cJSON*) cJSON_AddNewObjectToArray (cJSON *array)
{
	cJSON *new_item = cJSON_CreateObject ();
	add_item_to_array(array, new_item);

	return (new_item);
}

CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
{
    cJSON *array = cJSON_CreateArray();
    if (add_item_to_object(object, name, array, &global_hooks, false))
    {
        return array;
    }

    cJSON_Delete(array);
    return NULL;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item)
{
    if ((parent == NULL) || (item == NULL))
    {
        return NULL;
    }

    if (item != parent->child)
    {
        /* not the first element */
        item->prev->next = item->next;
    }
    if (item->next != NULL)
    {
        /* not the last element */
        item->next->prev = item->prev;
    }

    if (item == parent->child)
    {
        /* first element */
        parent->child = item->next;
    }
    else if (item->next == NULL)
    {
        /* last element */
        parent->child->prev = item->prev;
    }

    /* make sure the detached item doesn't point anywhere anymore */
	// DonR 02Mar2022: Also clear the detached item's Parent Pointer. It's an orphan now!
    item->prev		= NULL;
    item->next		= NULL;
	item->ParentPtr	= NULL;

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which)
{
    if (which < 0)
    {
        return NULL;
    }

    return cJSON_DetachItemViaPointer(array, get_array_item(array, (size_t)which));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which)
{
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItem(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItemCaseSensitive(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObject(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObjectCaseSensitive(object, string));
}

// DonR 01Mar2022: This function inserts the new item before the previous item at
// array subscript "which", but does *not* remove any previous contents of the array.
CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *after_inserted = NULL;

    if (which < 0)
    {
        return false;
    }

    after_inserted = get_array_item(array, (size_t)which);
    if (after_inserted == NULL)
    {
        return add_item_to_array(array, newitem);
    }

    newitem->next = after_inserted;
    newitem->prev = after_inserted->prev;
    after_inserted->prev = newitem;
    if (after_inserted == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	newitem->ParentPtr = array;

    return true;
}

/* Replace array/object items with new ones. */
// DonR 01Mar2022: See above comment - and note that this function actually *does* delete
// the previous item that it replaces!
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON * const parent, cJSON * const item, cJSON * replacement)
{
    if ((parent == NULL) || (replacement == NULL) || (item == NULL))
    {
        return false;
    }

    if (replacement == item)
    {
        return true;
    }

    replacement->next = item->next;
    replacement->prev = item->prev;

    if (replacement->next != NULL)
    {
        replacement->next->prev = replacement;
    }
    if (parent->child == item)
    {
        if (parent->child->prev == parent->child)
        {
            replacement->prev = replacement;
        }
        parent->child = replacement;
    }
    else
    {   /*
         * To find the last item in array quickly, we use prev in array.
         * We can't modify the last item's next pointer where this item was the parent's child
         */
        if (replacement->prev != NULL)
        {
            replacement->prev->next = replacement;
        }
        if (replacement->next == NULL)
        {
            parent->child->prev = replacement;
        }
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	replacement->ParentPtr = parent;

    item->next = NULL;
    item->prev = NULL;
    cJSON_Delete(item);

    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem)
{
    if (which < 0)
    {
        return false;
    }

    return cJSON_ReplaceItemViaPointer(array, get_array_item(array, (size_t)which), newitem);
}

static cJSON_bool replace_item_in_object(cJSON *object, const char *string, cJSON *replacement, cJSON_bool case_sensitive)
{
    if ((replacement == NULL) || (string == NULL))
    {
        return false;
    }

    /* replace the name in the replacement */
    if (!(replacement->type & cJSON_StringIsConst) && (replacement->string != NULL))
    {
        cJSON_free(replacement->string);
    }
    replacement->string = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
    replacement->type &= ~cJSON_StringIsConst;

    return cJSON_ReplaceItemViaPointer(object, get_object_item(object, string, case_sensitive), replacement);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, false);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, true);
}

/* Create basic types: */
CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_NULL;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
		// DonR 19Apr2021: Added assignment of item->valueint (which was missing - bug?) and item->IntegerType, which is new.
        item->type				= cJSON_True;
        item->valueint			= 1;
		item->IntegerType		= 0xFFFF;	// Could really be 0x00FF, but let's be over-thorough.
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
		// DonR 19Apr2021: Added assignment of item->valueint (which was missing - bug?) and item->IntegerType, which is new.
        item->type				= cJSON_False;
        item->valueint			= 0;
		item->IntegerType		= 0xFFFF;	// Could really be 0x00FF, but let's be over-thorough.
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
		// DonR 19Apr2021: Added assignment of item->valueint (which was missing - bug?) and item->IntegerType, which is new.
        item->type				= boolean ? cJSON_True	: cJSON_False;
        item->valueint			= boolean ? 1			: 0;
		item->IntegerType		= 0xFFFF;	// Could really be 0x00FF, but let's be over-thorough.
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num)
{
	double	DecimalPortion;
    cJSON	*item = cJSON_New_Item(&global_hooks);

    if (item)
    {
        item->type = cJSON_Number;
        item->valuedouble = num;

        /* use saturation in case of overflow */
        if (num >= LONG_MAX)
        {
            item->valueint = LONG_MAX;
        }
        else if (num <= (double)LONG_MIN)
        {
            item->valueint = LONG_MIN;
        }
        else
        {
            item->valueint = (long)num;
        }

		// DonR 19Apr2021: Set specific integer flags based on presence (or absence, rather)
		// of decimal data and the size of the number parsed. Since we're not parsing from
		// text here, we'll decide whether the number qualifies as an integer based on a
		// "reasonable tolerance" test of 1/10,000.
		// DonR 24Nov2025: We need to be more sensitive to decimal-ness, since we now have
		// to deal with opioid drugs with ingredients in micrograms that are reported to
		// the Ministry of Health in grams. If the number of micrograms is 100 or more,
		// we were already OK - but dosages of < 100 micrograms were being reported as
		// zero grams. So the new "reasonable tolerance" test will be 1,000 times more
		// sensitive - 1 in 10,000,000!
		DecimalPortion = num - (double)item->valueint;
		item->IntegerType = 0;	// Redundant re-initialization - the whole structure is set to zeroes on creation.

//		if ((DecimalPortion > -0.0001) && (DecimalPortion < 0.0001))
		if ((DecimalPortion > -0.0000001) && (DecimalPortion < 0.0000001))	// 1/10,000,000
		{
			item->IntegerType = item->IntegerType | cJSON_signed_long;	// All integers qualify as a long - we're not worrying about huge unsigned long values.

			if (item->valueint < 0)
			{
				if (item->valueint >= INT_MIN)
				{
					item->IntegerType = item->IntegerType | cJSON_signed_int;

					if (item->valueint >= SHRT_MIN)
					{
						item->IntegerType = item->IntegerType | cJSON_signed_short;
					}	// Qualified negative short integer.
				}	// Qualified negative integer.
			}	// Negative numbers.
			else
			{	// Natural numbers (including zero).
				item->IntegerType = item->IntegerType | cJSON_unsigned_long;	// Any integer >= 0 qualifies as an unsigned long.

				if (item->valueint <= UINT_MAX)
				{
					item->IntegerType = item->IntegerType | cJSON_unsigned_int;

					if (item->valueint <= INT_MAX)
					{
						item->IntegerType = item->IntegerType | cJSON_signed_int;

						if (item->valueint <= USHRT_MAX)
						{
							item->IntegerType = item->IntegerType | cJSON_unsigned_short;

							if (item->valueint <= SHRT_MAX)
							{
								item->IntegerType = item->IntegerType | cJSON_signed_short;
							}	// Qualified signed short integer.
						}	// Qualified unsigned short integer.
					}	// Qualified signed integer.
				}	// Qualified unsigned integer.
			}	// item->valueint >= 0.
		}	// Number does not have any meaningful digits after the decimal.

    }	// Created item != NULL.

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringReference(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL)
    {
        item->type = cJSON_String | cJSON_IsReference;
        item->valuestring = (char*)cast_away_const(string);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObjectReference(const cJSON *child)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Object | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArrayReference(const cJSON *child) {
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateRaw(const char *raw)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_Raw;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type=cJSON_Array;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
        item->type = cJSON_Object;
    }

    return item;
}

/* Create Arrays: */
CJSON_PUBLIC(cJSON *) cJSON_CreateIntArray(const int *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	n->ParentPtr = a;

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFloatArray(const float *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber((double)numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	n->ParentPtr = a;

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateDoubleArray(const double *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	n->ParentPtr = a;

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (strings == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for (i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateString(strings[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p,n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }
 
	// DonR 02Mar2022: Store the parent item's address in the child's Parent Pointer.
	n->ParentPtr = a;
   
    return a;
}

/* Duplication */
// DonR 02Mar2022: Note that the first item duplicated (i.e. the root of the
// duplicated tree) does *not* have a parent - it hasn't (yet) been inserted
// into an existing tree. But if cJSON_Duplicate() is called recursively,
// created children *will* get a parent pointer.
// NOTE ON RECURSION: Each iteration of cJSON_Duplicate() will duplicate the
// object "item" as well as all its first-level children. If a child has
// children of its own, that will trigger a further level of recursion.
CJSON_PUBLIC(cJSON *) cJSON_Duplicate (const cJSON *item, cJSON_bool recurse)
{
    cJSON *newitem	= NULL;
    cJSON *child	= NULL;
    cJSON *next		= NULL;
    cJSON *newchild	= NULL;
	cJSON *parent	= NULL;

    /* Bail on bad ptr */
    if (!item)
    {
        goto fail;
    }
    /* Create new item */
	// DonR 02Mar2022: Save the new item address as "parent", to be assigned
	// as the Parent Pointer to all its children.
    parent = newitem = cJSON_New_Item(&global_hooks);
    if (!newitem)
    {
        goto fail;
    }
    /* Copy over all vars */
    newitem->type			= item->type & (~cJSON_IsReference);
	newitem->IntegerType	= item->IntegerType;	// DonR 19Apr2021.
    newitem->valueint		= item->valueint;
    newitem->valuedouble	= item->valuedouble;
    if (item->valuestring)
    {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring, &global_hooks);
        if (!newitem->valuestring)
        {
            goto fail;
        }
    }
    if (item->string)
    {
        newitem->string = (item->type&cJSON_StringIsConst) ? item->string : (char*)cJSON_strdup((unsigned char*)item->string, &global_hooks);
        if (!newitem->string)
        {
            goto fail;
        }
    }
    /* If non-recursive, then we're done! */
    if (!recurse)
    {
        return newitem;
    }
    /* Walk the ->next chain for the child. */
    child = item->child;
    while (child != NULL)
    {
        newchild = cJSON_Duplicate(child, true); /* Duplicate (with recurse) each item in the ->next chain */
        if (!newchild)
        {
            goto fail;
        }

		// DonR 02Mar2022: Give each child object a pointer back to its parent.
		newchild->ParentPtr = parent;

        if (next != NULL)
        {
            /* If newitem->child already set, then crosswire ->prev and ->next and move on */
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            /* Set newitem->child and move to it */
            newitem->child = newchild;
            next = newchild;
        }
        child = child->next;
    }
    if (newitem && newitem->child)
    {
        newitem->child->prev = newchild;
    }

    return newitem;

fail:
    if (newitem != NULL)
    {
        cJSON_Delete(newitem);
    }

    return NULL;
}

static void skip_oneline_comment(char **input)
{
    *input += static_strlen("//");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if ((*input)[0] == '\n') {
            *input += static_strlen("\n");
            return;
        }
    }
}

static void skip_multiline_comment(char **input)
{
    *input += static_strlen("/*");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if (((*input)[0] == '*') && ((*input)[1] == '/'))
        {
            *input += static_strlen("*/");
            return;
        }
    }
}

static void minify_string(char **input, char **output) {
    (*output)[0] = (*input)[0];
    *input += static_strlen("\"");
    *output += static_strlen("\"");


    for (; (*input)[0] != '\0'; (void)++(*input), ++(*output)) {
        (*output)[0] = (*input)[0];

        if ((*input)[0] == '\"') {
            (*output)[0] = '\"';
            *input += static_strlen("\"");
            *output += static_strlen("\"");
            return;
        } else if (((*input)[0] == '\\') && ((*input)[1] == '\"')) {
            (*output)[1] = (*input)[1];
            *input += static_strlen("\"");
            *output += static_strlen("\"");
        }
    }
}

CJSON_PUBLIC(void) cJSON_Minify(char *json)
{
    char *into = json;

    if (json == NULL)
    {
        return;
    }

    while (json[0] != '\0')
    {
        switch (json[0])
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                json++;
                break;

            case '/':
                if (json[1] == '/')
                {
                    skip_oneline_comment(&json);
                }
                else if (json[1] == '*')
                {
                    skip_multiline_comment(&json);
                } else {
                    json++;
                }
                break;

            case '\"':
                minify_string(&json, (char**)&into);
                break;

            default:
                into[0] = json[0];
                json++;
                into++;
        }
    }

    /* and null-terminate. */
    *into = '\0';
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsInvalid(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Invalid;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_False;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xff) == cJSON_True;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & (cJSON_True | cJSON_False)) != 0;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_NULL;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Number;
}

// DonR 18Apr2021: Add specific functions for integer values. Note that
// because the relevant flags are non-exclusive, we need to perform
// the bitwise-AND against the specific value, *not* 0xFF! Note also
// that we're using a separate IntegerType variable to leave the standard
// type variable alone - this will avoid bugs (I hope). We assume here
// that if IntegerType is set, item->type == cJSON_Number; that should be
// true unless someone starts setting structure elements manually, which
// is not really supported.
CJSON_PUBLIC(cJSON_bool) cJSON_IsInt(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->IntegerType & cJSON_signed_int) == cJSON_signed_int;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedInt(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->IntegerType & cJSON_unsigned_int) == cJSON_unsigned_int;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsShort(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->IntegerType & cJSON_signed_short) == cJSON_signed_short;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedShort(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->IntegerType & cJSON_unsigned_short) == cJSON_unsigned_short;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsLong(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

//	// Assume (until proven otherwise) that the double value will compare equal to the
//	// integer value for any integer that will fit into a long int variable - so we'll
//	// see inequality only when there's a decimal component to the number. I think that
//	// this test will also work to exclude values that are valid unsigned long integers
//	// but are too big to be valid signed long integers.
//	return ((item->type & 0xFF) == cJSON_Number) && (item->valueint == item->valuedouble);
	return (item->IntegerType & cJSON_signed_long) == cJSON_signed_long;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedLong(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

//	// Assume (until proven otherwise) that the double value will compare equal to the
//	// integer value for any integer that will fit into a long int variable - so we'll
//	// see inequality only when there's a decimal component to the number. Casting
//	// valueint as an unsigned long will cause inequality if the actual value is negative.
//	return ((item->type & 0xFF) == cJSON_Number) && ((unsigned long)item->valueint == item->valuedouble);
	return (item->IntegerType & cJSON_unsigned_long) == cJSON_unsigned_long;
}



CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_String;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Array;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Object;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsRaw(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Raw;
}

CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive)
{
    if ((a == NULL) || (b == NULL) || ((a->type & 0xFF) != (b->type & 0xFF)) || cJSON_IsInvalid(a))
    {
        return false;
    }

    /* check if type is valid */
    switch (a->type & 0xFF)
    {
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
        case cJSON_Number:
        case cJSON_String:
        case cJSON_Raw:
        case cJSON_Array:
        case cJSON_Object:
            break;

        default:
            return false;
    }

    /* identical objects are equal */
    if (a == b)
    {
        return true;
    }

    switch (a->type & 0xFF)
    {
        /* in these cases and equal type is enough */
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
            return true;

        case cJSON_Number:
            if (compare_double(a->valuedouble, b->valuedouble))
            {
                return true;
            }
            return false;

        case cJSON_String:
        case cJSON_Raw:
            if ((a->valuestring == NULL) || (b->valuestring == NULL))
            {
                return false;
            }
            if (strcmp(a->valuestring, b->valuestring) == 0)
            {
                return true;
            }

            return false;

        case cJSON_Array:
        {
            cJSON *a_element = a->child;
            cJSON *b_element = b->child;

            for (; (a_element != NULL) && (b_element != NULL);)
            {
                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }

                a_element = a_element->next;
                b_element = b_element->next;
            }

            /* one of the arrays is longer than the other */
            if (a_element != b_element) {
                return false;
            }

            return true;
        }

        case cJSON_Object:
        {
            cJSON *a_element = NULL;
            cJSON *b_element = NULL;
            cJSON_ArrayForEach(a_element, a)
            {
                /* TODO This has O(n^2) runtime, which is horrible! */
                b_element = get_object_item(b, a_element->string, case_sensitive);
                if (b_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }
            }

            /* doing this twice, once on a and b to prevent true comparison if a subset of b
             * TODO: Do this the proper way, this is just a fix for now */
            cJSON_ArrayForEach(b_element, b)
            {
                a_element = get_object_item(a, b_element->string, case_sensitive);
                if (a_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(b_element, a_element, case_sensitive))
                {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

CJSON_PUBLIC(void *) cJSON_malloc(size_t size)
{
    return global_hooks.allocate(size);
}

CJSON_PUBLIC(void) cJSON_free(void *object)
{
    global_hooks.deallocate(object);
}


// DonR 02Mar2022: Add new functions/macros to support searching JSON object trees for specific values.
CJSON_PUBLIC(cJSON*) cJSON_FindMatchingItem_x	(	cJSON	*object,
													cJSON	*last_found,
													char	*tag_list,
													char	*match_char,
													long	match_long,
													double	match_double	)
{
	int			ObjectType;
	static char	top_tag	[100 + 1];	// Should check if there is actually an upper limit on tag length!
	char		*remaining_tags;
	cJSON		*child				= NULL;
	cJSON		*match				= NULL;
	cJSON		*result				= NULL;
	cJSON_bool	FoundPreviousMatch	= (last_found == NULL);	// I.e. for initial call, don't wait till we get past the first match.

	// Just in case a NULL object pointer was passed in, avoid errors by
	// using a local copy of object->type.
	ObjectType = (object == NULL) ? cJSON_Invalid : object->type & 0xFF;

	switch (ObjectType)
	{
		// If the current JSON object is an "object" type, look for the next tag inside it.
		// If found, call cJSON_FindMatchingItem_x() recursively with the child object.
		case cJSON_Object:
							// If tag_list has contents, extract the first tag (delimited by "/") and set
							// a pointer to any remaining tags after the first slash.
							if (*tag_list)
							{
								char	*slash = strchr (tag_list, '/');

								if (slash)
								{
									strncpy (top_tag, tag_list, (slash - tag_list));
									top_tag [slash - tag_list] = (char)0;
									remaining_tags = slash + 1;	// I.e. remaining tags start one byte after the slash.
								}
								else
								{
									strcpy (top_tag, tag_list);
									remaining_tags = tag_list + strlen(tag_list);	// I.e. point to the terminating NULL character.
								}
							}
							else
							{
								strcpy (top_tag, "");
								remaining_tags = tag_list;	// I.e. point to the terminating NULL of a zero-length string.
							}

							// If we have a tag to search for in the current object, look for it; if we find it,
							// call cJSON_FindMatchingItem_x() recursively with the remaining tag list.
							child	= (*top_tag)		? get_object_item (object, top_tag, true) : NULL;
							result	= (child != NULL)	? cJSON_FindMatchingItem_x (child, last_found, remaining_tags, match_char, match_long, match_double) : NULL;
							break;


		// If the current JSON object is an "array" type, loop through its elements and call
		// cJSON_FindMatchingItem_x() recursively for each of them. If this is a "find next"
		// call, don't return a match until we get past the previously-found match. (Note that
		// it may be possible to come up with a more efficient algorithm for this so we don't
		// have to repeatedly scan from the beginning!)
		// Note that since array elements don't have their own tags, here we simply pass
		// tag_list to the next recursion of cJSON_FindMatchingItem_x() unchanged.
		case cJSON_Array:
							for (child = object->child, result = NULL; child != NULL; child = child->next)
							{
								match = cJSON_FindMatchingItem_x (child, last_found, tag_list, match_char, match_long, match_double);

								// If we had a previous match and the new match matches it, set the
								// Found Previous Match flag TRUE so we will accept the *next* match
								// as a valid result. (Note that if there was no previous match,
								// FoundPreviousMatch will already be TRUE from its initialization
								// above.)
								if ((last_found != NULL) && (match == last_found))
								{
									FoundPreviousMatch = true;
									continue;	// Skip past the already-found match.
								}

								// We need to keep looping under two conditions:
								// 1) The current array element did not find a match.
								// 2) The current array element found a match, but it's
								//    a match that we found previously and we're looking
								//    for a new match we haven't found yet.
								if ((match == NULL) || (!FoundPreviousMatch))
									continue;

								// If we get here, we actually found a valid "fresh" match!
								result = match;
								break;
							}

							break;


		// All remaining valid object types are "end-points", meaning that we perform
		// an actual value test and return a result, without needing to recurse further.
		// Note that we're assuming that calls to cJSON_FindMatchingItem_x() will have
		// match-value arguments that actually make sense given the value type of the
		// JSON object we're searching for - at least for now, I am *not* testing for
		// problems like a numeric value given when we're supposed to be searching for
		// a string!

		// For type cJSON_True, consider the match as valid if both values
		// are non-zero - don't bother testing for actual equality.
		case cJSON_True:	result = (match_long != 0) ? object->ParentPtr : NULL;
							break;

		// For type cJSON_False, things are simple.
		case cJSON_False:	result = (match_long == 0) ? object->ParentPtr : NULL;
							break;


		// cJSON_Number comes in two "flavors" - if object->IntegerType is non-zero, we
		// test for exact integer equality. Otherwise we perform a double-precision
		// comparison, allowing for minor floating-point discrepancies.
		case cJSON_Number:
							if (object->IntegerType)
							{
								result = (object->valueint == match_long) ? object->ParentPtr : NULL;
							}
							else
							{
								double		DoubleDiff;
								double		MinSignificantDiff;

								// Set a tolerance ratio of 1/100,000 for equality. The math should work even
								// for very large or very small numbers - although in fact we're mostly dealing
								// with very "ordinary" orders of magnitude in our application.
								DoubleDiff			= (object->valuedouble > match_double) ? object->valuedouble - match_double : match_double - object->valuedouble;

								MinSignificantDiff	= (object->valuedouble == 0.0) ?	0.00001 :
																						((object->valuedouble > 0) ? object->valuedouble * 0.00001 : object->valuedouble * -0.00001);

								result = (DoubleDiff < MinSignificantDiff) ? object->ParentPtr : NULL;
							}

							break;


		// Note that at least for now, we are *not* testing for the validity of match_char
		// as a string pointer, other than checking that it's not NULL!
		case cJSON_String:	result = (match_char != NULL) ? ((!strcmp (object->string, match_char)) ? object->ParentPtr : NULL) : NULL;
							break;


		// For now at least, all other object types are automatic non-matches. If we need to start
		// supporting more "exotic" object types, additional cases will need to be added.
		default:			result = NULL;
							break;
	}


	return (result);
}


// Get a pointer to the next-higher JSON object, skipping past arrays.
CJSON_PUBLIC(cJSON*) cJSON_GetParentObject (const cJSON * const item)
{
	cJSON	*parent	= item->ParentPtr;

	// Skip past arrays - the assumption is that we're looking for objects with their
	// own contents, and intermediate-level arrays consist of a list of objects but have
	// no real content of their own. (In other words, if we found a matching value in
	// an object that is part of an array that in return is part of a higher-level object,
	// the parent of the lower-level object is the higher-level object.)
	while ((parent != NULL) && ((parent->type & 0xFF) == cJSON_Array))
	{
		parent	= parent->ParentPtr;
	}

	return (parent);
}


// DonR 10Apr2022: Undefine true and false to avoid compilation errors, since
// other code uses them as enum variables.
#undef true
#undef false

#endif	// cJSON_C defined