#include "rm.h"


RM_FileScan::RM_FileScan()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RM_FileScan::~RM_FileScan()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}

RC RM_FileScan::CloseScan()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}