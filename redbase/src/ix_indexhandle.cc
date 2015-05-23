#include "ix_internal.h"
#include "comp.h"


/*** HELPER FUNCTIONS ****/

/* Writes key at address indicated by pos */
void IX_IndexHandle::WriteKey(char *pos, char *key) { 
    memcpy(pos, key, keylen_);
}

/* Writes child ptr at address indicated by pos */
void WritePtr(char *pos, PageNum ptr) { 
    memcpy(pos, &ptr, sizeof(PageNum));
}

/* Read child pointer into ptr at address indicated by pos */
void GetPtr(char *pos, PageNum &ptr) {
    memcpy(&ptr, pos, sizeof(PageNum));
}

/* Get key at keyIndex in an internal node */
char* IX_IndexHandle::GetNthKeyInternal(char *nodeData, int keyIndex) {
    return nodeData + keyIndex * (keylen_ + sizeof(PageNum));
}

/* Get key at keyIndex in a leaf node */
char* IX_IndexHandle::GetNthKeyLeaf(char *nodeData, int keyIndex) {
    return nodeData + keyIndex * (keylen_);
}

/* Iterates through keys in a non-leaf node and returns the subtree pointer to be followed for newKey
 * Upon return, keyIndex points to the key to the right of the subtree pointer
 */
PageNum IX_IndexHandle::FindSubtreePtr(char *newKey, IX_NodeHdr *nodeHdr, char *nodeData, int &keyIndex) {
    PageNum subtree = nodeHdr->firstChild;
    keyIndex = 0;
    char *key = nodeData;

    // If there is only one child to be followed or if key is smaller than the first key, return the first child
    if (nodeHdr->numChild == 1 || CompareKey(newKey, key, 0, hdr_->attrType, hdr_->attrLength) < 0) return subtree;

    // Iterate through each key; stop if a key greater than newKey is found
    GetPtr(key+keylen_, subtree);
    for (keyIndex = 1; keyIndex < nodeHdr->numKeys; keyIndex++) {
        key = key + keylen_ + sizeof(PageNum);
        if (CompareKey(newKey, key, 0, hdr_->attrType, hdr_->attrLength) < 0) return subtree;
        GetPtr(key+keylen_, subtree);
    }

    return subtree;
}


/*** PUBLIC FUNCTIONS ***/

IX_IndexHandle::IX_IndexHandle()
{
    valid_ = 0;
    hdr_ = NULL;
}

IX_IndexHandle::~IX_IndexHandle()
{
    if(hdr_) free(hdr_);
}


RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    if (!valid_) return IX_FILEINVALID;
    if (pData == NULL) return IX_NULLDATA;

    // Create a B+ tree key by concatenating pData and rid 
    char key[keylen_];
    memset(key, 0, keylen_);
    memcpy(key, pData, hdr_->attrLength);
    memcpy(key + hdr_->attrLength, &rid, sizeof(RID));

    // Variables passed across recursive calls
    char newChildData[keylen_];
    PageNum newChild = -1;

    // Look for a place to insert beginning at the root
    RC rc = InsertEntryToNode(key, hdr_->rootPage, newChild, newChildData);
    if (rc) return rc;
    
    hdr_->numKeys++;
    hdrModified_ = 1;

    return 0;
}

RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    if (!valid_) return IX_FILEINVALID;
    if (pData == NULL) return IX_NULLDATA;

    // Create a B+ key by concatenating pData and rid 
    char key[keylen_];
    memcpy(key, pData, hdr_->attrLength);
    memcpy(key + hdr_->attrLength, &rid, sizeof(RID));

    // Variables passed across recursive calls
    int childDeleted = -1;

    // Look for entry to delete beginning at the root
    RC rc = DeleteEntryFromNode(key, hdr_->rootPage, childDeleted);
    if (rc) return rc;

    hdr_->numKeys--;
    hdrModified_ = 1;
    
    return 0;
}

