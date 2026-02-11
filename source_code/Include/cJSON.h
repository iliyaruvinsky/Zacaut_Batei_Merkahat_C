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

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__

/* When compiling for windows, we specify a specific calling convention to avoid issues where we are being called from a project with a different default calling convention.  For windows you have 3 define options:

CJSON_HIDE_SYMBOLS - Define this in the case where you don't want to ever dllexport symbols
CJSON_EXPORT_SYMBOLS - Define this on library build when you want to dllexport symbols (default)
CJSON_IMPORT_SYMBOLS - Define this if you want to dllimport symbol

For *nix builds that support visibility attribute, you can define similar behavior by

setting default visibility to hidden by adding
-fvisibility=hidden (for gcc)
or
-xldscope=hidden (for sun cc)
to CFLAGS

then using the CJSON_API_VISIBILITY flag to "export" the same symbols the way CJSON_EXPORT_SYMBOLS does

*/

#define CJSON_CDECL __cdecl
#define CJSON_STDCALL __stdcall

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(CJSON_HIDE_SYMBOLS) && !defined(CJSON_IMPORT_SYMBOLS) && !defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_EXPORT_SYMBOLS
#endif

#if defined(CJSON_HIDE_SYMBOLS)
#define CJSON_PUBLIC(type)   type CJSON_STDCALL
#elif defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllexport) type CJSON_STDCALL
#elif defined(CJSON_IMPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllimport) type CJSON_STDCALL
#endif
#else /* !__WINDOWS__ */
#define CJSON_CDECL
#define CJSON_STDCALL

#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(CJSON_API_VISIBILITY)
#define CJSON_PUBLIC(type)   __attribute__((visibility("default"))) type
#else
#define CJSON_PUBLIC(type) type
#endif
#endif

/* project version */
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 14

#include <stddef.h>

// DonR 20Apr2021: Define/declare global JSON error code and UTF-8 translation switch.
#ifdef MAIN
	int		Global_JSON_Error					= 0;
	char	*Global_JSON_Tag;
	int		Global_JSON_TranslateWin1255ToUTF8	= 0;
#else
	extern int	Global_JSON_Error;
	extern char	*Global_JSON_Tag;
	extern int	Global_JSON_TranslateWin1255ToUTF8;
#endif

#define JSON_NO_ERROR				 0
#define JSON_NO_DATA_NOT_MANDATORY	 1
#define JSON_PARENT_NULL			-1
#define JSON_TAG_NOT_SUPPLIED		-2
#define JSON_MISSING_VALUE_POINTER	-3
#define JSON_MAX_LENGTH_NEGATIVE	-4
#define JSON_TAG_NOT_FOUND			-5
#define JSON_VALUE_TYPE_MISMATCH	-6

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7) /* raw json */

// DonR 18Apr2021: NOTE: 256 = (1 << 8), 512 = (1 << 9). Dunno why they expressed these two
// as explicit numbers.
#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

// DonR 18Apr2021: Add specific integer types. Note that these are not mutually exclusive:
// a number that qualifies as a short integer also qualifies as a regular integer or a
// long integer. I don't think we need separate statuses for long integers, since basically
// anything integer-like would qualify as a long. Note that in order to avoid bugs, we'll
// store the specific integer-type status in a separate variable from the main "type" of
// the cJSON instance.
#define cJSON_signed_short		(1 << 10)
#define cJSON_signed_int		(1 << 11)
#define cJSON_unsigned_short	(1 << 12)
#define cJSON_unsigned_int		(1 << 13)
#define cJSON_signed_long		(1 << 14)
#define cJSON_unsigned_long		(1 << 15)

/* The cJSON structure: */
typedef struct cJSON
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct cJSON *next;
    struct cJSON *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct cJSON *child;

	// DonR 01Mar2022: Add a parent pointer, so that we can search for a specific value however many
	// layers deep in a JSON structure, and then follow links back to get a complete picture of what
	// values are associated with it. (Note that I didn't call it just "parent", since "parent" is
	// frequently used as a variable in the cJSON.c code.)
    struct cJSON *ParentPtr;

    /* The type of the item, as above. */
    int type;

	// DonR 19Apr2021: Add a separate integer-type status. Note that integer types are
	// non-exclusive: Something that's valid as a short integer, for example, is also
	// valid as a regular integer and as a long integer. So all tests need to work with
	// bitwise-ANDs, not regular equality as is used for the main "type" status.
	int IntegerType;

    /* The item's string, if type == cJSON_String or type == cJSON_Raw */
    char *valuestring;

    /* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
	// DonR 18Apr2021: Change valueint to a long integer.
//  int valueint;
    long valueint;

    /* The item's number, if type==cJSON_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} cJSON;

typedef struct cJSON_Hooks
{
      /* malloc/free are CDECL on Windows regardless of the default calling convention of the compiler, so ensure the hooks allow passing those functions directly. */
      void *(CJSON_CDECL *malloc_fn)(size_t sz);
      void (CJSON_CDECL *free_fn)(void *ptr);
} cJSON_Hooks;

