
#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "rm.h"

//
// Constants and defines
//

#define RM_PAGE_LIST_END  -1       // end of list of free pages
#define RM_PAGE_FULL      -2       // page is being used
#define HEADER_PAGENUM	0

//
// RM_PageHdr: Header structure for file
//
struct RM_FileHdr {
	int firstFree;			// page# of page with free slot (head of linked list)
    int recordSize;			// Size of each record
    int recordsPerPage;		// Maximum # of records per page
    int numPages;      		// # of pages in the file
};

//
// RM_PageHdr: Header structure for pages
//
struct RM_PageHdr {
	int numRecords;				// Number of records populating this page
	int nextFree;				// page# of page with free slot
    unsigned short slotStatus; 	// bitmap for storing whether slot is available or not
};

#endif
