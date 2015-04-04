#include "rm_rid.h"

RID::RID()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RID::RID(PageNum pageNum, SlotNum slotNum)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RID::~RID()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RID::GetPageNum(PageNum &pageNum) const
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RID::GetSlotNum(SlotNum &slotNum) const
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}