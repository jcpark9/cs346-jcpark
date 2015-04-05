#include <string.h>

#include "rm.h"
#include "rm_internal.h"

RM_FileHandle::RM_FileHandle()
{
    valid_ = 0;
}

RM_FileHandle::~RM_FileHandle()
{
}

RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
    if (!valid_) return RM_FILEINVALID;
    if (!rid.valid) return RM_RIDINVALID;

    PF_PageHandle page;
    RC rc = fileHandle_.GetThisPage(rid.pageNum_, page);
    if (rc) return rc;

    char *data;
    rc = page.GetData(data);
    if (rc) return rc;

    if (struct )

    if (rec) return RM_RECORDNOTEXIST;


    rec.contents = new char[hdr_.recordSize];
    memcpy(rec.contents, data, hdr_.recordSize);
    rec.rid = rid;
    rec.valid_ = 1;
    return 0;
}


RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
    if (!rid.invalid) return RM_RIDINVALID;
    


    rid = ;
    pageNum = INVALID_PAGE;
    pPageData = NULL;
}



RC RM_FileHandle::DeleteRec(const RID &rid)
{
      pageNum = INVALID_PAGE;
      pPageData = NULL;
}


RC RM_FileHandle::UpdateRec(const RM_Record &rec)
{
    if (!valid_) return RM_FILEINVALID;
    if (!rid.valid) return RM_RIDINVALID;


    PF_PageHandle page;
    RC rc = fileHandle_.GetThisPage(rid.pageNum_, page);
    if (rc) return rc;

    char *data;
    rc = page.GetData(data);
    if (rc) return rc;


  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RM_FileHandle::ForcePages(PageNum pageNum = ALL_PAGES)
{
    filehandle_.ForcePages(pageNum);
}

char *GetSlot(char *pagedata, int slotNum) {
    int bytePos = slotNum / BYTELENGTH;
    int bitPos = slotNum % BYTELENGTH;

    RM_PageHdr *hdr = (RM_PageHdr*)pagedata;
    int slotFull = ((hdr->slotsFull)[bytePos] >> bitPos) & 1;



    if (slotFull) return pagedata + hdr_.bitmapLength + hdr_.recordSize * slotNum;
    return NULL;
}