typedef int cJSON_bool;

/* Limits how deeply nested arrays/objects can be before cJSON rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* returns the version of cJSON as a string */
CJSON_PUBLIC(const char*) cJSON_Version(void);

/* Supply malloc, realloc and free functions to cJSON */
CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks* hooks);

/* Memory Management: the caller is always responsible to free the results from all
// variants of cJSON_Parse (with cJSON_Delete) and cJSON_Print (with stdlib free,
// cJSON_Hooks.free_fn, or cJSON_free as appropriate). The exception is cJSON_PrintPreallocated,
// where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a cJSON object you can interrogate. */
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value);
CJSON_PUBLIC(cJSON *) cJSON_ParseWithLength(const char *value, size_t buffer_length);

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match cJSON_GetErrorPtr(). */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated);
CJSON_PUBLIC(cJSON *) cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated);

/* Render a cJSON entity to text for transfer/storage. */
CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item);

// DonR 22Apr2021: Render a cJSON entity to file.
CJSON_PUBLIC(int) cJSON_PrintToFP(const cJSON *item, FILE *fp);
CJSON_PUBLIC(int) cJSON_PrintUnformattedToFP(const cJSON *item, FILE *fp);
static int PrintToFP (const cJSON *item, FILE *fp, cJSON_bool formatted, int depth);
static cJSON_bool PrintStringPtrToFile (char * StringToPrint, FILE *fp);


/* Render a cJSON entity to text for transfer/storage without any formatting. */
CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item);

/* Render a cJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
// DonR 10Feb2022: Added BufferLength_out parameter. If this is non-NULL, we'll send back the allocated buffer length;
// this will allow us to make a better guess next time if the initial buffer-size guess was too low.
CJSON_PUBLIC(char *) cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt, int *BufferLength_out);

/* Render a cJSON entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: cJSON is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format);

/* Delete a cJSON entity and all subentities. */
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item);

/* Returns the number of items in an array (or object). */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array);

/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index);

/* Get item "string" from object. Case insensitive. */
CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string);
CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void);

/* Check item type and return its value */
CJSON_PUBLIC(char *)			cJSON_GetStringValue		(const cJSON * const item);
CJSON_PUBLIC(double)			cJSON_GetNumberValue		(const cJSON * const item);

// DonR 18Apr2021: Add specific Get functions for integer types. Note that since
// NAN (= Not A Number) is defined only for floating-point numbers, we need to
// return two values: the actual return value is an error code, and the integer
// value - it it's the right type - is returned via a pointer argument.
CJSON_PUBLIC(int)	cJSON_GetIntValue			(const cJSON * const item, int				*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUnsignedIntValue	(const cJSON * const item, unsigned int		*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetShortValue			(const cJSON * const item, short			*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUnsignedShortValue	(const cJSON * const item, unsigned short	*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetLongValue			(const cJSON * const item, long				*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUnsignedLongValue	(const cJSON * const item, unsigned long	*ValueReturned);

// DonR 19Apr2021: Add a set of routines to retrieve values by name from a parent object.
CJSON_PUBLIC(int)	cJSON_GetStringByName		(const cJSON * const parent, char * tag, const int MaxLength, const cJSON_bool IsMandatory, char *ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetCharByName			(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, char				*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetIntByName			(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, int				*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUintByName			(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned int		*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetShortByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, short			*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUshortByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned short	*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetLongByName			(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, long				*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetUlongByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, unsigned long	*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetFloatByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, float			*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetDoubleByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, double			*ValueReturned);
CJSON_PUBLIC(int)	cJSON_GetArrayByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, cJSON			**ArrayPointerReturned);
CJSON_PUBLIC(int)	cJSON_GetObjectByName		(const cJSON * const parent, char * tag, const cJSON_bool IsMandatory, cJSON			**ObjectPointerReturned);
CJSON_PUBLIC(int)	cJSON_GetArrayItemByNumber	(const cJSON * const parent, const int ItemNumber, const cJSON_bool IsMandatory, cJSON	**ObjectPointerReturned);

// DonR 21Jul2025: Add a couple of new functions to streamline access to simple integer arrays.
CJSON_PUBLIC(int)	cJSON_GetIntegerArrayItem	(const cJSON * const array, const int ItemNumber, int *IntegerReturned);
CJSON_PUBLIC(int)	cJSON_GetIntegerArray		(const cJSON * const array, const int MaxSize, int *IntegerArrayPtr, int *SizeOut);


/* These functions check the type of an item */
// DonR 18Apr2021: Add specific Get functions for integer types.
CJSON_PUBLIC(cJSON_bool) cJSON_IsInvalid		(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsBool			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsString			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsArray			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsObject			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsRaw			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsInt			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedInt	(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsShort			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedShort	(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsLong			(const cJSON * const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsUnsignedLong	(const cJSON * const item);

/* These calls create a cJSON item of the appropriate type. */
CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean);
CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num);
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string);
/* raw json */
CJSON_PUBLIC(cJSON *) cJSON_CreateRaw(const char *raw);
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void);

/* Create a string where valuestring references a string so
 * it will not be freed by cJSON_Delete */
CJSON_PUBLIC(cJSON *) cJSON_CreateStringReference(const char *string);
/* Create an object/array that only references it's elements so
 * they will not be freed by cJSON_Delete */
CJSON_PUBLIC(cJSON *) cJSON_CreateObjectReference(const cJSON *child);
CJSON_PUBLIC(cJSON *) cJSON_CreateArrayReference(const cJSON *child);

/* These utilities create an Array of count items.
 * The parameter count cannot be greater than the number of elements in the number array,
   otherwise array access will be out of bounds.*/
CJSON_PUBLIC(cJSON *) cJSON_CreateIntArray(const int *numbers, int count);
CJSON_PUBLIC(cJSON *) cJSON_CreateFloatArray(const float *numbers, int count);
CJSON_PUBLIC(cJSON *) cJSON_CreateDoubleArray(const double *numbers, int count);
CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count);

/* Append item to the specified array/object. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);

// DonR 22Apr2021: Add new convenience function cJSON_AddNewObjectToArray(). This combines
// the functionality of cJSON_CreateObject() and cJSON_AddItemToArray().
CJSON_PUBLIC(cJSON*) cJSON_AddNewObjectToArray (cJSON *array);


/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the cJSON object.
 * WARNING: When this function was used, make sure to always check that (item->type & cJSON_StringIsConst) is zero before
 * writing to `item->string` */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);

/* Remove/Detach items from Arrays/Objects. */
CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which);
CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string);
CJSON_PUBLIC(void) cJSON_DeleteItemFromObject(cJSON *object, const char *string);
CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

/* Update array items. */
CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem); /* Shifts pre-existing items to the right. */
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON * const parent, cJSON * const item, cJSON * replacement);
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem);
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem);
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object,const char *string,cJSON *newitem);

