
#include <cerrno>
#include <cstdio>
#include <iostream>

using namespace std;

#include "rm_internal.h"
//
// Error table
//
static char *RM_WarnMsg[] = {
  (char*)"record ID is not initialized",
  (char*)"record size is not valid",
  (char*)"record is not initialized",
  (char*)"filehandle is not initialized",
  (char*)"record does not exist",
  (char*)"filescan is not initialized",
  (char*)"no more records found for file scan",
  (char*)"parameters for file scan are invalid",
  (char*)"file scan is still open. close before opening again.",
  (char*)"filename given is null",
  (char*)"data for insertion is null"
};



static char *RM_ErrorMsg[] = {

};

// ADAPTED FROM PF_ERROR.CC
// RM_PrintError
// 
// Desc: Send a message corresponding to a PF return code to cerr
//       Assumes PF_UNIX is last valid PF return code
// In:   rc - return code for which a message is desired
//
void RM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_RM_WARN && rc <= RM_LASTWARN)
    // Print warning
    cerr << "RM warning: " << RM_WarnMsg[rc - START_RM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_RM_ERR && -rc < -RM_LASTERROR)
    // Print error
    cerr << "RM error: " << RM_ErrorMsg[-rc + START_RM_ERR] << "\n";
  else if (rc == 0)
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "RM error: " << rc << " is out of bounds\n";
}
