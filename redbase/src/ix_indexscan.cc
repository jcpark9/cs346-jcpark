#include "ix_internal.h"
#include "comp.h"

IX_IndexScan::IX_IndexScan()
{
    valid_ = 0;
    scanComplete_ = 1;
}

IX_IndexScan::~IX_IndexScan()
{
  indexHandle_.hdr_ = NULL;
  if (valid_) free(lastKeySeen_);
  if (valid_ && !scanComplete_) indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
}


RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint)
{
	if (valid_) return IX_SCANOPEN;
	if (!indexHandle.valid_) return IX_FILEINVALID;

	/* Check if scan parameters are valid */
	if (compOp < 0 || compOp > GE_OP || compOp == NE_OP ||
    (value == NULL && compOp != NO_OP)) return IX_SCANPARAMINVALID;

  /* Initialize scan parameters */
  indexHandle_ = indexHandle;

  compOp_ = compOp;
  value_ = value;
  pinHint_ = pinHint;

  valid_ = 1;
  scanComplete_ = 0;
  currentKeyIndex_ = 0;

  keylen_ = indexHandle_.hdr_->attrLength + sizeof(RID);
  lastKeySeen_ = (char *)malloc(keylen_);
  firstEntryScanned_ = 0;
  return 0;
}



RC IX_IndexScan::GetNextEntry(RID &rid)
{
  if (!valid_) return IX_SCANINVALID;
  if (scanComplete_) return IX_EOF;

  // When first entry is being scanned, set scan pointer to the first entry that meets scan condition
  if (!firstEntryScanned_) {
    RC rc = InitializeScanPtr();
    firstEntryScanned_ = 1;
    if (rc == IX_EOF) scanComplete_ = 1;
    if (rc) return rc;
  
  // Check if previous key is same as the last key we've seen to check if the last key was deleted
  } else if (currentKeyIndex_ != 0) {
    IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;
    char *previousKey = nodeData_ + sizeof(IX_LeafHdr) + keylen_ * (currentKeyIndex_ - 1);

    // Move scan pointer back if the last key was deleted (not to miss the entry that was shifted left)
    if (memcmp(previousKey, lastKeySeen_, keylen_) != 0) currentKeyIndex_--;

    // Fetch next page if we reached end of leaf
    if (currentKeyIndex_ >= leafHdr->numKeys) {
      RC rc = FetchNextPage(nextLeaf_);
      if (rc) return rc;
      currentKeyIndex_ = 0;
    }

  }

  IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;

  char *existingKey = nodeData_ + sizeof(IX_LeafHdr) + keylen_ * currentKeyIndex_;
  // Pass RID of the entry found when scan condition is met
  if (ConditionMet(existingKey, (char *)value_)) {
    memcpy(lastKeySeen_, existingKey, keylen_);
    memcpy(&rid, existingKey + indexHandle_.hdr_->attrLength, sizeof(RID));

    currentKeyIndex_++;

    /* When there is only one key in the leaf (which just has been scanned), 
       Move on to next page since the current leaf may get deleted
     */
    if (leafHdr->numKeys == 1) {
      RC rc = FetchNextPage(nextLeaf_);
      if (rc == IX_EOF) scanComplete_ = 1;
      else if (rc) return rc;
      currentKeyIndex_ = 0;
    }
    return 0;
  
  // Terminate scan when scan condition is no longer met
  } else {
    scanComplete_ = 1;
    RC rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    if (rc) return rc;
    return IX_EOF;
  }
}

RC IX_IndexScan::FetchNextPage(PageNum next) {
  /* Unpin the previous page */  
  RC rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
  if (rc) return rc;

  /* Set currentPage_ as the next leaf */
  currentPage_ = next;

  /* Return IX_EOF when we reached the end of leaf list */
  if (currentPage_ == IX_INDEX_LIST_END) {
    scanComplete_ = 1;
    return IX_EOF;
  }

  /* Update nodeData_ and nextLeaf_ */
  PF_PageHandle pageHandle;
  rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
  if (rc) return rc;
  rc = pageHandle.GetData(nodeData_);
  if (rc) return rc;
  nextLeaf_ = ((IX_LeafHdr *) nodeData_)->next;
  return 0;
}


RC IX_IndexScan::CloseScan()
{
    if (!valid_) return IX_SCANINVALID;
    valid_ = 0;
    free(lastKeySeen_);
    if (!scanComplete_) indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    return 0;
}


/* Returns 1 if scan condition is met, and 0 otherwise. 
 */
int IX_IndexScan::ConditionMet(char *key1, char *key2) {
  int attrLength = indexHandle_.hdr_->attrLength;
  AttrType attrType = indexHandle_.hdr_->attrType;

  switch(compOp_) {
    case EQ_OP:
      return (CompareKey(key1, key2, 1, attrType, attrLength) == 0);
    
    case LT_OP:
      return (CompareKey(key1, key2, 1, attrType, attrLength) < 0);

    case LE_OP:
      return (CompareKey(key1, key2, 1, attrType, attrLength) <= 0);

    default:
      return 1;
  }
}

