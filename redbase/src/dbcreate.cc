//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "ix.h"
#include "rm.h"
#include "sm.h"
#include "redbase.h"

using namespace std;

static const AttrInfo RelcatInfo[] = {
    {(char*)"relName", STRING, MAXNAME},
    {(char*)"tupleLength", INT, 4},
    {(char*)"attrCount", INT, 4},
    {(char*)"indexCount", INT, 4},
    {(char*)"numTuples", INT, 4}
};

static const AttrInfo AttrcatInfo[] = {
    {(char*)"relName", STRING, MAXNAME},
    {(char*)"attrName", STRING, MAXNAME},
    {(char*)"offset", INT, 4},
    {(char*)"attrType", INT, 4},
    {(char*)"attrLength", INT, 4},
    {(char*)"indexNo", INT, 4}
};

//
// main
//
int main(int argc, char *argv[])
{
    char *dbname;
    char command[255] = "mkdir ";
    RC rc;

    // Look for 2 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // The database name is the second argument
    dbname = argv[1];

    // Create a subdirectory for the database
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot create directory: " << dbname << "\n";
        exit(1);
    }

    // Create the system catalogs...
    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        return SM_DBNOTEXIST;
    }

    PF_Manager pfm;
    RM_Manager rmm(pfm);
    IX_Manager ixm(pfm);
    SM_Manager smm(ixm, rmm);


    /* Create record files for two catalogs */
    if ((rc = rmm.CreateFile((char *)"relcat", sizeof(RelcatTuple)))) goto terminate;
    if ((rc = rmm.CreateFile((char *)"attrcat", sizeof(AttrcatTuple)))) goto terminate;

    /* Create index files for two catalogs */
    if ((rc = ixm.CreateIndex((char *)"relcat", CATALOGINDEXNO, STRING, MAXNAME))) goto terminate; 
    if ((rc = ixm.CreateIndex((char *)"attrcat", CATALOGINDEXNO, STRING, MAXNAME))) goto terminate;

    if ((rc = smm.OpenDb(dbname))) goto terminate;

    /* Add to catalog about metadata about catalogs */
    if ((rc = smm.AddToCatalog((char *)"relcat", RelcatCount, (AttrInfo *)RelcatInfo))) goto terminate;
    if ((rc = smm.AddToCatalog((char *)"attrcat", AttrcatCount, (AttrInfo *)AttrcatInfo))) goto terminate;

    if ((rc = smm.CloseDb())) goto terminate;

    return (0);

    terminate: 
        PrintError(rc);
        exit(1);
}
