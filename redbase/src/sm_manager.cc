//
// File:        SM component stubs
// Description: Print parameters of all SM_Manager methods
// Authors:     Dallan Quass (quass@cs.stanford.edu)
//

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include "redbase.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"

using namespace std;

SM_Manager::SM_Manager(IX_Manager &ixm, RM_Manager &rmm, LG_Manager &lgm)
{
    ixm_ = &(ixm);
    rmm_ = &(rmm);
    lgm_ = &(lgm);
}

SM_Manager::~SM_Manager()
{
}

RC SM_Manager::OpenDb(const char *dbName)
{
    RC rc;
    if ((rc = lgm_->CreateLog())) return rc;
    
    /* Open files and indices for catalog relations */
    if ((rc = rmm_->OpenFile("relcat",relcatFile_))) return rc;
    if ((rc = rmm_->OpenFile("attrcat", attrcatFile_))) return rc;
    if ((rc = ixm_->OpenIndex("relcat", CATALOGINDEXNO, relcatIndex_))) return rc;
    if ((rc = ixm_->OpenIndex("attrcat", CATALOGINDEXNO, attrcatIndex_))) return rc;
    return (0);
}

RC SM_Manager::CloseDb()
{
    RC rc;
    if ((rc = lgm_->DestroyLog())) return rc;

    /* Close files and indices for catalog relations */
    if ((rc = rmm_->CloseFile(relcatFile_))) return rc;
    if ((rc = rmm_->CloseFile(attrcatFile_))) return rc;
    if ((rc = ixm_->CloseIndex(relcatIndex_))) return rc;
    if ((rc = ixm_->CloseIndex(attrcatIndex_))) return rc;
    return (0);
}

