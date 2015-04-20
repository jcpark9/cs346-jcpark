#include "ix_internal.h"


IX_IndexScan::IX_IndexScan()
{
    valid_ = 0;
}

IX_IndexScan::~IX_IndexScan()
{
  	if (!scanComplete_) indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
}


RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT)
{
	if (valid_) return IX_SCANOPEN;
	if (!indexHandle.valid_) return IX_FILEINVALID;

	/* Check if scan parameters are valid */
	if (compOp < 0 || compOp > GE_OP || compOp == NE_OP ||
    (value == NULL && compOp != NO_OP)) return IX_SCANPARAMINVALID;


    return 0;
}

RC IX_IndexScan::GetNextEntry(RID &rid)
{
    if (!valid_) return IX_SCANINVALID;
    
    rid = rid_;
    return 0;
}

RC IX_IndexScan::CloseScan()
{
    if (!valid_) return IX_SCANINVALID;
    valid_ = 0;
  	if (!scanComplete_) indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    return 0;
}


/* Returns 1 if the condition is met for the key residing in keyData
 * returns 0 otherwise
 * returns -1 on error
 */
int RM_FileScan::ConditionMet(char *keyData) {
  int attrType = indexHandle_.hdr_.attrType;
  int attrLength = indexHandle_.hdr_.attrLength;
  int comp;
  /* Compare attribute and scan condition value */
  switch(attrType) {
    case INT:
      comp = IntCompare(keyData, value_, attrLength);
      break;
    case FLOAT:
      comp = FloatCompare(keyData, value_, attrLength);
      break;
    case STRING:
      comp = StringCompare(keyData, value_, attrLength);
      break;
    default:
      return -1;
  }

  /* Check if comp satisfies compOp_ specified in OpenScan */
  switch(compOp_) {
    case NO_OP:
      return 1;
    
    case EQ_OP:
      return (comp == 0);
    
    case LT_OP:
      return (comp < 0);

    case GT_OP:
      return (comp > 0 );

    case LE_OP:
      return (comp <= 0);

    case GE_OP:
      return (comp >= 0);
  }
  return -1;
}