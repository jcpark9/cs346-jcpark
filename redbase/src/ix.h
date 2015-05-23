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

struct IX_FileHdr;
struct IX_NodeHdr;
struct IX_LeafHdr;

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;

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
    IX_FileHdr *hdr_;                               // file header
    int hdrModified_;                               // dirty flag for file hdr
    PF_FileHandle PFfileHandle_;
    int keylen_;                                    // Length of each key (composed of data and RID) in B+ tree


    /* Helper functions */
    PageNum FindSubtreePtr(char *newKey, IX_NodeHdr *nodeHdr, char *nodeData, int &keyIndex);
    void WriteKey(char *pos, char *key);
    char* GetNthKeyInternal(char *nodeData, int keyIndex);
    char* GetNthKeyLeaf(char *nodeData, int keyIndex);

    /* Insert functions */
    RC InitializeFirstLeaf(PageNum &newLeafPageNum);
    RC InsertEntryToNode(char *key, PageNum currentNode, PageNum &newChild, char *newChildData);
    RC InsertEntryToNonLeaf(PageNum &newChild, char *newChildData, IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex);
    void InsertEntryToNonLeafHelper(PageNum ptr, char *key, IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex);
    RC InsertEntryToLeaf(char *newKey, IX_LeafHdr *leafHdr, char *leafData, PageNum &newChild, char *newChildData, PageNum currentPage);
    RC InsertEntryToLeafHelper(char *newKey, IX_LeafHdr *leafHdr, char *leafData);
    RC AllocateNewRoot(PageNum secondChild, char *secondChildKey);

    /* Delete functions */
    RC DeleteEntryFromNode(char *key, PageNum currentNode, int &childDeleted);
    void DeleteEntryFromNonLeaf(IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex);
    RC DeleteEntryFromLeaf(char *deletedKey, IX_LeafHdr *leafHdr, char *leafData);
    RC AdjustSiblingPointers(IX_LeafHdr *leafHdr);
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
    IX_IndexHandle indexHandle_;
    CompOp compOp_;
    void *value_;
    ClientHint pinHint_;

    int valid_;
    int scanComplete_;      // Flag for whether scan is complete or active

    int currentPage_;       // PageNum of the page where current scan pointer is
    int currentKeyIndex_;   // Index of the key that current scan pointer points to
    char *nodeData_;        // Data of the page being scanned by current scan pointer
    int keylen_;            // Length of each key in B+ tree

    char *lastKeySeen_;     // Key that was last seen
    int nextLeaf_;          // PageNum of the next leaf page
    int firstEntryScanned_; // Flag for whether first entry has been scanned (Used for initializing scan pointer)
    
    RC FetchNextPage(PageNum next);
    int ConditionMet(char *key1, char *key2);
    RC InitializeScanPtr();
    RC TreeSearch(char *key, PageNum current, PageNum &found);
    PageNum FindSubtreePtr(char *newKey, IX_NodeHdr *nodeHdr, char *nodeData);
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
    PF_Manager *pfm_;
};

//
// Print-error function
//
void IX_PrintError(RC rc);
#define IX_CREATEPARAMINVALID    (START_IX_WARN + 0) // parameters for index creation are invalid
#define IX_FILENAMENULL          (START_IX_WARN + 1) // filename provided is null
#define IX_FILEINVALID           (START_IX_WARN + 2) // index file handle object invalid
#define IX_NULLDATA              (START_IX_WARN + 3) // pData pointer is NULL
#define IX_SCANINVALID           (START_IX_WARN + 4) // index scan object invalid
#define IX_EOF                   (START_IX_WARN + 5) // no more indices to be scanned
#define IX_SCANPARAMINVALID      (START_IX_WARN + 6) // parameters for scanning are invalid
#define IX_SCANOPEN              (START_IX_WARN + 7) // index scan is still open
#define IX_ENTRYNOTFOUND         (START_IX_WARN + 8) // record couldn't be found for deletion
#define IX_DUPLICATEENTRY        (START_IX_WARN + 9) // there is a duplicate entry

#define IX_LASTWARN        IX_DUPLICATEENTRY

#define IX_LASTERROR       (END_IX_ERR)

#endif
