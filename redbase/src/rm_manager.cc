#include "rm.h"
#include <assert.h>

#define BYTELENGTH

RM_Manager::RM_Manager(PF_Manager &pfm)
{
    pfmanager_ = new PF_Manager;
}

RM_Manager::~RM_Manager()
{
    delete pfmanager_;
}


RC RM_Manager::CreateFile(const char *fileName, int recordSize)
{
    if (recordSize > PF_PAGE_SIZE) return RF_TOOBIG;
    RC rc = pfmanager_->CreateFile(fileName);
    if (rc) return rc;

    int numRecords = (BYTELENGTH * PF_PAGE_SIZE)/(1 + BYTELENGTH * recordSize); // x/8 + sz*x = PF_PAGE_SIZE
    int bitmapLength = numRecords / BYTELENGTH;
    if (numRecords % BYTELENGTH > 0) bitmapLength++;
    assert(recordSize * numRecords + bitmapLength <= PF_PAGE_SIZE);


    rc = pfmanager_->OpenFile(fileName, PF_FileHandle &fileHandle);
    PF_PageHandle headerHandle;
    pfmanager_->

   PF_FileHdr *hdr = (PF_FileHdr*)hdrBuf;
   hdr->firstFree = PF_PAGE_LIST_END;
   hdr->numPages = 0;

    return 0;
}


RC RM_Manager::DestroyFile(const char *fileName)
{
    return pfmanager_->DestroyFile(fileName);
}


RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle)
{
    RC rc = pfmanager_->OpenFile(fileName, fileHandle.fileHandle_);
    if (rc) return rc;
    fileHandle.valid = 1;
}


RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
    return pfmanager_->CloseFile(fileHandle);
}