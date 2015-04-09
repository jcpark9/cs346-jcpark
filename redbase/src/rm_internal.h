
#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "rm.h"



#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>

using namespace std;
//
// Constants and defines
//

#define RM_PAGE_LIST_END  -1       // end of list of free pages
#define RM_PAGE_FULL      -2       // page is being used
#define HEADER_PAGENUM	0

#define BYTELEN 8
#define NUMLEN 4

//
// RM_PageHdr: Header structure for pages
//
struct RM_PageHdr {
	int numRecords;				// Number of records populating this page
	int nextFree;				// page# of page with free slot
};

/*  Bitmap (unsigned char[]) to indicate status of slots follows RM_PageHdr */


#endif
