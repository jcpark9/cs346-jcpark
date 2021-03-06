#include "rm_internal.h"
#include "lg.h"

enum Mode {GET,SET,CLEAR};
int GetSlotBit(int slotNum, char *pageData, Mode mode);

RM_FileHandle::RM_FileHandle()
{
    valid_ = 0;
}

RM_FileHandle::~RM_FileHandle() {
}

RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;

    /* Get PageNum and SlotNum from rid */
    PageNum pageNum;
    rc = rid.GetPageNum(pageNum);
    if (rc) return rc;
    SlotNum slotNum;
    rc = rid.GetSlotNum(slotNum);
    if (rc) return rc;
    
    /* Get data of the page that record resides on */
    char *pageData;
    rc = GetPageData(pageNum, pageData);
    if (rc) return rc;

    /* Check bitmap and ensure the slot is not empty */
    if (!GetSlotBit(slotNum, pageData, GET)) {
        PFfileHandle_.UnpinPage(pageNum);
        return RM_RECNOTEXIST;
    }
    
    /* Copy record data to rec*/
    int recSz = hdr_.recordSize;
    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_.bitmapSize  + recSz * slotNum;
    
    if (rec.valid_) delete[] rec.contents_;
    rec.contents_ = new char[recSz];
    
    memcpy(rec.contents_, slotData, recSz);
    rec.rid_ = rid;
    rec.valid_ = 1;

    return PFfileHandle_.UnpinPage(pageNum);
}


RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;
    if (pData == NULL) return RM_DATANULL;

    PageNum pageNum;
    SlotNum slotNum;
    PF_PageHandle page;
    char *pageData;
    RM_PageHdr *phdr;

    /* When there is no available page, allocate one */
    if (hdr_.firstFree == RM_PAGE_LIST_END) {
        rc = PFfileHandle_.AllocatePage(page);
        if (rc) return rc;
        
        page.GetPageNum(pageNum);
        slotNum = 0;
        page.GetData(pageData);

        /* Initialize page header */
        phdr = (RM_PageHdr*) pageData;
        phdr->numRecords = 0;
        char *bitmap = pageData + sizeof(RM_PageHdr);
        memset(bitmap, 0, hdr_.bitmapSize); // Zero out bitmap for slot status

        /* Add the new page to linked list of free pages */
        phdr->nextFree = RM_PAGE_LIST_END;
        hdr_.firstFree = pageNum;

        /* Modify file header */
        hdr_.numPages++;
        hdrModified_= 1;

    /* When there is an available page, use first free page */
    } else {
        rc = PFfileHandle_.GetThisPage(hdr_.firstFree, page);
        if (rc) return rc;

        page.GetPageNum(pageNum);
        page.GetData(pageData);
        slotNum = FindAvailableSlot(pageData);
    }

    phdr = (RM_PageHdr*) pageData;
    /* If page has no more empty slots, remove from linked list of free pages */
    if (++(phdr->numRecords) == hdr_.recordsPerPage) {
        hdr_.firstFree = phdr->nextFree;
        phdr->nextFree = RM_PAGE_FULL;
        hdrModified_= 1;
    }

    /* Create RID object based on the free page/slot found */
    rid = RID(pageNum, slotNum);

    /* Insert a log record */
    if (strncmp(filename, "attrcat", MAXNAME) && strncmp(filename, "relcat", MAXNAME)) {
        LG_FullRec logRec;
        memset(&logRec, 0, sizeof(LG_FullRec));
        logRec.type = L_INSERT;
        logRec.offset = 0;
        logRec.dataSize = GetRecordSize();
        strncpy(logRec.fileName, filename, MAXNAME);
        logRec.rid = rid;
        if ((rc = lgm_->InsertLogRec(logRec, NULL, pData))) { return rc; }
        if ((rc = UpdatePageLSN(rid, logRec.lsn))) { return rc; }
    }
    
    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_.bitmapSize + hdr_.recordSize * slotNum;
    memcpy(slotData, pData, hdr_.recordSize);

    /* Mark record slot as full in page header bitmap */
    GetSlotBit(slotNum, pageData, SET);

    /* Mark the modified page as dirty and unpin it */
    rc = PFfileHandle_.MarkDirty(pageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(pageNum);
}



RC RM_FileHandle::DeleteRec(const RID &rid)
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;

    /* Get PageNum and SlotNum from rid */
    PageNum pageNum;
    rc = rid.GetPageNum(pageNum);
    if (rc) return rc;
    SlotNum slotNum;
    rc = rid.GetSlotNum(slotNum);
    if (rc) return rc;
    
    /* Get data of the page that record resides on */
    char *pageData;
    rc = GetPageData(pageNum, pageData);
    if (rc) return rc;

    RM_PageHdr *phdr = (RM_PageHdr *)pageData;
    GetSlotBit(slotNum, pageData, CLEAR); // Mark the slot as free on bitmap

    // Decrement number of records in the page
    if ((phdr->numRecords)-- == hdr_.recordsPerPage) {
        /* Add current page to the list of free pages if it is now free */
        phdr->nextFree = hdr_.firstFree;
        hdr_.firstFree = pageNum;
        hdrModified_ = 1;
    }; 

    rc = PFfileHandle_.MarkDirty(pageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(pageNum);
}