RC IX_IndexHandle::ForcePages()
{
    if (!valid_) return IX_FILEINVALID;
    return PFfileHandle_.ForcePages();
}


/*** INSERTION HELPER FUNCTIONS ***/

RC IX_IndexHandle::InsertEntryToNode(
    char *key, PageNum currentNode, PageNum &newChild, char *newChildData) 
{
    RC rc;
    PF_PageHandle page;
    rc = PFfileHandle_.GetThisPage(currentNode, page);
    if (rc) return rc;

    char *nodeData;
    page.GetData(nodeData);
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *) nodeData;

    // Initialize the first leaf if there is zero leaf page
    if (nodeHdr->nodeType == root && nodeHdr->numChild == 0) {
        rc = InitializeFirstLeaf(nodeHdr->firstChild);
        if (rc) return rc;
        nodeHdr->numChild++;
    }

    // For root & internal nodes
    if (nodeHdr->nodeType == root || nodeHdr->nodeType == internal) {
        nodeData += sizeof(IX_NodeHdr);

        /*  keyIndex = key to the right of the subtree pointer that is to be followed
            It is the position where a key will be inserted if subtree is split */
        int keyIndex; 
        PageNum subtree = FindSubtreePtr(key, nodeHdr, nodeData, keyIndex);

        // Recursively attempt to insert to child node
        rc = InsertEntryToNode(key, subtree, newChild, newChildData);
        if (rc) return rc;

        /* If the subtree was split */
        if (newChild != -1) {
            /* Insert the first key of the new child to the current non-leaf node */
            rc = InsertEntryToNonLeaf(newChild, newChildData, nodeHdr, nodeData, keyIndex);
            if (rc) return rc;
        }
    
    // For leaf nodes
    } else if (nodeHdr->nodeType == leaf) {
        IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData;
        nodeData += sizeof(IX_LeafHdr);

        rc = InsertEntryToLeaf(key, leafHdr, nodeData, newChild, newChildData, currentNode);
        if (rc) return rc;
    }

    rc = PFfileHandle_.MarkDirty(currentNode);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(currentNode);
}