RC SM_Manager::CreateTable(const char *relName,
                           int        attrCount,
                           AttrInfo   *attributes)
{
    cout << "CreateTable\n"
         << "   relName     =" << relName << "\n"
         << "   attrCount   =" << attrCount << "\n";

    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;
    if (attrCount > MAXATTRS) return SM_TOOMANYATTR;

    /* Check if relName already exists */
    RM_Record rec;
    if (FindRelMetadata(relName, rec) == 0) return SM_RELALREADYEXISTS;

    /* Check for duplicate attr name */
    for (int i=0; i < attrCount; i++) {
        for (int j=i+1; j < attrCount; j++) {
            if (strcmp(attributes[i].attrName, attributes[j].attrName) == 0) return SM_DUPLICATEATTR; 
        }
    }

    /* Check whether each attribue has valid type, length and name */
    int tupleLength = 0;
    for (int i = 0; i < attrCount; i++) {
        cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
             << "   attrType="
             << (attributes[i].attrType == INT ? "INT" :
                 attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
             << "   attrLength=" << attributes[i].attrLength << "\n";
        
        if (strlen(attributes[i].attrName) > MAXNAME) return SM_ATTRNAMETOOLONG;
        if (attributes[i].attrType < 0 || attributes[i].attrType > STRING) return SM_INVALIDATTRTYPE;
        if (attributes[i].attrLength > MAXSTRINGLEN || attributes[i].attrLength <= 0 ||
            ((attributes[i].attrType == INT || attributes[i].attrType == FLOAT) 
            && attributes[i].attrLength != 4)) return SM_INVALIDATTRLEN;
        tupleLength += attributes[i].attrLength;
    }

    /* Add data about this relation to catalog */
    RC rc = AddToCatalog((char *)relName, attrCount, attributes);
    if (rc) return rc;

    /* Create a file for the relation records */
    rc = rmm_->CreateFile(relName, tupleLength);
    if (rc) return rc;

    return (0);
}

/* Adds metadata about the new relation and attributes to relcat and attract
 */
RC SM_Manager::AddToCatalog(char *relName, int attrCount, AttrInfo *attributes)
{
    /* Insert metadata about the new attributes into Attrcat catalog */
    RC rc;
    int offset = 0;
    RID rid;
    for (int i=0; i < attrCount; i++) {  
        AttrcatTuple attrcatTuple;
        strncpy(attrcatTuple.relName, relName, MAXNAME);
        strncpy(attrcatTuple.attrName, attributes[i].attrName, MAXNAME);
        attrcatTuple.offset = offset;
        attrcatTuple.attrType = attributes[i].attrType;
        attrcatTuple.attrLength = attributes[i].attrLength;
        attrcatTuple.indexNo = -1;

        /* relName attribute of relcat and attrcat is indexed by default */
        if ((strcmp(relName, "relcat") == 0 || (strcmp(relName, "attrcat") == 0))
            && strcmp(attributes[i].attrName, "relName") == 0)
            attrcatTuple.indexNo = CATALOGINDEXNO;

        /* Add record into attrcat record file */
        if ((rc = attrcatFile_.InsertRec(((char *) &attrcatTuple), rid))) return rc;

        /* Add index entry for relName in Attrcat */
        if ((rc = attrcatIndex_.InsertEntry(relName, rid))) return rc;
        
        offset += attributes[i].attrLength;
    }

    /* Insert metadata about the new relation into Relcat catalog */
    RelcatTuple relcatTuple;
    strncpy(relcatTuple.relName, relName, MAXNAME);
    relcatTuple.tupleLength = offset;
    relcatTuple.attrCount = attrCount;
    relcatTuple.indexCount = 0;
    relcatTuple.numTuples = 0;

    /* relcat and attrcat have one index by default */
    if ((strcmp(relName, "relcat") == 0 || (strcmp(relName, "attrcat") == 0)))
        relcatTuple.indexCount = 1;
    /* Manually enter number of attributes for Attrcat catalog */
    if (strcmp(relName, "attrcat") == 0) {
        relcatTuple.numTuples = RelcatCount + AttrcatCount;
    }

    /* Add record to relcat record file and index entry for relName */
    if ((rc = relcatFile_.InsertRec(((char *) &relcatTuple), rid))) return rc;
    if ((rc = relcatIndex_.InsertEntry(relName, rid))) return rc;

    /* Flush changes in catalogs */
    if ((rc = relcatFile_.ForcePages())) return rc;
    if ((rc = attrcatFile_.ForcePages())) return rc;
    if ((rc = relcatIndex_.ForcePages())) return rc;
    if ((rc = attrcatIndex_.ForcePages())) return rc;

    /* Increment numTuples for catalog relations */
    RM_Record relRecord;
    if ((rc = FindRelMetadata("relcat", relRecord))) return rc;
    RelcatTuple *relMetadata = GetRelcatTuple(relRecord);
    relMetadata->numTuples++;
    if ((rc = relcatFile_.UpdateRec(relRecord))) return rc;

    if (strcmp(relName, "relcat") != 0 && strcmp(relName, "attrcat") != 0) {
        if ((rc = FindRelMetadata("attrcat", relRecord))) return rc;
        relMetadata = GetRelcatTuple(relRecord);
        relMetadata->numTuples += attrCount;
        if ((rc = relcatFile_.UpdateRec(relRecord))) return rc;
    }

    if ((rc = relcatFile_.ForcePages())) return rc;

    return (0);
}

RC SM_Manager::DropTable(const char *relName)
{
    RC rc;
    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;
    if (strcmp(relName, "attrcat") == 0 || strcmp(relName, "relcat") == 0) return SM_CANTMODIFYCATALOG;
    
    /* Delete record of the relation from Relcat and Delete index entry for relName */
    RM_Record relMetadata; RID rid;
    if ((rc = FindRelMetadata(relName, relMetadata))) return rc;
    relMetadata.GetRid(rid);
    if ((rc = relcatFile_.DeleteRec(rid))) return rc;
    if ((rc = relcatIndex_.DeleteEntry((void *)relName, rid))) return rc;

    /* Destroy relation data */
    if ((rc = rmm_->DestroyFile(relName))) return rc;

    /* Iterate over each attribute (of the given relation) in Attrcat and delete */
    IX_IndexScan scan;
    if ((rc = scan.OpenScan(attrcatIndex_, EQ_OP, (char *)relName))) return rc;

    RM_Record rec; 
    while (rc != IX_EOF) {
        rc = scan.GetNextEntry(rid);
       
        if (rc != 0 && rc != IX_EOF) return (rc);

        if (rc != IX_EOF) {
            if ((rc = attrcatFile_.GetRec(rid, rec))) return rc;
            AttrcatTuple *attrcatTuple = GetAttrcatTuple(rec);

            /* Destroy index file if this attribute is indexed */
            if (attrcatTuple->indexNo != -1) {
                if ((rc = ixm_->DestroyIndex(relName, attrcatTuple->indexNo))) return rc;
            }

            /* Delete record of this attribute from Attrcat */
            if ((rc = attrcatFile_.DeleteRec(rid))) return rc;

            /* Delete index entry for relName in Attrcat */
            if ((rc = attrcatIndex_.DeleteEntry((void *)relName, rid)));
        }
    }

    /* Flush changes in catalog */
    if ((rc = relcatFile_.ForcePages())) return rc;
    if ((rc = attrcatFile_.ForcePages())) return rc;
    if ((rc = relcatIndex_.ForcePages())) return rc;
    if ((rc = attrcatIndex_.ForcePages())) return rc;

    return (0);
}

RC SM_Manager::CreateIndex(const char *relName,
                           const char *attrName)
{
    RC rc;
    cout << "CreateIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";

    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;
    if (strlen(attrName) > MAXNAME) return SM_ATTRNAMETOOLONG;
    
    RM_Record relRecord;
    RM_Record attrRecord;

    /* Get metadata about the relation and thet attribute */
    if ((rc = FindRelMetadata(relName, relRecord))) return rc;
    if ((rc = FindAttrMetadata(relName, attrName, attrRecord))) return rc;
    RelcatTuple *relMetadata = GetRelcatTuple(relRecord);
    AttrcatTuple *attrMetadata = GetAttrcatTuple(attrRecord);

    if (attrMetadata->indexNo != -1) return SM_INDEXEXISTSONATTR;
    attrMetadata->indexNo = (relMetadata->indexCount)++;

    /* Create new index file and open files */
    if ((rc = ixm_->CreateIndex(relName, attrMetadata->indexNo, 
        (AttrType) attrMetadata->attrType, attrMetadata->attrLength))) return rc;
    
    IX_IndexHandle ih;
    RM_FileHandle fh;
    if ((rc = ixm_->OpenIndex(relName, attrMetadata->indexNo, ih))) return rc;
    if ((rc = rmm_->OpenFile(relName, fh))) return rc;

    /* Insert index entries for existing records */
    RM_FileScan scan;
    RM_Record rec;
    if ((rc = scan.OpenScan(fh, INT, sizeof(int), 0,
        NO_OP, NULL))) return rc;

    while (rc != RM_EOF) {
       rc = scan.GetNextRec(rec);
       
       if (rc != 0 && rc != RM_EOF) return (rc);

       if (rc != RM_EOF) {
            char *pData; RID rid;
            rec.GetData(pData);
            rec.GetRid(rid);
            if ((rc = ih.InsertEntry(pData + attrMetadata->offset, rid))) return rc;
       }
    }

    /* Close scans and files */
    if ((rc = scan.CloseScan())) return rc;
    if ((rc = ixm_->CloseIndex(ih))) return rc;
    if ((rc = rmm_->CloseFile(fh))) return rc;

    /* Update catalogs and flush changes */
    if ((rc = relcatFile_.UpdateRec(relRecord))) return rc;
    if ((rc = attrcatFile_.UpdateRec(attrRecord))) return rc;
    if ((rc = relcatFile_.ForcePages())) return rc;
    if ((rc = attrcatFile_.ForcePages())) return rc;

    return (0);
}

RC SM_Manager::DropIndex(const char *relName,
                         const char *attrName)
{
    RC rc;
    cout << "DropIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";

    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;
    if (strlen(attrName) > MAXNAME) return SM_ATTRNAMETOOLONG;
    if (strcmp(relName, "attrcat") == 0 || strcmp(relName, "relcat") == 0) return SM_CANTMODIFYCATALOG;

    RM_Record attrRecord;
    if ((rc = FindAttrMetadata(relName, attrName, attrRecord))) return rc;
    AttrcatTuple *attrMetadata = GetAttrcatTuple(attrRecord);

    if (attrMetadata->indexNo == -1) return SM_ATTRNOTINDEXED;

    /* Destroy index file */
    if ((rc = ixm_->DestroyIndex(relName, attrMetadata->indexNo))) return rc;
    
    /* Update attrcat catalog */
    attrMetadata->indexNo = -1;
    if ((rc = attrcatFile_.UpdateRec(attrRecord))) return rc;
    if ((rc = attrcatFile_.ForcePages())) return rc;

    return (0);
}

RC SM_Manager::Load(const char *relName,
                    const char *fileName)
{
    RC rc;
    cout << "Load\n"
         << "   relName =" << relName << "\n"
         << "   fileName=" << fileName << "\n";

    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;

    /* Open input file */
    ifstream inputfile(fileName);
    if (!inputfile.is_open()) return SM_CANTOPENFILE;

    /* Open relation file and get relation metadata */
    RM_Record relRecord;
    if ((rc = FindRelMetadata(relName, relRecord))) return rc;
    RelcatTuple *relMetadata = GetRelcatTuple(relRecord);

    RM_FileHandle fh;
    if ((rc = rmm_->OpenFile(relName, fh))) return rc;
    
    /* Get data for each relation attribute*/
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = FillDataAttributes(relName, attributes, attrCount))) return rc;

    int singleStatementXact = 0;
    if (!lgm_->bInTransaction) {
        singleStatementXact = 1;
        lgm_->BeginT();
    }

    /* Iterate through each line (tuple) */
    char line[MAXATTRS * (MAXSTRINGLEN+1)];
    int numTuplesAdded = 0;
    while (inputfile.getline(line, MAXATTRS * (MAXSTRINGLEN+1), '\n')) {
        if (bAbort && (rand() % 100) < abortProb) {
            cout << "[ CRASH WHILE LOAD ]" << endl;
            abort();
        }

        RID rid;
        char recData[relMetadata->tupleLength];
        memset(recData, 0, relMetadata->tupleLength);
        char *token = strtok(line, ",");

        /* Tokenize and pack data for each attribute into one record */
        for (int i = 0; i < attrCount; i++) {
            char *fieldData = recData + attributes[i].offset;

            switch(attributes[i].attrType) {
                case INT: 
                    {
                        int n = atoi(token);
                        memcpy(fieldData, &n, attributes[i].attrLength);
                    }
                    break;
                case FLOAT:
                    {
                        float f = atof(token);
                        memcpy(fieldData, &f, attributes[i].attrLength);
                    }
                    break;
                case STRING: 
                    strncpy(fieldData, token, attributes[i].attrLength);
                    break;
            }
            token = strtok(NULL, ",");
        }

        /* Insert tuple into relation */
        if ((rc = fh.InsertRec(recData, rid))) return rc;
        numTuplesAdded++;

        /* Insert index entry for indexed attributes */
        for (int i = 0; i < attrCount; i++) {
            if (attributes[i].indexNo != -1) {
                IX_IndexHandle ih;
                if ((rc = ixm_->OpenIndex(relName, attributes[i].indexNo, ih))) return rc;
                if ((rc = ih.InsertEntry(recData + attributes[i].offset, rid))) return rc;
                if ((rc = ixm_->CloseIndex(ih))) return rc;
            }
        }
    }

    if (singleStatementXact) lgm_->CommitT();

    /* Update relcat catalog */
    relMetadata->numTuples += numTuplesAdded;
    if ((rc = relcatFile_.UpdateRec(relRecord))) return rc;
    if ((rc = relcatFile_.ForcePages())) return rc;
    
    /* Clean up */
    delete[] attributes;
    inputfile.close();
    if ((rc = rmm_->CloseFile(fh))) return rc;
    return (0);
}

