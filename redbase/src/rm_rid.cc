#include "rm_rid.h"




#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>

using namespace std;
//
RID::RID()
{
	valid_ = 0;
}

RID::RID(PageNum pageNum, SlotNum slotNum)
{
	if (pageNum > 0 && slotNum >= 0) {
		pageNum_ = pageNum;
		slotNum_ = slotNum;
		valid_ = 1;
	} else {
		valid_ = 0;
	}
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