RC RM_FileHandle::UpdateRec(const RM_Record &rec)
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;

    /* Extract RID and Updated Data from Rec */
    if (!rec.valid_) return RM_RECINVALID;
    char *updatedData = rec.contents_;
    RID rid = rec.rid_;

    /* Extract PageNum and SlotNum*/
    PageNum pageNum;
    rc = rid.GetPageNum(pageNum);
    if (rc) return rc;
    SlotNum slotNum;
    rc = rid.GetSlotNum(slotNum);
    if (rc) return rc;

    /* Get data of the page that record resides on */
    char *pageData;
    rc = GetPageData(pageNum, pageData);
    if (rc) return rc;

    /* Check bitmap and ensure the slot is not empty */
    if (!GetSlotBit(slotNum, pageData, GET)) {
        PFfileHandle_.UnpinPage(pageNum);
        return RM_RECNOTEXIST;
    }
    
    /* Copy record data to rec*/
    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_.bitmapSize + hdr_.recordSize * slotNum;
    memcpy(slotData, updatedData, hdr_.recordSize);
    rc = PFfileHandle_.MarkDirty(pageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(pageNum);
}

/* Insert recData at position indicated by rid */
RC RM_FileHandle::InsertRecAtRid(const char *recData, const RID &rid)
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;

    /* Extract PageNum and SlotNum*/
    PageNum pageNum;
    rc = rid.GetPageNum(pageNum);
    if (rc) return rc;
    SlotNum slotNum;
    rc = rid.GetSlotNum(slotNum);
    if (rc) return rc;

    /* Get data of the page that record resides on */
    char *pageData;
    rc = GetPageData(pageNum, pageData);
    if (rc) return rc;
    
    /* Copy record data to rec*/
    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_.bitmapSize + hdr_.recordSize * slotNum;
    memcpy(slotData, recData, hdr_.recordSize);

    GetSlotBit(slotNum, pageData, SET);

    rc = PFfileHandle_.MarkDirty(pageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(pageNum);
}

/* Returns fixed size of the records in this file */
int RM_FileHandle::GetRecordSize() {
    return hdr_.recordSize;
}

/* Returns pageLSN (last log record that touched this file) 
   in the page header */
RC RM_FileHandle::GetPageLSN(RID rid, LSN &lsn) {
    PageNum pn; char *pData;
    rid.GetPageNum(pn);

    PF_PageHandle page;
    RC rc = PFfileHandle_.GetThisPage(pn, page);
    if (rc) return rc;
    page.GetData(pData);

    RM_PageHdr *phdr = (RM_PageHdr *) pData;
    lsn = phdr->pageLSN;

    return PFfileHandle_.UnpinPage(pn);
}


RC RM_FileHandle::ForcePages(PageNum pageNum)
{
    if (!valid_) return RM_FILEINVALID;
    return PFfileHandle_.ForcePages(pageNum);
}

/* When function exits, pageData refers to data
   of the page specified by pageNum */
RC RM_FileHandle::GetPageData(PageNum pageNum, char *&pageData) const {
    PF_PageHandle page;
    RC rc = PFfileHandle_.GetThisPage(pageNum, page);
    if (rc) return rc;

    return page.GetData(pageData);
}

/* Find the first available slot in a given page from its bitmap */
SlotNum RM_FileHandle::FindAvailableSlot(char *pageData) {
    SlotNum s;
    for (s = 0; s < hdr_.recordsPerPage; s++) {
        if (!GetSlotBit(s, pageData, GET)) return s;
    }
    return -1;
}

/* Checks or modifies the bit in slot availability bitmap 
 * if mode is 'GET', checks the bit and returns it
 * if mode is 'CLEAR', marks the slot as empty
 * if mode is 'SET', marks the slot as full
 */
int GetSlotBit(int slotNum, char *pageData, Mode mode) {
    unsigned char *bitmap = (unsigned char *) (pageData + sizeof(RM_PageHdr));

    int bytePos = slotNum / BYTELEN;
    int bitPos = slotNum % BYTELEN;

    if (mode == GET) {
        return ((bitmap[bytePos] >> bitPos) & 1);
    } else if (mode == SET) {
        (bitmap[bytePos]) |= (1 << bitPos);
    } else if (mode == CLEAR) {
        (bitmap[bytePos]) &= ~(1 << bitPos);
    }

    return 0;
}

/* Updates the page LSN in the page header of the page indicated by rid */
RC RM_FileHandle::UpdatePageLSN(const RID &rid, const LSN &lsn) {
    RC rc; PageNum pn;
    if ((rc = rid.GetPageNum(pn))) return rc;

    PF_PageHandle dataph;
    if ((rc = PFfileHandle_.GetThisPage(pn, dataph))) return rc; 
    
    char *pData;
    dataph.GetData(pData);
    
    RM_PageHdr *phdr = (RM_PageHdr *) pData;
    phdr->pageLSN = lsn;

    if ((rc = PFfileHandle_.MarkDirty(pn))) return rc;
    if ((rc = PFfileHandle_.UnpinPage(pn))) return rc;
    return 0;
}