#include "ix_internal.h"


IX_IndexHandle::IX_IndexHandle()
{
    valid_ = 0;
    keylen_ = hdr_.attrLength + sizeof(PageNum) + sizeof(slotNum);
}

IX_IndexHandle::~IX_IndexHandle()
{ }


RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    if (!valid_) return IX_FILEINVALID;
    /*
    if (hdr_.numKeys == 0) initializeTree(pData, rid);

    PF_PageHandle bucketFound;
    rc = TreeSearch(pData, rid, hdr_.rootPage, bucketFound);
    if (rc) return rc;

    rc = InsertEntryToBucket(pData, rid, bucketFound);
    if (rc) return rc;
    */

    PageNum newChild = -1;
    InsertEntryToBucket(pData, rid, hdr_.rootPage, newChild)

    hdr_.numKeys++;
    hdrModified_ = 1;

    return 0;
}

RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    if (!valid_) return IX_FILEINVALID;
    
    rid = rid_;
    return 0;
}

RC IX_IndexHandle::ForcePages()
{
    if (!valid_) return IX_FILEINVALID;
    
    rid = rid_;
    return 0;
}

RC IX_IndexHandle::InitializeTree(void *pData, const RID &rid) {
    RC rc;
    PF_PageHandle rootPage;
    rc = PFfileHandle_.GetThisPage(hdr_.rootPage, rootPage);
    if (rc) return rc;

    char *rootPageData;
    rootPage.GetPageData(rootPageData);
    IX_NodeHdr *rootHdr = (IX_NodeHdr *)rootPageData;
    rootHdr->numKeys = 1;

    rootHdr->firstPtr = IX_NO_CHILD;
    char *firstKey = rootPageData + sizeof(rootHdr);
    WriteKey(firstKey, pData, rid);

    for (int i = 0; i < 2; i++) {
        PF_PageHandle newPage;
        rc = PFfileHandle_.AllocatePage(newPage);
        if (rc) return rc;

        char *newPageData;
        newPage.GetPageData(newPageData);
        IX_NodeHdr *nodeHdr = (IX_NodeHdr *) newPageData;
        nodeHdr->numKeys = 0;
        nodeHdr->nodeType = leaf;
        nodeHdr->parent = hdr_.rootPage;

        PageNum pn;
        newPage.GetPageNum(pn);
        if (i == 0) rootHdr->firstPtr = pn;
        if (i == 1) *(firstKey + keylen_) = pn;
    
        PFfileHandle_.MarkDirty(pn);
        PFfileHandle_.UnpinPage(pn);
    }

    rc = PFfileHandle_.MarkDirty(hdr_.rootPage);
    if (rc) return rc;
    rc = PFfileHandle_.UnpinPage(hdr_.rootPage);
    if (rc) return rc;
}


RC IX_IndexHandle::TreeSearch(void *pData, const RID &rid, PageNum current, PageNum &found) {
    RC rc;
    PF_PageHandle page;
    rc = PFfileHandle_.GetThisPage(current, page);
    if (rc) return rc;

    char *pageData;
    page.GetPageData(pageData);
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *) pageData;
    
    if (nodeHdr->nodeType == leaf) {
        rc = PFfileHandle_.UnpinPage(current);
        if (rc) return rc;
        found = current;
        return 0;
    }
    
    PageNum child = nodeHdr->firstPtr;
    char *key = pageData + sizeof(IX_NodeHdr);
    if (CompareKey(pData, rid, key) < 0) {
        rc = PFfileHandle_.UnpinPage(current);
        if (rc) return rc;
        return TreeSearch(pData, rid, child, found);
    }

    child = *((int *)(key + keylen_));
    for (int i = 1; i < nodeHdr->numKeys; i++) {
        key = key + keylen_ + sizeof(PageNum);
        if (CompareKey(pData, rid, key) < 0) {
            rc = PFfileHandle_.UnpinPage(current);
            if (rc) return rc;
            return TreeSearch(pData, rid, child, found);
        }
        child = *((int *)(key + keylen_));
    }

    rc = PFfileHandle_.UnpinPage(current);
    if (rc) return rc;
    return TreeSearch(pData, rid, child, found);
}

/* Returns 1 if the unique key defined by (pData1, rid1) is greater than key
 *        -1 if it is lesser than key
 *         0 if two keys are identical (which they never should be)
 */
