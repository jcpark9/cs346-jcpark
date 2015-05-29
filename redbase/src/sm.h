//
// sm.h
//   Data Manager Component Interface
//

#ifndef SM_H
#define SM_H

// Please do not include any other files than the ones below in this file.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "redbase.h"  // Please don't change these lines
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "printer.h"

#define CATALOGINDEXNO 0

struct RelcatTuple {
    char relName[MAXNAME];
    uint32_t tupleLength;
    uint32_t attrCount;
    uint32_t indexCount;
    uint32_t numTuples;
};
static const int RelcatCount = 5;

struct AttrcatTuple {
    char relName[MAXNAME];
    char attrName[MAXNAME];
    uint32_t offset;
    AttrType attrType;
    uint32_t attrLength;
    int32_t indexNo;
};
static const int AttrcatCount = 6;


//
// SM_Manager: provides data management
//
class SM_Manager {
    friend class QL_Manager;
public:
    SM_Manager    (IX_Manager &ixm, RM_Manager &rmm);
    ~SM_Manager   ();                             // Destructor

    RC OpenDb     (const char *dbName);           // Open the database
    RC CloseDb    ();                             // close the database

    RC CreateTable(const char *relName,           // create relation relName
                   int        attrCount,          //   number of attributes
                   AttrInfo   *attributes);       //   attribute data
    RC CreateIndex(const char *relName,           // create an index for
                   const char *attrName);         //   relName.attrName
    RC DropTable  (const char *relName);          // destroy a relation

    RC DropIndex  (const char *relName,           // destroy index on
                   const char *attrName);         //   relName.attrName
    RC Load       (const char *relName,           // load relName from
                   const char *fileName);         //   fileName
    RC Help       ();                             // Print relations in db
    RC Help       (const char *relName);          // print schema of relName

    RC Print      (const char *relName);          // print relName contents

    RC Set        (const char *paramName,         // set parameter to
                   const char *value);            //   value

    RC AddToCatalog(char *relName, int attrCount, AttrInfo *attributes);
    RC FillDataAttributes(const char *relName, DataAttrInfo *&attributes, int &attrCount);

private:
    IX_Manager *ixm_;
    RM_Manager *rmm_;
    RM_FileHandle attrcatFile_;
    RM_FileHandle relcatFile_;
    IX_IndexHandle relcatIndex_;
    IX_IndexHandle attrcatIndex_;

    RC FindRelMetadata(const char *relName, RM_Record &rec);
    RC FindAttrMetadata(const char *relName, const char* attrName, RM_Record &rec);
    RelcatTuple* GetRelcatTuple(RM_Record &rec);
    AttrcatTuple* GetAttrcatTuple(RM_Record &rec);
};

//
// Print-error function
//
void SM_PrintError(RC rc);

#define SM_CANTOPENFILE       (START_SM_WARN + 0)
#define SM_RELNAMETOOLONG     (START_SM_WARN + 1) 
#define SM_ATTRNAMETOOLONG    (START_SM_WARN + 2)
#define SM_DBNOTEXIST         (START_SM_WARN + 3) 
#define SM_TOOMANYATTR        (START_SM_WARN + 4)
#define SM_INVALIDATTRLEN     (START_SM_WARN + 5) 
#define SM_INVALIDATTRTYPE    (START_SM_WARN + 6)
#define SM_RELNOTEXIST        (START_SM_WARN + 7)
#define SM_ATTRNOTEXIST       (START_SM_WARN + 8)
#define SM_INDEXEXISTSONATTR  (START_SM_WARN + 9)
#define SM_ATTRNOTINDEXED     (START_SM_WARN + 10)
#define SM_CANTMODIFYCATALOG  (START_SM_WARN + 11)
#define SM_RELALREADYEXISTS   (START_SM_WARN + 12)
#define SM_DUPLICATEATTR      (START_SM_WARN + 13)
#define SM_LASTWARN           SM_DUPLICATEATTR

#define SM_LASTERROR       (END_SM_ERR)

#endif