/* Initialize scan pointer (CurrentPage_ and CurrentKeyIndex_) to point 
 * at the first entry that meets scan condition
 */
RC IX_IndexScan::InitializeScanPtr() {

  // For NO_OP, LT_OP and LE_OP, scan from the very beginning of leaf list
  if (compOp_ == NO_OP || compOp_ == LT_OP || compOp_ == LE_OP) {
    currentPage_ = indexHandle_.hdr_->leftmostLeaf;
    currentKeyIndex_ = 0;

    PF_PageHandle pageHandle;
    RC rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
    if (rc) return rc;
    pageHandle.GetData(nodeData_);
    nextLeaf_ = ((IX_LeafHdr *) nodeData_)->next;

    return 0;
  }

  // For EQ_OP, GT_OP and GE_OP, find the first entry that meets scan condition
  AttrType attrType = indexHandle_.hdr_->attrType;
  int attrLength = indexHandle_.hdr_->attrLength;
  char *keyData = (char *)value_;

  // By setting rid to (1,0), attempt to find the leftmost (smallest) key with data == value_
  RID rid = RID(1,0); 
  char key[keylen_];

  memcpy(key, keyData, attrLength);
  memcpy(key + attrLength, &rid, sizeof(RID));

  // Walk down the tree to find the leaf that possibly contains key
  int found;
  RC rc = TreeSearch(key, indexHandle_.hdr_->rootPage, found);
  if (rc) return rc;

  currentPage_ = found;

  // Traverse through keys in a leaf node until we find the entry with exact scan condition
  // For GE_OP and GT_OP, we might need to traverse multiple leaf nodes if duplicates span over multiple pages
  while (1) {

    // Return EOF when we reach the end of leaf list
    if (currentPage_ == IX_INDEX_LIST_END) return IX_EOF;

    PF_PageHandle pageHandle;
    rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
    if (rc) return rc;

    pageHandle.GetData(nodeData_);
    IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;

    for (int i=0; i < leafHdr->numKeys; i++) {
      char *existingKey = nodeData_ + sizeof(IX_LeafHdr) + i * keylen_;

      // Return when we found the first key that meets scan condition
      if ((compOp_ == EQ_OP && CompareKey(existingKey, keyData, 1, attrType, attrLength) == 0) ||
          (compOp_ == GE_OP && CompareKey(existingKey, keyData, 1, attrType, attrLength) >= 0) ||
          (compOp_ == GT_OP && CompareKey(existingKey, keyData, 1, attrType, attrLength) > 0)) {
        nextLeaf_ = ((IX_LeafHdr *) nodeData_)->next;
        currentKeyIndex_ = i;
        return 0;

      // For EQ_OP, we can terminate prematurely if existingKey is larger (since keys in the leaf are in sorted order)
      } else if (compOp_ == EQ_OP && CompareKey(keyData, existingKey, 1, attrType, attrLength) < 0) {
        rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
        if (rc) return rc;
        return IX_EOF;
      }
    }

    rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    if (rc) return rc;
    currentPage_ = leafHdr->next;
  }
}

/* Iterates through keys in a non-leaf node and returns the subtree pointer to be followed for newKey
 */
PageNum IX_IndexScan::FindSubtreePtr(char *newKey, IX_NodeHdr *nodeHdr, char *nodeData) {
  AttrType attrType = indexHandle_.hdr_->attrType;
  int attrLength = indexHandle_.hdr_->attrLength;

  char *key = nodeData;
  PageNum subtree = nodeHdr->firstChild;
  if (nodeHdr->numChild == 1 || CompareKey(newKey, key, 0, attrType, attrLength) < 0) return subtree;

  memcpy(&subtree, key+keylen_, sizeof(PageNum));
  for (int keyIndex = 1; keyIndex < nodeHdr->numKeys; keyIndex++) {
    key += (keylen_ + sizeof(PageNum));  
    if (CompareKey(newKey, key, 0, attrType, attrLength) < 0) return subtree;
    memcpy(&subtree, key+keylen_, sizeof(PageNum));
  }

  return subtree;
}

/* Walk down the tree to find leaf page that possibly contains key */    
RC IX_IndexScan::TreeSearch(char *key, PageNum current, PageNum &found) {
  RC rc;
  PF_PageHandle page;
  rc = indexHandle_.PFfileHandle_.GetThisPage(current, page);
  if (rc) return rc;

  char *pageData;
  page.GetData(pageData);
  IX_NodeHdr *nodeHdr = (IX_NodeHdr *) pageData;
  pageData += sizeof(IX_NodeHdr);

  /* If we reached leaf, return with found set to this current node*/
  if (nodeHdr->nodeType == leaf) {
      rc = indexHandle_.PFfileHandle_.UnpinPage(current);
      if (rc) return rc;
      found = current;
      return 0;
  }
  
  /* Find the subtree to be followed */
  PageNum subtree = FindSubtreePtr(key, nodeHdr, pageData);
  rc = indexHandle_.PFfileHandle_.UnpinPage(current);
  if (rc) return rc;

  /* Recursively search down the tree */
  return TreeSearch(key, subtree, found);
}