RC SM_Manager::Print(const char *relName)
{
    RC rc;
    cout << "Print\n"
         << "   relName=" << relName << "\n";
    
    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;

    DataAttrInfo *attributes;
    int attrCount;
    // Fill in the attributes structure
    if ((rc = FillDataAttributes(relName, attributes, attrCount))) return rc;

    // Instantiate a Printer object and print the header information
    Printer p(attributes, attrCount);
    p.PrintHeader(cout);

    // Open the file and set up the file scan
    RM_FileHandle rfh;
    if ((rc = rmm_->OpenFile(relName, rfh))) return(rc);
    RM_FileScan rfs;
    if ((rc = rfs.OpenScan(rfh, INT, sizeof(int), 0, NO_OP, NULL))) {
        cout  << "??" << rc << endl;        
        return (rc);
    }

    RM_Record rec;
    // Print each tuple
    while (rc != RM_EOF) {
       rc = rfs.GetNextRec(rec);
       
       if (rc != 0 && rc != RM_EOF) return (rc);

       if (rc != RM_EOF) {
            char *data;
            rec.GetData(data);
            p.Print(cout, data);
       }
    }
    p.PrintFooter(cout);

    // Close the scan, file, delete the attributes pointer
    if ((rc = rfs.CloseScan())) return rc;
    if ((rc = rmm_->CloseFile(rfh))) return rc;
    delete[] attributes;

    return (0);
}

