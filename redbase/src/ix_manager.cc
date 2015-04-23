#include <stdio.h>
#include "ix_internal.h"

IX_Manager::IX_Manager(PF_Manager &pfm)
{
	pfm_ = &pfm;
}

IX_Manager::~IX_Manager() { }


RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength)
{
    if (fileName == NULL) return IX_FILENAMENULL;

	RC rc;
	if (indexNo < 0 || attrLength <= 0 || attrLength > MAXSTRINGLEN ||
		attrType < 0 || attrType > STRING || 
		((attrType == FLOAT || attrType == INT) && (attrLength != NUMLEN))) {
		return IX_CREATEPARAMINVALID;
	}

    char indexfileName [strlen(fileName) + 15];
    sprintf(indexfileName, "%s.%d", fileName, indexNo);

	rc = pfm_->CreateFile(indexfileName);
	if (rc) return rc;

	/* Fill out Index File Header */
	IX_FileHdr fileHdr;
    fileHdr.maxKeysInternal = (PF_PAGE_SIZE - sizeof(IX_NodeHdr)) / (attrLength + sizeof(RID) + sizeof(PageNum));
    fileHdr.maxKeysLeaf = (PF_PAGE_SIZE - sizeof(IX_LeafHdr)) / (attrLength + sizeof(RID));
    fileHdr.attrType = attrType;
    fileHdr.attrLength = attrLength;
    fileHdr.numKeys = 0;

    /* Open the new file and allocate a new page for index file header */
    PF_FileHandle newFileHandle;
    PageNum pgnum;
    rc = pfm_->OpenFile(indexfileName, newFileHandle);
    if (rc) return rc;

    PF_PageHandle headerPageHandle;
    rc = newFileHandle.AllocatePage(headerPageHandle);
    if (rc) return rc;
    headerPageHandle.GetPageNum(pgnum);
    
    // Allocate a new page for root node
    PF_PageHandle rootPageHandle;
    PageNum rootPageNum;
    rc = newFileHandle.AllocatePage(rootPageHandle);
    if (rc) return rc;
    rootPageHandle.GetPageNum(rootPageNum);

    // Initialize root header
    char *rootPageData;
    rootPageHandle.GetData(rootPageData);
    IX_NodeHdr *rootHdr = (IX_NodeHdr *) rootPageData;
    rootHdr->nodeType = root;
    rootHdr->numKeys = 0;
    rootHdr->numChild = 0;

    // Mark root page as dirty and unpin it
    rc = newFileHandle.MarkDirty(rootPageNum);
    if (rc) return rc;
    rc = newFileHandle.UnpinPage(rootPageNum);
    if (rc) return rc;

    /* Copy file header onto header page */
    fileHdr.rootPage = rootPageNum;
    char *headerPageData;
    headerPageHandle.GetData(headerPageData);
    memcpy(headerPageData, &fileHdr, sizeof(IX_FileHdr));

    /* Mark header page as dirty and unpin it */
    rc = newFileHandle.MarkDirty(HEADER_PAGENUM);
    if (rc) return rc;
    rc = newFileHandle.UnpinPage(HEADER_PAGENUM);
    if (rc) return rc;
    return pfm_->CloseFile(newFileHandle);
}

RC IX_Manager::DestroyIndex(const char *fileName, int indexNo)
{
    if (fileName == NULL) return IX_FILENAMENULL;

    char indexfileName [strlen(fileName) + 15];
    sprintf(indexfileName, "%s.%d", fileName, indexNo);
	return pfm_->DestroyFile(indexfileName);
}

RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle)
{
    if (fileName == NULL) return IX_FILENAMENULL;

    char indexfileName [strlen(fileName) + 15];
    sprintf(indexfileName, "%s.%d", fileName, indexNo);
    
    RC rc = pfm_->OpenFile(indexfileName, indexHandle.PFfileHandle_);
    if (rc) return rc;

    /* Fetch header page */
    PF_PageHandle headerPageHandle;
    rc = indexHandle.PFfileHandle_.GetFirstPage(headerPageHandle);
    if (rc) return rc;

    /* Copy file header to FileHandle object */
    char *headerPageData;
    headerPageHandle.GetData(headerPageData);
    memcpy(&(indexHandle.hdr_), headerPageData, sizeof(IX_FileHdr));

    indexHandle.PFfileHandle_.UnpinPage(HEADER_PAGENUM);
    indexHandle.valid_ = 1;
    indexHandle.hdrModified_ = 0;
    indexHandle.keylen_ = indexHandle.hdr_.attrLength + sizeof(RID);

    return 0;
}

RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    RC rc;
    if (!indexHandle.valid_) return IX_FILEINVALID;
    if (indexHandle.scans_ != 0) return IX_SCANOPEN;
    /* Write index file header if modified */
    if (indexHandle.hdrModified_) {
        PF_PageHandle headerPageHandle;
        rc = indexHandle.PFfileHandle_.GetFirstPage(headerPageHandle);
        if (rc) return rc;

        /* Copy file header onto header page */
        char *headerPageData;
        headerPageHandle.GetData(headerPageData);
        memcpy(headerPageData, &(indexHandle.hdr_), sizeof(IX_FileHdr));

        /* Mark header page as dirty and unpin it */
        indexHandle.PFfileHandle_.MarkDirty(HEADER_PAGENUM);
        indexHandle.PFfileHandle_.UnpinPage(HEADER_PAGENUM);
    }

    rc = pfm_->CloseFile(indexHandle.PFfileHandle_);
    if (rc) return rc;
    
    indexHandle.valid_ = 0;

    return 0;
}
