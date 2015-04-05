#include "rm_rid.h"

RID::RID()
{
	valid_ = 0;
}

RID::RID(PageNum pageNum, SlotNum slotNum)
{
	pageNum_ = pageNum;
	slotNum_ = slotNum;
	valid_ = 1;
}

RID::~RID() { }

RC RID::GetPageNum(PageNum &pageNum) const
{
	if (!valid_) return RM_RIDINVALID;

	pageNum = pageNum_;
	return 0;
}

RC RID::GetSlotNum(SlotNum &slotNum) const
{
	if (!valid_) return RM_RIDINVALID;
	
	slotNum = slotNum_;
	return 0;
}