int IX_IndexHandle::CompareKey(void *pData1, const RID &rid1, char *key) {
  char *pData2 = key;

  switch(hdr_.attrType) {
    case INT:
      comp = IntCompare(pData1, pData2, hdr_.attrLength);
      break;
    case FLOAT:
      comp = FloatCompare(pData1, pData2, hdr_.attrLength);
      break;
    case STRING:
      comp = StringCompare(pData1, pData2, hdr_.attrLength);
      break;
  }
  if (comp != 0) return comp;

  /* Compare PageNum when key is the same */
  PageNum pn1, pn2;
  rid1.GetPageNum(pn1);
  memcpy(pn2, key + hdr_.attrLength, sizeof(PageNum));
  if (pn1 > pn2) return 1;
  if (pn1 < pn2) return -1;
  
  /* Compare SlotNum when PageNum is the same */
  SlotNum sn1, sn2;
  rid1.GetSlotNum(sn1);
  memcpy(sn2, key + hdr_.attrLength + sizeof(PageNum), sizeof(SlotNum));
  if (sn1 > sn2) return 1;
  if (sn1 < sn2) return -1;
  return 0;
}

/* Writes pData, PageNum and slotNum of record at address indicated by key */
void IX_IndexHandle::WriteKey(char *key, void *pData, const RID &rid) {
    PageNum pn, SlotNum sn;
    rid.GetPageNum(pn);
    rid.GetSlotNum(sn);
    
    memcpy(key, pData, hdr_.attrLength);
    memcpy(key + hdr_.attrLength, &pn, sizeof(PageNum));
    memcpy(key + hdr_.attrLength + sizeof(PageNum), &sn, sizeof(SlotNum));   
}

RC IX_IndexHandle::InsertEntryToNode(
    void *pData, const RID &rid, PageNum currentNode, PageNum &newChild) 
{
    RC rc;
    PF_PageHandle page;
    rc = PFfileHandle_.GetThisPage(currentNode, page);
    if (rc) return rc;

    char *nodeData;
    page.GetPageData(nodeData);
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *) nodeData;
    nodeData += sizeof(IX_NodeHdr);

    if (nodeHdr->nodeType == root || internal) {
        int keyIndex; // subtree to be followed is a pointer that is to the left of key with keyIndex
        PageNum subtree = FindSubtreePtr(pData, rid, nodeData, keyIndex);
        rc = InsertEntryToNode(pData, rid, keyIndex, newChild);
        if (newChild == -1) return;
        rc = InsertEntryToNonLeaf();
    }

    if (nodeHdr->nodeType == leaf) {
        rc = InsertEntryToLeaf(pData, rid, nodeData, nodeHdr, newChild);
        if (rc) return rc;
    }

    rc = PFfileHandle_.UnpinPage(currentNode);k
    if (rc) return rc;
}


RC IX_IndexHandle::InsertEntryToNonLeaf(
    void *pData, const RID &rid, IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex, PageNum &newChild) 
{
    /* If there is room in the non-leaf node */
    if (nodeHdr->numKeys < hdr_.maxKeysInternal) {
        char *key = nodeData + keyIndex * (keylen_ + sizeof(PageNum)); // position to be inserted
        if (keyIndex != nodeHdr->numKeys) {
            memcpy(key + keylen_ + sizeof(PageNum), key, (keylen_ + sizeof(PageNum))*(nodeHdr->numKeys - keyIndex)); // Shift keys to the right by one
        }
        WriteKey(key, pData, rid);
        *((int *)(key + keylen_)) = newChild;
        nodeHdr->numKeys++;


        char *existingKey; int i;
        /* Iterate through existing keys to find appropriate position */
        for (i = 0; i < nodeHdr->numKeys; i++) {
            existingKey = nodeData + i*keylen_;
            if (CompareKey(pData, rid, existingKey) < 0) break;
        }
        /* If key being inserted is greater than all keys in this node, write to the next key slot */
        if (i == nodeHdr->numKeys) {
            existingKey += keylen_; 
        /* If key is being inserted in between existing keys, shift keys to the right */
        } else {
            memcpy(existingKey + keylen_, existingKey, (nodeHdr->numKeys-i)*keylen_);
        }
        WriteKey(existingKey, pData, rid);
        nodeHdr->numKeys++;
        return 0;
    }

    /* If there is not enough room, split */
    PF_PageHandle newNode;
    rc = PFfileHandle_.AllocatePage(newNode);
    if (rc) return rc;

    PageNum newNodePageNum;
    char *newNodeData;
    newLeaf.GetPageNum(newChild);
    newLeaf.GetPageData(newNode);

    /* Initialize header for the new leaf */
    IX_NodeHdr *newNodeHdr = (IX_NodeHdr *) newNodeData;
    newLeafData += sizeof(IX_NodeHdr);
    newLeafHdr->nodeType = internal;
    int firstKeyMoved = hdr_.maxKeysInternal/2;
    int numKeysMoved = hdr_.maxKeysInternal - firstKeyMoved;
    newLeafHdr->numKeys = numKeysMoved;
    newLeafHdr->firstPtr = nodeHdr->firstPtr; // Copy sibling pointer

    /* Update header for the original leaf */
    nodeHdr->numKeys -= numKeysMoved;
    nodeHdr->firstPtr = newChild;

    /* Copy keys to the new leaf */
    memcpy(newLeafData, nodeData + firstKeyMoved * keylen_, numKeysMoved * keylen_);
    WriteKey(newLeafData + numKeysMoved * keylen_, pData, newChild);
    newLeafHdr->numKeys++;
    return 0;
}