RC SM_Manager::Set(const char *paramName, const char *value)
{
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    return (0);
}

RC SM_Manager::Help()
{
    return Print("relcat");
}

RC SM_Manager::Help(const char *relName)
{
    RC rc;
    cout << "Help\n"
         << "   relName=" << relName << "\n";
 
    if (strlen(relName) > MAXNAME) return SM_RELNAMETOOLONG;

    /* Check if relName already exists */
    RM_Record rec;
    if (FindRelMetadata(relName, rec)) return SM_RELNOTEXIST;

    DataAttrInfo *attributes;
    int attrCount;
    // Fill in the attributes structure
    if ((rc = FillDataAttributes("attrcat", attributes, attrCount))) return rc;

    Printer p(attributes, attrCount);
    p.PrintHeader(cout);

    // Open attrcat file and initialize scan
    IX_IndexScan scan;
    if ((rc = scan.OpenScan(attrcatIndex_, EQ_OP, (char *)relName)))
       return (rc);

    // Print each relation tuple in relcat
    RID rid;
    while (rc != IX_EOF) {
        rc = scan.GetNextEntry(rid);
       
        if (rc != 0 && rc != IX_EOF) return (rc);

        if (rc != IX_EOF) {
            if ((rc = attrcatFile_.GetRec(rid, rec))) return rc;
            char *data;
            rec.GetData(data);
            p.Print(cout, data);
        }
    }
    p.PrintFooter(cout);

    // Close File and Scan
    if ((rc = scan.CloseScan())) return rc;
    delete[] attributes;

    return (0);
}

