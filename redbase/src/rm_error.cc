
#include <cerrno>
#include <cstdio>
#include <iostream>

using namespace std;

//
// Error table
//
static char *RM_WarnMsg[] = {
  (char*)"record ID is not initialized",
  (char*)"record size is too big",
  (char*)"record is not initialized",
  (char*)"filehandle is not initialized",

  (char*)"file open",
  (char*)"invalid file descriptor (file closed)",
  (char*)"page already free",
  (char*)"page already unpinned",
  (char*)"end of file",
  (char*)"attempting to resize the buffer too small",
  (char*)"invalid filename"
};

static char *RM_ErrorMsg[] = {
  (char*)"no memory",
  (char*)"no buffer space",
  (char*)"incomplete read of page from file",
  (char*)"incomplete write of page to file",
  (char*)"incomplete read of header from file",
  (char*)"incomplete write of header from file",
  (char*)"new page to be allocated already in buffer",
  (char*)"hash table entry not found",
  (char*)"page already in hash table",
  (char*)"invalid file name"
};

//
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
    cerr << "RM warning: " << RM_WarnMsg[rc - START_PF_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_RM_ERR && -rc < -RM_LASTERROR)
    // Print error
    cerr << "RM error: " << RM_ErrorMsg[-rc + START_RM_ERR] << "\n";
  else if (rc == 0)
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "RM error: " << rc << " is out of bounds\n";
}
