#include "rm_internal.h"
#include <stdint.h>
#include <assert.h>

RM_FileScan::RM_FileScan() { 
  valid_ = 0;
}

RM_FileScan::~RM_FileScan() {
}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint)
{
  if (!fileHandle_.valid_) return RM_FILEINVALID;

  /* Save parameters passed to the function */
  fileHandle_ = fileHandle;
  attrType_ = attrType;
  attrLength_ = attrLength;
  attrOffset_ = attrOffset_;
  compOp_ = compOp;
  value_ = value;
  pinHint_ = pinHint;
  
  /* Initialize scan */
  valid_ = 1;
  currentPage_ = HEADER_PAGENUM;
  currentSlot_ = 0;
  return FetchNextPage();
}

RC RM_FileScan::GetNextRec(RM_Record &rec)
{
  if (!valid_) return RM_FILESCANINVALID;

  char * pageData;
  pageHandle_.GetData(pageData);
  RM_PageHdr *phdr = (RM_PageHdr *) pageData;
  unsigned short bitmap = phdr->slotStatus;
  int recSz = (fileHandle_.hdr_)->recordSize;

  while (1) {
    // Pin the next page if all records in current page are scanned
    if (currentSlot_ == (fileHandle_.hdr_)->recordsPerPage) {
      RC rc = FetchNextPage();
      if (rc) return rc;
      pageHandle_.GetData(pageData);
      bitmap = phdr->slotStatus;
    }

    // If the slot is not empty
    if ((bitmap >> currentSlot_) & 1) {
      char *slotData = pageData + sizeof(RM_PageHdr) + recSz * currentSlot_;

      // Check if the condition is met and copy record data
      if (value_ == NULL || ConditionMet(slotData)) {
        rec.rid_ = RID(currentPage_, currentSlot_);

        rec.contents_ = new char[recSz];
        rec.valid_ = 1;
        memcpy(rec.contents_, slotData, recSz);
        return 0;
      }
    }

    currentSlot_++;
  }
}

/* Fetch the next page containing some records */
RC RM_FileScan::FetchNextPage() {
  while (1) {
    /* Unpin the previous page */
    if (currentPage_ != HEADER_PAGENUM) fileHandle_.PFfileHandle_.UnpinPage(currentPage_);
    /* Get next page */
    RC rc = fileHandle_.PFfileHandle_.GetNextPage(currentPage_, pageHandle_);
    if (rc == PF_EOF) return RM_EOF;
    else if (rc) return rc;

    pageHandle_.GetPageNum(currentPage_);
    char *pageData;
    pageHandle_.GetData(pageData);
  
    RM_PageHdr *phdr = (RM_PageHdr *) pageData;
    /* Return if there is at least one record to scan */
    if (phdr->numRecords > 0) {
      currentSlot_ = 0;
      return 0;
    }
  }  
}

int RM_FileScan::StringCompare(void *rec, void *val, int n) {
  return strncmp((char *)rec, (char *)val, n);
}

int RM_FileScan::IntCompare(void *rec, void *val, int n) {
  assert(sizeof(int32_t) == n);
  int32_t rec_v, val_v;
  memcpy(&val_v, val, n);
  memcpy(&rec_v, rec, n);

  if (rec_v == val_v) return 0;
  if (rec_v < val_v) return -1;
  return 1;
}

int RM_FileScan::FloatCompare(void *rec, void *val, int n) {
  assert(sizeof(float) == n);
  float rec_v, val_v;
  memcpy(&val_v, val, n);
  memcpy(&rec_v, rec, n);

  if (rec_v == val_v) return 0;
  if (rec_v < val_v) return -1;
  return 1;
}

/* Returns 1 if the condition is met for the record in slotData */
int RM_FileScan::ConditionMet(char *slotData) {
  /* Fetch data for the attribute */
  void *attrData = (void *)(slotData + attrOffset_);
  int comp;
  /* Compare attribute and scan condition value */
  switch(attrType_) {
    case INT:
      comp = IntCompare(attrData, value_, attrLength_);
      break;
    case FLOAT:
      comp = FloatCompare(attrData, value_, attrLength_);
      break;
    case STRING:
      comp = StringCompare(attrData, value_, attrLength_);
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
    
    case NE_OP:
      return (comp != 0);
    
    case LT_OP:
      return (comp == -1);

    case GT_OP:
      return (comp == 1);

    case LE_OP:
      return (comp == -1 || comp == 0);

    case GE_OP:
      return (comp == 1 || comp == 0);
  }
  return -1;
}

RC RM_FileScan::CloseScan()
{
  if (!valid_) return RM_FILESCANINVALID;
  valid_ = 0;
  return 0;
}