/* Looks up relcat to find metadata about a given relation
 * Upon return, rec contains the corresponding relcat tuple
 */
RC SM_Manager::FindRelMetadata(const char *relName, RM_Record &rec) {
    RC rc;
    IX_IndexScan scan;
    if ((rc = scan.OpenScan(relcatIndex_, EQ_OP, (char *)relName))) return rc;
    
    RID resultRID;
    rc = scan.GetNextEntry(resultRID);
    if ((rc == IX_EOF)) return SM_RELNOTEXIST;
    else if (rc) return rc;
    
    if ((rc = relcatFile_.GetRec(resultRID, rec))) return rc;
    return (0);
}

/* Helper function for extracting data from rec
    and casting it to (RelcatTuple *) */
RelcatTuple * SM_Manager::GetRelcatTuple(RM_Record &rec) {
    char *pData;
    rec.GetData(pData);
    return ((RelcatTuple *) pData);
}


/* Looks up attrcat to find metadata about a given attribute in a given relation
 * Upon return, rec contains the corresponding attrcat tuple
 */
RC SM_Manager::FindAttrMetadata(const char *relName, const char* attrName, RM_Record &rec) {
    RC rc;
    IX_IndexScan scan;

    if ((rc = scan.OpenScan(attrcatIndex_, EQ_OP, (char *)relName))) return rc;
    
    RID rid;

    while ((rc = scan.GetNextEntry(rid)) == 0) {
        if ((rc = attrcatFile_.GetRec(rid, rec))) return rc;

        char *pData;
        rec.GetData(pData);
        if (strcmp(pData + MAXNAME, attrName) == 0) return 0;
    }

    return SM_ATTRNOTEXIST;
}

/* Helper function for extracting data from rec
    and casting it to (AttrcatTuple *) */
AttrcatTuple * SM_Manager::GetAttrcatTuple(RM_Record &rec) {
    char *pData;
    rec.GetData(pData);
    return ((AttrcatTuple *) pData);
}

/* Populates attributes with metadata about each attribute in a given relation
 * Upon return, attributes contains DataAttrInfo about each attribute
 * and attrCount = # of attriutes in a given relation
 */
RC SM_Manager::FillDataAttributes(const char *relName, DataAttrInfo *&attributes, int &attrCount) {
    RC rc;
    // Find metadata on the relation to get attrCount
    RM_Record relRecord;
    if ((rc = FindRelMetadata(relName, relRecord))) return rc;
    RelcatTuple *relMetadata = GetRelcatTuple(relRecord);
    attrCount = relMetadata->attrCount;

    // Allocate memory for attributes
    attributes = new DataAttrInfo[attrCount];

    // Fill in the attributes structure by scanning through each attribute
    IX_IndexScan scan; 
    RID rid;
    if ((rc = scan.OpenScan(attrcatIndex_, EQ_OP, (char *)relName))) {
        delete[] attributes; return rc;
    }

    int i = 0;
    RM_Record rec;
    while ((rc = scan.GetNextEntry(rid)) == 0) {
        if ((rc = attrcatFile_.GetRec(rid, rec))) {
            delete[] attributes; return rc;
        }

        AttrcatTuple *attrMetadata = GetAttrcatTuple(rec);
        strncpy(attributes[i].relName, attrMetadata->relName, MAXNAME + 1);
        strncpy(attributes[i].attrName, attrMetadata->attrName, MAXNAME + 1);
        attributes[i].attrType = (AttrType) attrMetadata->attrType;
        attributes[i].attrLength = attrMetadata->attrLength;
        attributes[i].offset = attrMetadata->offset;
        attributes[i].indexNo = attrMetadata->indexNo;        
        i++;
    }

    if (rc && rc != IX_EOF) { delete[] attributes; return rc; } 
    if ((rc == scan.CloseScan())) { delete[] attributes; return rc; }
    return 0;
}