RC IX_IndexHandle::InsertEntryToLeaf(
    void *pData, const RID &rid, IX_NodeHdr *nodeHdr, char *nodeData, PageNum &newChild) 
{
    /* If there is room in the leaf node */
    if (nodeHdr->numKeys < hdr_.maxKeysLeaf) {
        char *existingKey; int i;
        /* Iterate through existing keys to find appropriate position */
        for (i = 0; i < nodeHdr->numKeys; i++) {
            existingKey = nodeData + i*keylen_;
            if (CompareKey(pData, rid, existingKey) < 0) break;
        }
        /* If key being inserted is greater than all keys in this node, write to the next key slot */
        if (i == nodeHdr->numKeys) {
            existingKey += keylen_; 
        /* If key is being inserted in between existing keys, shift keys to the right */
        } else {
            memcpy(existingKey + keylen_, existingKey, (nodeHdr->numKeys-i)*keylen_);
        }
        WriteKey(existingKey, pData, rid);
        nodeHdr->numKeys++;
        return 0;
    }

    /* If there is not enough room, split */
    PF_PageHandle newLeaf;
    rc = PFfileHandle_.AllocatePage(newLeaf);
    if (rc) return rc;

    PageNum newLeafPageNum;
    char *newLeafData;
    newLeaf.GetPageNum(newChild);
    newLeaf.GetPageData(newLeafData);

    /* Initialize header for the new leaf */
    IX_NodeHdr *newLeafHdr = (IX_NodeHdr *) newLeafData;
    newLeafData += sizeof(IX_NodeHdr);
    newLeafHdr->nodeType = leaf;
    int firstKeyMoved = hdr_.maxKeysLeaf/2;
    int numKeysMoved = hdr_.maxKeysLeaf - firstKeyMoved;
    newLeafHdr->numKeys = numKeysMoved;
    newLeafHdr->firstPtr = nodeHdr->firstPtr; // Copy sibling pointer

    /* Update header for the original leaf */
    nodeHdr->numKeys -= numKeysMoved;
    nodeHdr->firstPtr = newChild;

    /* Copy keys to the new leaf */
    memcpy(newLeafData, nodeData + firstKeyMoved * keylen_, numKeysMoved * keylen_);
    WriteKey(newLeafData + numKeysMoved * keylen_, pData, newChild);
    newLeafHdr->numKeys++;
    return 0;
}

PageNum findSubtreePtr(char *nodeData, void *pData, const RID &rid, int &keyIndex) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *) nodeData;
    PageNum child = nodeHdr->firstPtr;

    keyIndex = 0;
    char *key = nodeData + sizeof(IX_NodeHdr);
    if (CompareKey(pData, rid, key) < 0) return child;

    child = *((int *)(key + keylen_));
    for (keyIndex = 1; keyIndex < nodeHdr->numKeys; keyIndex++) {
        key = key + keylen_ + sizeof(PageNum);
        if (CompareKey(pData, rid, key) < 0) return child;
        child = *((int *)(key + keylen_));
    }

    return child;
}