/* Duplicate a cJSON item */
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
/* Duplicate will create a new, identical cJSON item to the one you pass, in new memory that will
 * need to be released. With recurse!=0, it will duplicate any children connected to the item.
 * The item->next and ->prev pointers are always zero on return from Duplicate. */
/* Recursively compare two cJSON items for equality. If either a or b is NULL or invalid, they will be considered unequal.
 * case_sensitive determines if object keys are treated case sensitive (1) or case insensitive (0) */
CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive);

/* Minify a strings, remove blank characters(such as ' ', '\t', '\r', '\n') from strings.
 * The input pointer json cannot point to a read-only address area, such as a string constant, 
 * but should point to a readable and writable adress area. */
CJSON_PUBLIC(void) cJSON_Minify(char *json);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);
CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw);
CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
// DonR 21Apr2021: Note that this macro does *not* support integer-type validation - I would
// avoid using it, choosing an actual function call instead.
#define cJSON_SetIntValue(object, number) ((object) ? (object)->valueint = (object)->valuedouble = (number) : (number))

/* Helper for the cJSON_SetNumberValue macro - the macro accepts any numeric type. */
CJSON_PUBLIC(double) cJSON_SetNumberHelper(cJSON *object, double number);
#define cJSON_SetNumberValue(object, number) ((object != NULL) ? cJSON_SetNumberHelper(object, (double)number) : (number))

/* Change the valuestring of a cJSON_String object, only takes effect when type of object is cJSON_String */
CJSON_PUBLIC(char*) cJSON_SetValuestring(cJSON *object, const char *valuestring);

/* Macro for iterating over an array or object */
#define cJSON_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

// Numeric-type-tolerant macro for adding any numeric type to an existing object.
#define cJSON_AddAnyNumberToObject(object,name,number) cJSON_AddNumberToObject(object, name, (double)number)

// DonR 02Mar2022: Add new functions/macros to support searching JSON object trees for specific values.
CJSON_PUBLIC(cJSON*) cJSON_FindMatchingItem_x (cJSON *object, cJSON *last_found, char *tag_list, char *match_char, long match_long, double match_double);
CJSON_PUBLIC(cJSON*) cJSON_GetParentObject (const cJSON * const item);

// To make coding simple, provide macros for cJSON_FindMatchingItem_x() calls.
#define cJSON_FindMatchingItem(object,tag_list,value)					cJSON_FindMatchingItem_x (object, NULL,			tag_list, (char *)value, (long)value, (double)value)
#define cJSON_FindNextMatchingItem(object,tag_list,value,last_match)	cJSON_FindMatchingItem_x (object, last_match,	tag_list, (char *)value, (long)value, (double)value)
// DonR 02Mar2022 end.

/* malloc/free objects using the malloc/free functions that have been set with cJSON_InitHooks */
CJSON_PUBLIC(void *) cJSON_malloc(size_t size);
CJSON_PUBLIC(void) cJSON_free(void *object);

#ifdef __cplusplus
}
#endif

#endif
