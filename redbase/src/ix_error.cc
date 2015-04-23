
#include <cerrno>
#include <cstdio>
#include <iostream>

using namespace std;

#include "ix_internal.h"
//
// Error table
//
static char *IX_WarnMsg[] = {
  (char*)"parameters for index creation are invalid",
  (char*)"filename provided is null",
  (char*)"indexhandle object is not initialized",
  (char*)"provided pData is null",
  (char*)"indexscan object is not initialized",
  (char*)"no more indices found for index scan",
  (char*)"parameters for index scan are invalid",
  (char*)"index scan is still open. close before opening again.",
  (char*)"record couldn't be found for deletion.",
  (char*)"duplicate index entry exists"
};

static char *IX_ErrorMsg[] = {

};

// ADAPTED FROM PF_ERROR.CC
// IX_PrintError
// 
// Desc: Send a message corresponding to a IX return code to cerr
//       Assumes PF_UNIX is last valid IX return code
// In:   rc - return code for which a message is desired
//
void IX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
    // Print warning
    cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
    // Print error
    cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
  else if (rc == 0)
    cerr << "IX_PrintError called with return code of 0\n";
  else
    cerr << "IX error: " << rc << " is out of bounds\n";
}
