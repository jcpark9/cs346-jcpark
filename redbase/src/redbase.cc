//
// redbase.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include "ql.h"
#include "lg.h"

using namespace std;

//
// main
//
int main(int argc, char *argv[])
{
    char *dbname;
    RC rc;

    // Look for 2 arguments.  The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc < 2 || argc > 3) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    dbname = argv[1];

    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    PF_Manager pfm;
    RM_Manager rmm(pfm);
    LG_Manager lgm(pfm, rmm);
    IX_Manager ixm(pfm);
    SM_Manager smm(ixm, rmm, lgm);
    QL_Manager qlm(smm, ixm, rmm, lgm);

    if (argc == 3) {
        bAbort = 1;
        abortProb = atoi(argv[2]);
    } else {
        bAbort = 0;
    }

    // open the database
    if ((rc = smm.OpenDb(dbname))) {
        PrintError(rc);
        exit(1);
    }
   
    // call the parser
    RBparse(pfm, smm, qlm, lgm);
    
    // close the database
    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        exit(1);
    }
    cout << "Bye.\n";
}
