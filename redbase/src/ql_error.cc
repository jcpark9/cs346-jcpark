
#include <cerrno>
#include <cstdio>
#include <iostream>

#include "ql.h"
using namespace std;

//
// Error table
//
static char *QL_WarnMsg[] = {
  (char*)"End of Query Result",
  (char*)"Left hand side attribute and right hadn side attribute doesn't have the same type",
  (char*)"Duplicate relations in WHERE clause",
  (char*)"Attribute is not an attribute of one of the relations in WHERE clause",
  (char*)"Ambiguous unqualified attribute",
  (char*)"Number of attributes do not match",
  (char*)"Attribute does not exist"
};

static char *QL_ErrorMsg[] = {

};

// ADAPTED FROM PF_ERROR.CC
// QL_PrintError
// 
// Desc: Send a message corresponding to a QL return code to cerr
// In:   rc - return code for which a message is desired
//
void QL_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_QL_WARN && rc <= QL_LASTWARN)
    // Print warning
    cerr << "QL warning: " << QL_WarnMsg[rc - START_QL_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_QL_ERR && -rc < -QL_LASTERROR)
    // Print error
    cerr << "QL error: " << QL_ErrorMsg[-rc + START_QL_ERR] << "\n";
  else if (rc == 0)
    cerr << "QL_PrintError called with return code of 0\n";
  else
    cerr << "QL error: " << rc << " is out of bounds\n";
}