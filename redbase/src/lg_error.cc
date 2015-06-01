
#include <cerrno>
#include <cstdio>
#include <iostream>

#include "lg.h"
using namespace std;

//
// Error table
//
static char *LG_WarnMsg[] = {
  (char *) "There is a transaction in progress",
  (char *) "There is NO transaction in progress",
  (char *) "The log is empty"
};

#define LG_INTRANSACTION                (START_LG_WARN + 0)
#define LG_NOTINTRANSACTION             (START_LG_WARN + 1)
#define LG_LOGEMPTY                     (START_LG_WARN + 2)
static char *LG_ErrorMsg[] = {

};

// ADAPTED FROM PF_ERROR.CC
// LG_PrintError
// 
// Desc: Send a message corresponding to a LG return code to cerr
// In:   rc - return code for which a message is desired
//
void LG_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_LG_WARN && rc <= LG_LASTWARN)
    // Print warning
    cerr << "LG warning: " << LG_WarnMsg[rc - START_LG_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_LG_ERR && -rc < -LG_LASTERROR)
    // Print error
    cerr << "LG error: " << LG_ErrorMsg[-rc + START_LG_ERR] << "\n";
  else if (rc == 0)
    cerr << "LG_PrintError called with return code of 0\n";
  else
    cerr << "LG error: " << rc << " is out of bounds\n";
}