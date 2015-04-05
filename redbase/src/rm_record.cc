#include "rm.h"
#include "rm_rid.h"

RM_Record::RM_Record()
{
    valid_ = 0;
    contents_ = NULL;
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
    if (valid_) delete contents_;
}


RC RM_Record::RM_GetData(char *&pData) const
{
    if (!valid_) return RM_RECINVALID;

    pData = contents_;
    return 0;
}

RC RM_Record::GetRid(RID &rid) const
{
    if (!valid_) return RM_RECINVALID;
    
    rid = rid_;
    return 0;
}
