#include "ix_internal.h"

IX_Manager::IX_Manager(PF_Manager &pfm)
{
	pfm_ = pfm;
}

IX_Manager::~IX_Manager() { }


RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength)
{
	RC rc;
	if (attrLength <= 0 || attrLength > MAXSTRINGLEN ||
		attrType < 0 || attrType > STRING || 
		((attrType == FLOAT || attrType == INT) && (attrLength != NUMLEN))) {
		return IX_CREATEPARAMINVALID;
	}

	rc = pfm_.CreateFile(filename + "." + stoi(indexNo));
	if (rc) return rc;

	/* Fill out Index File Header */
	IX_FileHdr fileHdr;
    fileHdr.maxKeysInternal = (PF_PAGE_SIZE - sizeof(PageNum)) / (sizeof(RID) + attrLength + sizeof(PageNum));
    fileHdr.maxKeysLeaf = (PF_PAGE_SIZE - sizeof(PageNum)) / (sizeof(RID) + attrLength);
    fileHdr.attrType = attrType;
    fileHdr.attrLength = attrLength;
    fileHdr.numKeys = 0;

    /* Open the new file and allocate a new page for index file header */
    PF_FileHandle newFileHandle;
    rc = pfmanager_->OpenFile(filename + "." + stoi(indexNo), newFileHandle);
    if (rc) return rc;

    PF_PageHandle headerPageHandle;
    rc = newFileHandle.AllocatePage(headerPageHandle);
    if (rc) return rc;

    /* Allocate a new page for root node */
    PF_PageHandle rootPageHandle;
    rc = newFileHandle.AllocatePage(rootPageHandle);
    if (rc) return rc;
    rootPageHandle.GetPageNum(fileHdr.rootPage);

    /* Initialize root header */
    char *rootPageData;
    rootPageHandle.GetPageData(rootPageData);
    IX_NodeHdr *rootHdr = (IX_NodeHdr *) rootPageData;
    rootHdr->nodeType = root;
    rootHdr->numKeys = 0;
    
    /* Mark root page as dirty and unpin it */
    rc = newFileHandle.MarkDirty(fileHdr.rootPage);
    if (rc) return rc;
    rc = newFileHandle.UnpinPage(fileHdr.rootPage);
    if (rc) return rc;

    /* Copy file header onto header page */
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
	return pfmanager_->DestroyFile(fileName + "." + indexNo);
}

RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle)
{
    RC rc = pfm_->OpenFile(fileName + "." + indexNo, indexHandle.PFindexHandle_);
    if (rc) return rc;

    /* Fetch header page */
    PF_PageHandle headerPageHandle;
    rc = indexHandle.PFindexHandle_.GetFirstPage(headerPageHandle);
    if (rc) return rc;

    /* Copy file header to FileHandle object */
    char *headerPageData;
    headerPageHandle.GetData(headerPageData);
    memcpy(&(indexHandle.hdr_), headerPageData, sizeof(IX_FileHdr));

    indexHandle.PFindexHandle_.UnpinPage(HEADER_PAGENUM);
    indexHandle.valid_ = 1;
    indexHandle.hdrModified_ = 0;
    return 0;
}

RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    RC rc;
    if (!indexHandle.valid_) return IX_FILEINVALID;
    /* Write file header if modified */
    if (indexHandle.hdrModified_) {
        PF_PageHandle headerPageHandle;
        rc = indexHandle.PFindexHandle_.GetFirstPage(headerPageHandle);
        if (rc) return rc;

        /* Copy file header onto header page */
        char *headerPageData;
        headerPageHandle.GetData(headerPageData);
        memcpy(headerPageData, &(indexHandle.hdr_), sizeof(IX_FileHdr));

        /* Mark header page as dirty and unpin it */
        indexHandle.PFindexHandle_.MarkDirty(HEADER_PAGENUM);
        indexHandle.PFindexHandle_.UnpinPage(HEADER_PAGENUM);
    }

    rc = pfm_->CloseFile(indexHandle.PFindexHandle_);
    if (rc) return rc;
    
    indexHandle.valid_ = 0;
    indexHandle.hdrModified_ = 0;

    return 0;
}