/* Allocates the very first leaf and initializes a linked list of leaf nodes */
RC IX_IndexHandle::InitializeFirstLeaf(PageNum &newLeafPageNum)
{
    /* Allocate a new page for first leaf node */
    PF_PageHandle leafPageHandle;
    RC rc = PFfileHandle_.AllocatePage(leafPageHandle);
    if (rc) return rc;
    leafPageHandle.GetPageNum(newLeafPageNum);
    hdr_->leftmostLeaf = newLeafPageNum;
    hdrModified_ = 1;

    /* Initialize first leaf node */
    char *leafPageData;
    leafPageHandle.GetData(leafPageData);

    IX_LeafHdr *leafHdr = (IX_LeafHdr *) leafPageData;
    leafHdr->nodeType = leaf;
    leafHdr->numKeys = 0;
    leafHdr->prev = IX_INDEX_LIST_BEGIN;
    leafHdr->next = IX_INDEX_LIST_END;

    rc = PFfileHandle_.MarkDirty(newLeafPageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(newLeafPageNum);
}


/* Insert newChildData (first key of newChild) and newChild pointer at keyIndex of current node 
 * Upon return, 
    if the node was split, newChild is PageNum of the new node and newChildData is the first key of new node
    if the node wasn't split, newChild = -1
 */
RC IX_IndexHandle::InsertEntryToNonLeaf(
    PageNum &newChild, char *newChildData, IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex) 
{
    RC rc;
    // If there is room in the non-leaf node, just insert
    if (nodeHdr->numKeys < hdr_->maxKeysInternal) {
        InsertEntryToNonLeafHelper(newChild, newChildData, nodeHdr, nodeData, keyIndex);
        newChild = -1;
        return 0;
    }

    // If there is not enough room, split
    PF_PageHandle newNode;
    rc = PFfileHandle_.AllocatePage(newNode);
    if (rc) return rc;

    PageNum newNodePageNum;
    char *newNodeData;
    newNode.GetPageNum(newNodePageNum);
    newNode.GetData(newNodeData);

    int middleKey = hdr_->maxKeysInternal/2;
    int firstKeyMoved = middleKey + 1;
    int numKeysMoved = hdr_->maxKeysInternal - firstKeyMoved;

    // Initialize header for the new node
    IX_NodeHdr *newNodeHdr = (IX_NodeHdr *) newNodeData;
    newNodeData += sizeof(IX_NodeHdr);
    newNodeHdr->nodeType = internal;
    newNodeHdr->numKeys = numKeysMoved;
    newNodeHdr->numChild = numKeysMoved + 1;
    GetPtr(GetNthKeyInternal(nodeData, firstKeyMoved) - sizeof(PageNum), newNodeHdr->firstChild);

    // Update header for the original node
    nodeHdr->numKeys -= (numKeysMoved + 1);
    nodeHdr->numChild = nodeHdr->numKeys + 1;

    // Copy half of the keys to the new node
    memcpy(newNodeData, GetNthKeyInternal(nodeData, firstKeyMoved), numKeysMoved * (keylen_ + sizeof(PageNum)));
    
    // Insert the new key to either of two split nodes
    if (keyIndex <= middleKey) {
        InsertEntryToNonLeafHelper(newChild, newChildData, nodeHdr, nodeData, keyIndex);
    } else {
        keyIndex = keyIndex - firstKeyMoved;
        InsertEntryToNonLeafHelper(newChild, newChildData, newNodeHdr, newNodeData, keyIndex);
    }

    rc = PFfileHandle_.MarkDirty(newNodePageNum);
    if (rc) return rc;
    rc = PFfileHandle_.UnpinPage(newNodePageNum);
    if (rc) return rc;
    
    // If the root was split, allocate a new root
    if (nodeHdr->nodeType == root) {
        nodeHdr->nodeType = internal;
        rc = AllocateNewRoot(newNodePageNum, newNodeData);
        if (rc) return rc;
    }

    // Copy up the middle key between two split nodes to be inserted to the parent node
    newNode.GetPageNum(newChild);
    memcpy(newNodeData, GetNthKeyInternal(nodeData, middleKey), keylen_);

    return 0;
}

/* When the root node becomes split, allocate a new root with two split nodes as its child */
RC IX_IndexHandle::AllocateNewRoot(PageNum secondChild, char *secondChildKey)
{
    // Allocate a new root
    PF_PageHandle newRoot;
    RC rc = PFfileHandle_.AllocatePage(newRoot);
    if (rc) return rc;
    PageNum newRootPageNum;
    char *newRootData;
    newRoot.GetPageNum(newRootPageNum);
    newRoot.GetData(newRootData);

    // Initialize new root
    IX_NodeHdr *newRootHdr = (IX_NodeHdr *) newRootData;
    newRootHdr->numKeys = 1;
    newRootHdr->numChild = 2;
    newRootHdr->nodeType = root;

    // Write the first key and two children pointers
    newRootHdr->firstChild = hdr_->rootPage;
    char *firstKey = newRootData + sizeof(IX_NodeHdr);
    WriteKey(firstKey, secondChildKey);
    WritePtr(firstKey + keylen_, secondChild);

    // Write the new root to index file header
    hdr_->rootPage = newRootPageNum;

    rc = PFfileHandle_.MarkDirty(newRootPageNum);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(newRootPageNum);
}

/* Write key (and child pointer to its right) onto a non-leaf node at keyIndex*/
void IX_IndexHandle::InsertEntryToNonLeafHelper(PageNum ptr, char *key, IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex) {
    char *pos = GetNthKeyInternal(nodeData, + keyIndex); // position to be inserted
    if (keyIndex != nodeHdr->numKeys) {
        // Shift keys to the right by one
        memmove(pos + keylen_ + sizeof(PageNum), pos, (keylen_ + sizeof(PageNum))*(nodeHdr->numKeys - keyIndex));
    }
    WriteKey(pos, key);
    WritePtr(pos+keylen_, ptr);

    nodeHdr->numKeys++;
    nodeHdr->numChild++;
}

RC IX_IndexHandle::InsertEntryToLeaf(
    char *newKey, IX_LeafHdr *leafHdr, char *leafData, PageNum &newChild, char *newChildData, PageNum currentNode)
{
    RC rc;
    /* If there is room in the leaf node, just insert */
    if (leafHdr->numKeys < hdr_->maxKeysLeaf) {
        rc = InsertEntryToLeafHelper(newKey, leafHdr, leafData);
        if (rc) return rc;
        newChild = -1;
        return 0;
    }

    /* If there is not enough room, split */
    PF_PageHandle newLeaf;
    rc = PFfileHandle_.AllocatePage(newLeaf);
    if (rc) return rc;
    char *newLeafData;
    newLeaf.GetData(newLeafData);
    newLeaf.GetPageNum(newChild);

    int firstKeyMoved = hdr_->maxKeysLeaf/2;
    int numKeysMoved = hdr_->maxKeysLeaf - firstKeyMoved;

    /* Initialize header for the new leaf */
    IX_LeafHdr *newLeafHdr = (IX_LeafHdr *) newLeafData;
    newLeafData += sizeof(IX_NodeHdr);
    newLeafHdr->nodeType = leaf;
    newLeafHdr->numKeys = numKeysMoved;

    /* Adjust sibling pointers */
    newLeafHdr->next = leafHdr->next; // Copy sibling pointer
    newLeafHdr->prev = currentNode;
    leafHdr->next = newChild;

    PF_PageHandle nextPage; char *nextLeafData; PageNum nextLeafPageNum; IX_LeafHdr *nextLeafHdr;
    if (newLeafHdr->next != IX_INDEX_LIST_END) {
        rc = PFfileHandle_.GetThisPage(newLeafHdr->next, nextPage);
        if (rc) return rc;
        nextPage.GetData(nextLeafData);
        nextPage.GetPageNum(nextLeafPageNum);
        
        nextLeafHdr = (IX_LeafHdr *) nextLeafData;
        nextLeafHdr->prev = newChild;

        rc = PFfileHandle_.MarkDirty(nextLeafPageNum);
        if (rc) return rc;
        rc = PFfileHandle_.UnpinPage(nextLeafPageNum);
        if (rc) return rc;
    }

    leafHdr->numKeys -= numKeysMoved;

    /* Copy half of the keys to the new leaf */
    memcpy(newLeafData, GetNthKeyLeaf(leafData, firstKeyMoved), numKeysMoved * keylen_);
    
    /* Copy up the first key of the new leaf to be inserted to the parent node */
    newLeaf.GetPageNum(newChild);
    memcpy(newChildData, newLeafData, keylen_);

    /* Insert the new key into either of two split nodes */
    if (CompareKey(newKey, newChildData, 0, hdr_->attrType, hdr_->attrLength) < 0) {
        rc =InsertEntryToLeafHelper(newKey, leafHdr, leafData);
        if (rc) return rc;
    } else {
        rc = InsertEntryToLeafHelper(newKey, newLeafHdr, newLeafData);
        if (rc) return rc;
    }

    rc = PFfileHandle_.MarkDirty(newChild);
    if (rc) return rc;
    return PFfileHandle_.UnpinPage(newChild);
}

RC IX_IndexHandle::InsertEntryToLeafHelper(char *newKey, IX_LeafHdr *leafHdr, char *leafData) {
    int i;
    char *insertPos = leafData;
    
    if (leafHdr->numKeys > 0) {
        char * existingKey;
        // Iterate through existing keys to find appropriate position
        for (i = 0; i < leafHdr->numKeys; i++) {
            existingKey = GetNthKeyLeaf(leafData, i);
            // When there is an identical entry, return error
            if (CompareKey(newKey, existingKey, 0, hdr_->attrType, hdr_->attrLength) == 0) return IX_DUPLICATEENTRY;
            if (CompareKey(newKey, existingKey, 0, hdr_->attrType, hdr_->attrLength) < 0) {
                insertPos = existingKey;
                // If key is being inserted in between existing keys, shift keys to the right
                memmove(insertPos + keylen_, insertPos, (leafHdr->numKeys-i)*keylen_);
                break;
            }
        }
        // If key being inserted at the leaf's end
        if (i == leafHdr->numKeys) insertPos = GetNthKeyLeaf(leafData, leafHdr->numKeys);
    }

    WriteKey(insertPos, newKey);
    leafHdr->numKeys++;
    return 0;
}



/*** DELETION HELPER FUNCTIONS ***/

RC IX_IndexHandle::DeleteEntryFromNode(
    char *key, PageNum currentNode, int &childDeleted)
{
    RC rc;
    PF_PageHandle page;
    rc = PFfileHandle_.GetThisPage(currentNode, page);
    if (rc) return rc;
    
    char *nodeData;
    page.GetData(nodeData);
    IX_NodeHdr * nodeHdr = (IX_NodeHdr *)nodeData;
    
    // For root and internal nodes
    if (nodeHdr->nodeType == root || nodeHdr->nodeType == internal) {
        nodeData += sizeof(IX_NodeHdr);

        /*  keyIndex == key to the right of the subtree pointer that is to be followed
            Position where a key will be deleted if all children are deleted
         */
        int keyIndex;
        PageNum subtree = FindSubtreePtr(key, nodeHdr, nodeData, keyIndex);
        rc = DeleteEntryFromNode(key, subtree, childDeleted);
        if (rc) return rc;

        // If the child was deleted, delete its associated key from current node
        if (childDeleted == 1) {
            DeleteEntryFromNonLeaf(nodeHdr, nodeData, keyIndex);
            rc = PFfileHandle_.MarkDirty(currentNode);
            if (rc) return rc;

            // When there are no more children in this node, delete it
            if (nodeHdr->numChild == 0 && nodeHdr->nodeType == internal) {
                childDeleted = 1; // Indicate to parent that the current node was deleted
                rc = PFfileHandle_.UnpinPage(currentNode);
                if (rc) return rc;
                return PFfileHandle_.DisposePage(currentNode);
            } else {
                childDeleted = 0;
            }
        }

    // If we reached leaf, delete the entry from this leaf
    } else if (nodeHdr->nodeType == leaf) {
        IX_LeafHdr *leafHdr = (IX_LeafHdr *) nodeData;
        nodeData += sizeof(IX_LeafHdr);
        
        rc = DeleteEntryFromLeaf(key, leafHdr, nodeData);
        if (rc) return rc;
        rc = PFfileHandle_.MarkDirty(currentNode);
        if (rc) return rc;

        // When there are no more keys in this node, delete it
        if (leafHdr->numKeys == 0) {
            childDeleted = 1;
            rc = AdjustSiblingPointers(leafHdr); // Indicate to parent that the current node was deleted
            if (rc) return rc;
            rc = PFfileHandle_.UnpinPage(currentNode);
            if (rc) return rc;
            return PFfileHandle_.DisposePage(currentNode);
        } else {
            childDeleted = 0;
        }
    }

    return PFfileHandle_.UnpinPage(currentNode);
}

/* Delete key at KeyIndex and child pointer to its left */
void IX_IndexHandle::DeleteEntryFromNonLeaf(
    IX_NodeHdr *nodeHdr, char *nodeData, int keyIndex)
{
    /* When deleting the first key; if there is only 1 child, skip since there is no key to delete */
    if (keyIndex == 0 && nodeHdr->numChild > 1) {
        GetPtr(nodeData + keylen_, nodeHdr->firstChild);
        if (nodeHdr->numKeys > 1) memmove(nodeData, nodeData + keylen_ + sizeof(PageNum), (nodeHdr->numKeys - 1) * (keylen_ + sizeof(PageNum)));
    /* When deleting the last key */
    } else if (keyIndex == nodeHdr->numKeys - 1) {
        char *deletedKey = GetNthKeyInternal(nodeData, keyIndex);
        memmove(deletedKey - sizeof(PageNum), deletedKey + keylen_, sizeof(PageNum)); // only move the rightmost child
    /* When deleting the intermediate keys */
    } else if (keyIndex < nodeHdr->numKeys - 1) {
        char *deletedKey = GetNthKeyInternal(nodeData, keyIndex);
        memmove(deletedKey - sizeof(PageNum), deletedKey + keylen_, (nodeHdr->numKeys - keyIndex - 1) * (keylen_ + sizeof(PageNum)) + sizeof(PageNum));
    }

    if(nodeHdr->numKeys >= 1) nodeHdr->numKeys--;
    nodeHdr->numChild--;
}

/* Delete index entry from a leaf node */
RC IX_IndexHandle::DeleteEntryFromLeaf(
    char *deletedKey, IX_LeafHdr *leafHdr, char *leafData)
{
    int found = 0;
    // Iterate through each key to find the identical one to deletedKey
    for (int i = 0; i < leafHdr->numKeys; i++) {
        char *existingKey = GetNthKeyLeaf(leafData, i);
        if (CompareKey(deletedKey, existingKey, 0, hdr_->attrType, hdr_->attrLength) == 0) {
            memmove(existingKey, existingKey + keylen_, (leafHdr->numKeys - i - 1) * keylen_); // Left-shift keys to the right to delete entry
            found = 1;
            break;
        }
    }
    if (!found) return IX_ENTRYNOTFOUND; // When no identical index entry was found
    leafHdr->numKeys--;
    return 0;
}

/* Adjust prev and next pointers of the neighbors of a deleted leaf */
RC IX_IndexHandle::AdjustSiblingPointers(IX_LeafHdr *leafHdr)
{
    RC rc;

    // Adjust prev pointer of the next leaf
    if (leafHdr->next != IX_INDEX_LIST_END) {
        PF_PageHandle nextPage; char *nextNodeData; PageNum nextNodePageNum;
        rc = PFfileHandle_.GetThisPage(leafHdr->next, nextPage);
        if (rc) return rc;
        nextPage.GetData(nextNodeData);
        nextPage.GetPageNum(nextNodePageNum);
        
        IX_LeafHdr *nextNodeHdr = (IX_LeafHdr *) nextNodeData;
        nextNodeHdr->prev = leafHdr->prev;

        rc = PFfileHandle_.MarkDirty(nextNodePageNum);
        if (rc) return rc;
        rc = PFfileHandle_.UnpinPage(nextNodePageNum);
        if (rc) return rc;
    }

    // Adjust next pointer of the previous leaf
    if (leafHdr->prev != IX_INDEX_LIST_BEGIN) {
        PF_PageHandle prevPage; char *prevNodeData; PageNum prevNodePageNum;
        rc = PFfileHandle_.GetThisPage(leafHdr->prev, prevPage);
        if (rc) return rc;
        prevPage.GetData(prevNodeData);
        prevPage.GetPageNum(prevNodePageNum);

        IX_LeafHdr *prevNodeHdr = (IX_LeafHdr *) prevNodeData;
        prevNodeHdr->next = leafHdr->next;

        rc = PFfileHandle_.MarkDirty(prevNodePageNum);
        if (rc) return rc;
        rc = PFfileHandle_.UnpinPage(prevNodePageNum);
        if (rc) return rc;
    }
    return 0;
}
