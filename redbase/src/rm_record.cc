#include "rm_internal.h"
#include "rm_rid.h"

RM_Record::RM_Record()
{
    valid_ = 0;
    contents_ = NULL;
}

RM_Record::~RM_Record()
{
    if (valid_) delete[] contents_; // free memory if Record used to hold data
}


RC RM_Record::GetData(char *&pData) const
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
