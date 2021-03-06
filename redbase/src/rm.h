//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

class qJoin;
class RM_FileHandle;
class RM_Record;
class RM_FileScan;
class LG_Manager;

//
// RM_FileHdr: Header structure for file
//
struct RM_FileHdr {
    int firstFree;          // page# of page with free slot (head of linked list)
    int recordSize;         // Size of each record
    int recordsPerPage;     // Maximum # of records per page
    int numPages;           // # of pages in the file
    int bitmapSize;         // Size of the bitmap used to indicate whether a slot in a page is filled
};
typedef struct RM_FileHdr RM_FileHdr;

//
// RM_Manager: provides RM file management
//
class RM_Manager {
    friend class LG_Manager;
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);
    RC OpenFileWithFd(int fd, RM_FileHandle &fileHandle);
    RC CloseFile  (RM_FileHandle &fileHandle);

private:
    PF_Manager *pfmanager_;
    LG_Manager *lgmanager_;
};


struct LSN;
//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
    friend class RM_Manager;
    friend class RM_FileScan;

public:
    RM_FileHandle ();
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record
    RC InsertRecAtRid(const char *recData, const RID &rid);
    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);
    
    int GetRecordSize();
    RC UpdatePageLSN(const RID &rid, const LSN &lsn);
    RC GetPageLSN(RID rid, LSN &lsn);

private:
    int valid_;
    RM_FileHdr hdr_;                               // file header
    int hdrModified_;                               // dirty flag for file hdr
    PF_FileHandle PFfileHandle_;
    char filename[MAXNAME];
    LG_Manager *lgm_;

    SlotNum FindAvailableSlot(char *pageData);
    RC GetPageData(PageNum pageNum, char *&pageData) const;
};


//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan

private:
    RM_FileHandle fileHandle_;
    AttrType attrType_;
    int attrLength_;
    int attrOffset_;
    CompOp compOp_;
    void *value_;
    ClientHint pinHint_;

    int valid_;
    int currentPage_;
    int currentSlot_;
    PF_PageHandle pageHandle_;
    char *pageData_;
    RC FetchNextPage();
    int ConditionMet(char *slotData);
    int scanComplete_;
};


//
// RM_Record: RM Record interface
//
class RM_Record {
    friend class qJoin;
    friend class RM_FileHandle;
    friend class RM_FileScan;
    friend class LG_Manager;

public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

private:
    char *contents_;
    RID rid_;
    int valid_;
};

//
// Print-error function
//
void RM_PrintError(RC rc);

#define RM_RECSZINVALID    (START_RM_WARN + 1) // record size too big or small
#define RM_RECINVALID      (START_RM_WARN + 2) // record object invalid 
#define RM_FILEINVALID     (START_RM_WARN + 3) // file handle object invalid
#define RM_RECNOTEXIST     (START_RM_WARN + 4) // record does not exist
#define RM_FILESCANINVALID (START_RM_WARN + 5) // file scan object invalid
#define RM_EOF             (START_RM_WARN + 6) // no more records to be scanned
#define RM_SCANPARAMINVALID (START_RM_WARN + 7) // parameters for scanning are invalid
#define RM_SCANOPEN         (START_RM_WARN + 8) // file scan is still open
#define RM_FILENAMENULL     (START_RM_WARN + 9)
#define RM_DATANULL         (START_RM_WARN + 10)
#define RM_LASTWARN        RM_DATANULL

#define RM_LASTERROR       (END_RM_ERR)


#endif
