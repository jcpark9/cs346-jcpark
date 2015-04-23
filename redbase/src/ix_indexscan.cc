#include "ix_internal.h"
#include "comp.h"

IX_IndexScan::IX_IndexScan()
{
    valid_ = 0;
}

IX_IndexScan::~IX_IndexScan()
{
  if (valid_ && !scanComplete_) {
    indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    free(lastKeySeen_);
  }
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

  /* Initialize Scan Parameters */
  indexHandle_ = indexHandle;
  compOp_ = compOp;
  value_ = value;
  pinHint_ = pinHint;

  valid_ = 1;
  scanComplete_ = 0;
  currentKeyIndex_ = 0;
  indexHandle_.scans_++;

  keylen_ = indexHandle_.hdr_.attrLength + sizeof(RID);
  lastKeySeen_ = (char *)malloc(keylen_);
  firstEntryScanned_ = 0;

  //std::cout << "SCANNING FOR VALUE: " << *((int *) value_ ) << std::endl;
  return 0;
}



RC IX_IndexScan::GetNextEntry(RID &rid)
{
  if (!valid_) return IX_SCANINVALID;
  if (scanComplete_) return IX_EOF;

  /* When first entry is being scanned, set scan pointer to the first entry that meets condition */
  if (!firstEntryScanned_) {
    RC rc = InitializeScanPtr();
    firstEntryScanned_ = 1;
    if (rc == IX_EOF) {
      scanComplete_ = 1;
      return IX_EOF;
    } else if (rc) return rc;
  /* Check if previous key is same as the last key we seen to check if a key was deleted */
  } else if (currentKeyIndex_ != 0) {
    IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;
    char *previousKey = nodeData_ + sizeof(IX_LeafHdr) + keylen_ * (currentKeyIndex_ - 1);
/*
    std::cout << "CURR " << currentKeyIndex_ << std::endl;
    std::cout << "NUM K " << leafHdr->numKeys << std::endl;
*/  
    if (memcmp(previousKey, lastKeySeen_, keylen_) != 0) currentKeyIndex_--;
    if (currentKeyIndex_ >= leafHdr->numKeys) {
      RC rc = FetchNextPage(nextLeaf_);
      if (rc) return rc;
      currentKeyIndex_ = 0;
    }

  }


  IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;

  char *existingKey = nodeData_ + sizeof(IX_LeafHdr) + keylen_ * currentKeyIndex_;
/*  if (value_ != NULL) {
     std::cout << "existingKey_ == " << *((int *)existingKey) << std::endl;
     std::cout << "value_ == " << *((int *)value_) << std::endl;
     std::cout << ConditionMet(existingKey, (char *)value_) << std::endl;
  }*/
  /* Pass the rid found when scan condition is met */
  if (ConditionMet(existingKey, (char *)value_)) {
    memcpy(lastKeySeen_, existingKey, keylen_);
    memcpy(&rid, existingKey + indexHandle_.hdr_.attrLength, sizeof(RID));

    PageNum pn;
    SlotNum sn;
    rid.GetPageNum(pn);
    rid.GetSlotNum(sn);
    //std::cout << *((int *)existingKey) << "  RID " << pn << ":" << sn << std::endl;

    currentKeyIndex_++;

    /* When only there is only one key in the leaf that has been scanned, 
       Move on to next page sincen the current leaf may get deleted. 
     */
    if (leafHdr->numKeys == 1) {
      //std::cout << "NUMKEY == 1  nextLeaf_ == " << nextLeaf_ << std::endl;
      RC rc = FetchNextPage(nextLeaf_);
      //std::cout << "RC == " << rc << std::endl;
      if (rc == IX_EOF) scanComplete_ = 1;
      else if (rc) return rc;
      currentKeyIndex_ = 0;
    }
    return 0;
  /* Terminate scan when scan condition is no longer met */
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

  /* Set next leaf page to currentPage_*/
  currentPage_ = next;

  /* Return IX_EOF when we reached the end of leaf list */
  if (currentPage_ == IX_INDEX_LIST_END) {
    scanComplete_ = 1;
    return IX_EOF;
  }

  /* Get leaf page and update scan pointers */
  PF_PageHandle pageHandle;
  rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
  //std::cout << "PINNING PAGE " << currentPage_ << std::endl;
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
    indexHandle_.scans_--;
    free(lastKeySeen_);
    lastKeySeen_ = NULL;
    if (!scanComplete_) indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    return 0;
}

/* Returns 1 if scan condition is met, and 0 otherwise. 
 */
int IX_IndexScan::ConditionMet(char *key1, char *key2) {
  int attrLength = indexHandle_.hdr_.attrLength;
  AttrType attrType = indexHandle_.hdr_.attrType;

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

/* Set CurrentPage_ and CurrentKeyIndex_ to point at entry that
 * first meets scan condition
 */
RC IX_IndexScan::InitializeScanPtr() {
  if (compOp_ == NO_OP || compOp_ == LT_OP || compOp_ == LE_OP) {
    currentPage_ = indexHandle_.hdr_.leftmostLeaf;
    //std::cout << currentPage_ << std::endl;
    currentKeyIndex_ = 0;

    PF_PageHandle pageHandle;
    RC rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
    if (rc) return rc;
    pageHandle.GetData(nodeData_);
    nextLeaf_ = ((IX_LeafHdr *) nodeData_)->next;

    return 0;
  }

  AttrType attrType = indexHandle_.hdr_.attrType;
  int attrLength = indexHandle_.hdr_.attrLength;
  char *keyData = (char *)value_;

  /* By setting rid to (1,0), we attempt to find the leftmost (smallest) key with data == value_ */
  RID rid = RID(1,0); 
  char key[keylen_];

  memcpy(key, keyData, attrLength);
  memcpy(key + attrLength, &rid, sizeof(RID));

  int found; // PageNum of leaf node that would contain key
  RC rc = TreeSearch(key, indexHandle_.hdr_.rootPage, found);
  if (rc) return rc;

  currentPage_ = found;
  //std::cout << "SUBTREE " << currentPage_ << std::endl;  
  /* Traverse through leaf nodes until we find entry with exact scan condition */
  while (1) {
    if (currentPage_ == IX_INDEX_LIST_END) return IX_EOF;

    PF_PageHandle pageHandle;
    rc = indexHandle_.PFfileHandle_.GetThisPage(currentPage_, pageHandle);
    if (rc) return rc;

    pageHandle.GetData(nodeData_);
    IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData_;

/*  std::cout << "RRRRRRRRRRRR" << std::endl;
    for (int i = 0; i < leafHdr->numKeys; i++) {
        char *k = nodeData_ + sizeof(IX_LeafHdr) + i*(keylen_);
        memcpy(&rid, k + indexHandle_.hdr_.attrLength, sizeof(RID));
        PageNum pn;
        SlotNum sn;
        rid.GetPageNum(pn);
        rid.GetSlotNum(sn);
        std::cout << *((int *)k) << "  RID " << pn << ":" << sn << std::endl;
    }
  std::cout << "RRRRRRRRRRRR" << std::endl;
*/
    for (int i=0; i < leafHdr->numKeys; i++) {
      char *existingKey = nodeData_ + sizeof(IX_LeafHdr) + i * keylen_;
      //std::cout << "KEYDATA " << *((int *)keyData) << std::endl;  
      //std::cout << "EXISTINGKEY " << *((int *)existingKey) << std::endl;  
      if ((compOp_ == EQ_OP && CompareKey(keyData, existingKey, 1, attrType, attrLength) == 0) ||
          (compOp_ == GE_OP && CompareKey(keyData, existingKey, 1, attrType, attrLength) >= 0) ||
          (compOp_ == GT_OP && CompareKey(keyData, existingKey, 1, attrType, attrLength) > 0)) {
        nextLeaf_ = ((IX_LeafHdr *) nodeData_)->next;
        currentKeyIndex_ = i;
        //std::cout << "INITIAL SCANINDEX FOUND: " << currentKeyIndex_ << std::endl;
        return 0;
      }
      /*} else if (compOp_ == EQ_OP && CompareKey(keyData, existingKey, 1, attrType, attrLength) > 0) {
        rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
        if (rc) return rc;
        return IX_EOF;
      }*/
    }
    //std::cout << "NOT FOUND IN THIS PAGE" << std::endl;
    rc = indexHandle_.PFfileHandle_.UnpinPage(currentPage_);
    if (rc) return rc;
    currentPage_ = leafHdr->next;
    //std::cout << "NEXT PAGE" << currentPage_ << std::endl;
  }
}

PageNum IX_IndexScan::FindSubtreePtr(char *newKey, IX_NodeHdr *nodeHdr, char *nodeData) {
  AttrType attrType = indexHandle_.hdr_.attrType;
  int attrLength = indexHandle_.hdr_.attrLength;
/*
  std::cout << "SPSPSPSPSP" << std::endl;
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *k = nodeData + i*(keylen_ + sizeof(PageNum));
        int subtree;
        memcpy(&subtree, k-sizeof(PageNum), sizeof(PageNum));
        //int subtree;
        //memcpy(&subtree, k -sizeof(PageNum), sizeof(PageNum));
        std::cout << "subtree" << i << ":" << subtree << std::endl;  
        std::cout << "key" << i << ":" << *((int *)k) << std::endl;  
    }
  std::cout << "SPSPSPSPSP" << std::endl;
  */
    char *key = nodeData;
    PageNum subtree = nodeHdr->firstChild;
    if (nodeHdr->numChild == 1 || CompareKey(newKey, key, 0, attrType, attrLength) < 0) return subtree;

    memcpy(&subtree, key+keylen_, sizeof(PageNum));
    for (int keyIndex = 1; keyIndex < nodeHdr->numKeys; keyIndex++) {
      key += (keylen_ + sizeof(PageNum));
      //std::cout << "subtree" << keyIndex << ":" << subtree << std::endl;  
      //std::cout << "key" << keyIndex << ":" << *((int *)key) << std::endl;  
      if (CompareKey(newKey, key, 0, attrType, attrLength) < 0) return subtree;
      memcpy(&subtree, key+keylen_, sizeof(PageNum));
    }

    return subtree;
}
/*
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *k = nodeData + i*(keylen_ + sizeof(PageNum));
        int subtree;
        GetPtr(k-sizeof(PageNum), subtree);
        //int subtree;
        //memcpy(&subtree, k -sizeof(PageNum), sizeof(PageNum));
        std::cout << "subtree" << i << ":" << subtree << std::endl;  
        std::cout << "key" << i << ":" << *((int *)k) << std::endl;  
    }*/
    
RC IX_IndexScan::TreeSearch(char *key, PageNum current, PageNum &found) {
  RC rc;
  PF_PageHandle page;
  rc = indexHandle_.PFfileHandle_.GetThisPage(current, page);
  if (rc) return rc;

  char *pageData;
  page.GetData(pageData);
  IX_NodeHdr *nodeHdr = (IX_NodeHdr *) pageData;
  pageData += sizeof(IX_NodeHdr);

  /* If we reached leaf, return with found set to current node*/
  if (nodeHdr->nodeType == leaf) {
      rc = indexHandle_.PFfileHandle_.UnpinPage(current);
      if (rc) return rc;
      found = current;
      return 0;
  }
  
  /* Find the subtree to be followed */
  PageNum subtree = FindSubtreePtr(key, nodeHdr, pageData);
  //std::cout << "SUBTREE" << subtree << std::endl;  

  rc = indexHandle_.PFfileHandle_.UnpinPage(current);
  if (rc) return rc;
  return TreeSearch(key, subtree, found);
}