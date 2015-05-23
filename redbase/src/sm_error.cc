
#include <cerrno>
#include <cstdio>
#include <iostream>

#include "sm.h"
using namespace std;

//
// Error table
//
static char *SM_WarnMsg[] = {
  (char*)"can't open input file for loading",
  (char*)"relation name is too long",
  (char*)"attribute name is too long",
  (char*)"database does not exist",
  (char*)"too many attributes in a relation",
  (char*)"attribute length is invalid",
  (char*)"attribute type is invalid",
  (char*)"relation does not exist",
  (char*)"attribute does not exist",
  (char*)"can't create index: index already exists on the attribute",
  (char*)"can't drop index: the attribute is not indexed",
  (char*)"cannot modify catalog relations",
  (char*)"relation already exists",
  (char*)"duplicate attrname"
};

static char *SM_ErrorMsg[] = {

};

// ADAPTED FROM PF_ERROR.CC
// SM_PrintError
// 
// Desc: Send a message corresponding to a SM return code to cerr
// In:   rc - return code for which a message is desired
//
void SM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_SM_WARN && rc <= SM_LASTWARN)
    // Print warning
    cerr << "SM warning: " << SM_WarnMsg[rc - START_SM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_SM_ERR && -rc < -SM_LASTERROR)
    // Print error
    cerr << "SM error: " << SM_ErrorMsg[-rc + START_SM_ERR] << "\n";
  else if (rc == 0)
    cerr << "SM_PrintError called with return code of 0\n";
  else
    cerr << "SM error: " << rc << " is out of bounds\n";
}
