#include "rm.h"

RM_Record::RM_Record()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

//
// ~PF_PageHandle
//
// Desc: Destroy the page handle object.
//       If the page handle object refers to a pinned page, the page will
//       NOT be unpinned.
//
RM_Record::~RM_Record()
{
  // Don't need to do anything
}

//
// PF_PageHandle
//
// Desc: Copy constructor
//       If the incoming page handle object refers to a pinned page,
//       the page will NOT be pinned again.
// In:   pageHandle - page handle object from which to construct this object
//
RC RM_Record::RM_GetData(char *&pData) const
{
  // Just copy the local variables since there is no local memory
  // allocation involved
  this->pageNum = pageHandle.pageNum;
  this->pPageData = pageHandle.pPageData;
}

RC RM_Record::GetRid(RID &rid) const
{
  // Just copy the local variables since there is no local memory
  // allocation involved
  this->pageNum = pageHandle.pageNum;
  this->pPageData = pageHandle.pPageData;
}
