#include "rm_internal.h"

RM_FileHandle::RM_FileHandle()
{
    valid_ = 0;
    hdr_ = (RM_FileHdr *) new char [sizeof(RM_FileHdr)];
}

RM_FileHandle::~RM_FileHandle() {
    delete[] hdr_;
}

RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;
    if (rec.valid_) return RM_RECNOTEMPTY; // check rec is empty to prevent overwriting

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

    unsigned short bitmap = ((RM_PageHdr *)pageData)->slotStatus;
    int slotFull = (bitmap >> slotNum) & 1;
    /* Check bitmap and ensure the slot is not empty */
    if (!slotFull) {
        PFfileHandle_.UnpinPage(pageNum);
        return RM_RECNOTEXIST;
    }
    
    /* Copy record data to rec*/
    int recSz = hdr_->recordSize;
    char *slotData = pageData + sizeof(RM_PageHdr) + recSz * slotNum;
    rec.contents_ = new char[recSz];
    memcpy(rec.contents_, slotData, recSz);
    rec.rid_ = rid;
    rec.valid_ = 1;

    PFfileHandle_.UnpinPage(pageNum);
    return 0;
}


RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
    RC rc;
    if (!valid_) return RM_FILEINVALID;

    PageNum pageNum;
    SlotNum slotNum;
    PF_PageHandle page;
    char *pageData;

    /* When there is no available page, allocate one */
    if (hdr_->firstFree == RM_PAGE_LIST_END) {
        rc = PFfileHandle_.AllocatePage(page);
        if (rc) return rc;
        
        page.GetPageNum(pageNum);
        slotNum = 0;
        page.GetData(pageData);

        /* Modify page header */
        RM_PageHdr *phdr = (RM_PageHdr*) pageData;
        phdr->numRecords = 1;
        phdr->slotStatus = 0; // Zero out bitmap

        /* Add the new page to linked list of free pages */
        phdr->nextFree = hdr_->firstFree;
        hdr_->firstFree = pageNum;

        /* Modify file header */
        hdr_->numPages++;
        hdrModified_= 1;

    /* When there is an available page, use first free page */
    } else {
        rc = PFfileHandle_.GetThisPage(hdr_->firstFree, page);
        if (rc) return rc;

        page.GetPageNum(pageNum);
        page.GetData(pageData);
        RM_PageHdr *phdr = (RM_PageHdr*) pageData;

        /* If page has no more empty slots, remove from linked list of free pages */
        if (++(phdr->numRecords) == hdr_->recordsPerPage) {
            hdr_->firstFree = phdr->nextFree;
            phdr->nextFree = RM_PAGE_FULL;
            hdrModified_= 1;
        }

        slotNum = FindAvailableSlot(((RM_PageHdr*) pageData)->slotStatus);
    }

    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_->recordSize * slotNum;
    memcpy(slotData, pData, hdr_->recordSize);

    /* Create RID object based on the free page/slot found */
    rid = RID(pageNum, slotNum);

    /* Mark record slot as full in page header bitmap */
    ((RM_PageHdr *)pageData)->slotStatus |= (1 << slotNum);

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
    phdr->slotStatus &= ~(1 << slotNum); // Mark the slot as free on bitmap
    (phdr->numRecords)--; // Decrement number of records in the page

    /* Add current page to the list of free pages */
    phdr->nextFree = hdr_->firstFree;
    hdr_->firstFree = pageNum;
    hdrModified_ = 1;

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

    unsigned short bitmap = ((RM_PageHdr *)pageData)->slotStatus;
    int slotFull = (bitmap >> slotNum) & 1;
    /* Check bitmap and ensure the slot is not empty */
    if (!slotFull) {
        PFfileHandle_.UnpinPage(pageNum);
        return RM_RECNOTEXIST;
    }
    
    /* Copy record data to rec*/
    char *slotData = pageData + sizeof(RM_PageHdr) + hdr_->recordSize * slotNum;
    memcpy(slotData, updatedData, hdr_->recordSize);
    rc = PFfileHandle_.MarkDirty(pageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(pageNum);
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

    char *pagedata;
    return page.GetData(pagedata);
}

/* Find the first available slot in a given page from its bitmap */
SlotNum RM_FileHandle::FindAvailableSlot(unsigned short bitmap) {
    SlotNum s;
    for (s = 0; s < hdr_->recordsPerPage; s++) {
        if (((bitmap >> s) & 1) == 0) return s;
    }
    return -1;
}