
#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "rm.h"
#include "lg.h"

//
// Constants and defines
//

#define RM_PAGE_LIST_END  -1       // end of list of free pages
#define RM_PAGE_FULL      -2       // page is being used
#define HEADER_PAGENUM	0		// Page number of file header

#define BYTELEN 8				// length of bits per byte
#define NUMLEN 4				// length of FLOAT and INT

//
// RM_PageHdr: Header structure for pages
//
struct RM_PageHdr {
	int numRecords;				// Number of records populating this page
	int nextFree;				// page# of page with free slot
	LSN pageLSN;
};

/* RM_PageHdr in each page is followed by bitmap (unsigned char[]) to indicate availability of slots */


#endif
