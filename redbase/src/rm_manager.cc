#include "rm_internal.h"
#include "lg.h"
#include <assert.h>


RM_Manager::RM_Manager(PF_Manager &pfm)
{
    pfmanager_ = &(pfm);
}

RM_Manager::~RM_Manager() { }


RC RM_Manager::CreateFile(const char *fileName, int recordSize)
{
    if (recordSize <=0 || recordSize >= PF_PAGE_SIZE) return RM_RECSZINVALID;
    if (fileName == NULL) return RM_FILENAMENULL;

    RC rc = pfmanager_->CreateFile(fileName);
    if (rc) return rc;

    /* Calculate file header fields */
    RM_FileHdr fileHdr;
    fileHdr.recordsPerPage = BYTELEN * (PF_PAGE_SIZE - sizeof(RM_PageHdr)) / (1 + BYTELEN * recordSize);
    if (fileHdr.recordsPerPage % BYTELEN == 0) {
        fileHdr.bitmapSize = fileHdr.recordsPerPage/BYTELEN;
    } else {
        fileHdr.bitmapSize = fileHdr.recordsPerPage/BYTELEN + 1;     
    }
    assert(sizeof(RM_PageHdr) + fileHdr.bitmapSize + fileHdr.recordsPerPage * recordSize <= PF_PAGE_SIZE);
    fileHdr.recordSize = recordSize;
    fileHdr.numPages = 1; // 1 header page allocated for new file
    fileHdr.firstFree = RM_PAGE_LIST_END; // No free data pages for new file

    /* Open the new file and allocate a new page for header */
    PF_FileHandle newFileHandle;
    rc = pfmanager_->OpenFile(fileName, newFileHandle);

    /* Allocate a new page for file header */
    PF_PageHandle headerPageHandle;
    rc = newFileHandle.AllocatePage(headerPageHandle);
    if (rc) return rc;

    /* Copy file header onto header page */
    char *headerPageData;
    headerPageHandle.GetData(headerPageData);
    memcpy(headerPageData, &fileHdr, sizeof(RM_FileHdr));

    /* Mark header page as dirty and unpin it */
    rc = newFileHandle.MarkDirty(HEADER_PAGENUM);
    if (rc) return rc;
    rc = newFileHandle.UnpinPage(HEADER_PAGENUM);
    if (rc) return rc;
    return pfmanager_->CloseFile(newFileHandle);
}


RC RM_Manager::DestroyFile(const char *fileName)
{
    if (fileName == NULL) return RM_FILENAMENULL;
    return pfmanager_->DestroyFile(fileName);
}


RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle)
{
    if (fileName == NULL) return RM_FILENAMENULL;
    RC rc = pfmanager_->OpenFile(fileName, fileHandle.PFfileHandle_);
    if (rc) return rc;

    /* Fetch header page */
    PF_PageHandle headerPageHandle;
    rc = fileHandle.PFfileHandle_.GetFirstPage(headerPageHandle);
    if (rc) return rc;

    fileHandle.lgm_ = lgmanager_;
    strncpy(fileHandle.filename, fileName, MAXNAME);

    /* Copy file header to FileHandle object */
    char *headerPageData;
    headerPageHandle.GetData(headerPageData);
    memcpy(&(fileHandle.hdr_), headerPageData, sizeof(RM_FileHdr));

    fileHandle.PFfileHandle_.UnpinPage(HEADER_PAGENUM);
    fileHandle.valid_ = 1;
    fileHandle.hdrModified_ = 0;
    return 0;
}


RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
    RC rc;
    if (!fileHandle.valid_) return RM_FILEINVALID;
    /* Write file header if modified */
    if (fileHandle.hdrModified_) {
        PF_PageHandle headerPageHandle;
        rc = fileHandle.PFfileHandle_.GetFirstPage(headerPageHandle);
        if (rc) return rc;

        /* Copy file header onto header page */
        char *headerPageData;
        headerPageHandle.GetData(headerPageData);
        memcpy(headerPageData, &(fileHandle.hdr_), sizeof(RM_FileHdr));

        /* Mark header page as dirty and unpin it */
        fileHandle.PFfileHandle_.MarkDirty(HEADER_PAGENUM);
        fileHandle.PFfileHandle_.UnpinPage(HEADER_PAGENUM);
    }

    rc = pfmanager_->CloseFile(fileHandle.PFfileHandle_);
    if (rc) return rc;
    
    fileHandle.valid_ = 0;
    fileHandle.hdrModified_ = 0;
    return 0;
}