#include "rm.h"

RM_FileHandle::RM_FileHandle()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RM_FileHandle::~RM_FileHandle()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}


RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
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
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RM_FileHandle::ForcePages(PageNum pageNum = ALL_PAGES)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}