#include "rm_internal.h"
#include "comp.h"
#include <stdint.h>
#include <cstdio>
#include <assert.h>

int SlotFull(int slotNum, char *pageData);

RM_FileScan::RM_FileScan() { 
  valid_ = 0;
}

RM_FileScan::~RM_FileScan() {
  // Unpin the current page if the object is being destroyed amid scanning
  if (valid_ && !scanComplete_) fileHandle_.PFfileHandle_.UnpinPage(currentPage_);
}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint)
{
  if (valid_) return RM_SCANOPEN;
  if (!fileHandle.valid_) return RM_FILEINVALID;
  /* Check if scan parameters are valid */
  if (attrLength <= 0 || attrLength > MAXSTRINGLEN ||
    attrOffset < 0 || attrOffset >= fileHandle.hdr_.recordSize ||
    attrType < 0 || attrType > STRING ||
    compOp < 0 || compOp > GE_OP ||
    (value == NULL && compOp != NO_OP) ||
    ((attrType == INT || attrType == FLOAT) && attrLength != NUMLEN)) return RM_SCANPARAMINVALID;

  /* Save parameters passed to the function */
  fileHandle_ = fileHandle;
  attrType_ = attrType;
  attrLength_ = attrLength;
  attrOffset_ = attrOffset;
  compOp_ = compOp;
  value_ = value;
  pinHint_ = pinHint;
  
  /* Initialize scan */
  valid_ = 1;
  currentPage_ = HEADER_PAGENUM;
  scanComplete_ = 0;
  currentSlot_ = 0;
  return FetchNextPage();
}

RC RM_FileScan::GetNextRec(RM_Record &rec)
{
  if (!valid_) return RM_FILESCANINVALID;
  if (scanComplete_) return RM_EOF;
  //printf("WTF1\n");

  int recSz = (fileHandle_.hdr_).recordSize;

  while (1) {
    // Pin the next page if all records in current page are scanned
    if (currentSlot_ >= (fileHandle_.hdr_).recordsPerPage) {
      RC rc = FetchNextPage();
      if (rc) return rc;
      currentSlot_ = 0;
    }

    // If the slot is not empty
    if (SlotFull(currentSlot_, pageData_)) {
      int slotOffset = sizeof(RM_PageHdr) + (fileHandle_.hdr_).bitmapSize + recSz * currentSlot_;
      char *slotData = pageData_ + slotOffset;

      // Check if the condition is met and copy record data
      if (value_ == NULL || ConditionMet(slotData)) {
        rec.rid_ = RID(currentPage_, currentSlot_++);

        //printf("valid?: %d\n", rec.valid_);

        if (rec.valid_) {
          //printf("%s\n", rec.contents_);
          delete[] rec.contents_; // delete contents if record object already contains another record
        }
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
  int openScan = ((currentPage_ == HEADER_PAGENUM) ? 1 : 0);

  while (1) {

    /* Unpin the previous page */
    if (currentPage_ != HEADER_PAGENUM) fileHandle_.PFfileHandle_.UnpinPage(currentPage_);
    /* Get next page */
    RC rc = fileHandle_.PFfileHandle_.GetNextPage(currentPage_, pageHandle_);
    if (rc == PF_EOF) {
      scanComplete_ = 1;
      if (!openScan) return RM_EOF;
      else return 0;
    }
    else if (rc) return rc;

    pageHandle_.GetPageNum(currentPage_);
    pageHandle_.GetData(pageData_);
  
    RM_PageHdr *phdr = (RM_PageHdr *) pageData_;
    /* Return if there is at least one record to scan */
    if (phdr->numRecords > 0) return 0;
  }  
}

/* Returns 1 if the condition is met for the record residing in slotData 
 * returns 0 otherwise
 * returns -1 on error
 */
int RM_FileScan::ConditionMet(char *slotData) {
  /* Fetch data for the attribute */
  char *attrData = slotData + attrOffset_;
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

RC RM_FileScan::CloseScan()
{
  if (!valid_) return RM_FILESCANINVALID;
  valid_ = 0;
  if (!scanComplete_) {
    fileHandle_.PFfileHandle_.UnpinPage(currentPage_);
    scanComplete_ = 1;
  }
  return 0;
}

/* Checks whether the slot is full by looking at the bitmap */
int SlotFull(int slotNum, char *pageData) {
    unsigned char *bitmap = (unsigned char *) (pageData + sizeof(RM_PageHdr));

    int bytePos = slotNum / BYTELEN;
    int bitPos = slotNum % BYTELEN;

    return (bitmap[bytePos] >> bitPos) & 1;
}