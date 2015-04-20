//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

private:
    int valid_;
    IX_FileHdr hdr_;                                // file header
    int hdrModified_;                               // dirty flag for file hdr
    PF_FileHandle PFfileHandle_;
    int keylen_;
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();

private:
    IX_FileHandle indexHandle_;
    CompOp compOp_;
    void *value_;
    ClientHint pinHint_;

    int valid_;
    int currentPage_;
    int currentKeyOffset_;

    PF_PageHandle pageHandle_;
    char *pageData_;

    RC FetchNextPage();
    int ConditionMet(char *keyData);
    int scanComplete_;
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);

private:
    PF_Manager pfm_;
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_CREATEPARAMINVALID    (START_IX_WARN + 0) // record size too big or small
#define IX_            (START_IX_WARN + 1) // record object invalid 
#define IX_FILEINVALID           (START_IX_WARN + 2) // index file handle object invalid
#define IX_           (START_IX_WARN + 3) // record does not exist
#define IX_SCANINVALID           (START_IX_WARN + 4) // file scan object invalid
#define IX_EOF                   (START_IX_WARN + 5) // no more records to be scanned
#define IX_SCANPARAMINVALID      (START_IX_WARN + 6) // parameters for scanning are invalid
#define IX_SCANOPEN              (START_IX_WARN + 7) // file scan is still open

#define IX_LASTWARN        RM_SCANPARAMINVALID

#define IX_LASTERROR       (END_RM_ERR)


IX_CREATEPARAMINVALID
#endif
