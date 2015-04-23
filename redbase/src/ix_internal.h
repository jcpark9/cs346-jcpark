
#ifndef IX_INTERNAL_H
#define IX_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <stdint.h>
#include <iostream>
#include "ix.h"

//
// Constants and defines
//

#define HEADER_PAGENUM	0		// Page number of file header

#define NUMLEN 4				// length of FLOAT and INT
#define IX_INDEX_LIST_BEGIN -1
#define IX_INDEX_LIST_END -2
#define IX_NO_CHILD -1

struct IX_FileHdr {
    PageNum rootPage; // PageNum of root page
    AttrType attrType;
    int attrLength;

    int maxKeysInternal; // Maximum # of keys that an internal/root node can contain
    int maxKeysLeaf; // Maximum # of keys that a leaf node can contain

    int numKeys; // Total # of leaf keys (indices) in the tree
    int leftmostLeaf;
};
typedef struct IX_FileHdr IX_FileHdr;

enum NodeType
{
  root,internal,leaf
};

struct IX_NodeHdr {
    NodeType nodeType;
    int numKeys; // # of keys currently in the node

  int numChild; // # of children (pointers) in the node
  /* When node is root/internal, points to page with keys < first key */
  PageNum firstChild;
};
typedef struct IX_NodeHdr IX_NodeHdr;

struct IX_LeafHdr {
  NodeType nodeType;
  int numKeys; // # of keys currently in the node

  /* When node is leaf, points to next and previous leaf pages */
  PageNum next;
  PageNum prev;
};
typedef struct IX_LeafHdr IX_LeafHdr;

#endif
