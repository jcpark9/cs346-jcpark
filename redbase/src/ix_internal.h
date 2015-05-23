
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

#define HEADER_PAGENUM	0		      // Page number of index file header
#define NUMLEN 4				          // Length of FLOAT and INT
#define IX_INDEX_LIST_BEGIN -1    // Prev pointer of the first leaf
#define IX_INDEX_LIST_END -2      // Next pointer of the last leaf


/* Header for index file */
struct IX_FileHdr {
    PageNum rootPage;       // PageNum of root page
    AttrType attrType;
    int attrLength;

    int maxKeysInternal;    // Maximum # of keys that an internal/root node can contain
    int maxKeysLeaf;        // Maximum # of keys that a leaf node can contain

    int numKeys;            // Total # of leaf keys (indices) in the tree
    int leftmostLeaf;       // Pointer to the first leaf
};

enum NodeType
{
  root,internal,leaf
};


/* Header for root and internal nodes */
struct IX_NodeHdr {
  NodeType nodeType;
  int numKeys;              // # of keys currently in the node

  int numChild;             // # of children (pointers) in the node
  PageNum firstChild;       // Pointer to page with keys < firstKey of this node
};


/* Header for leaf nodes */
struct IX_LeafHdr {
  NodeType nodeType;
  int numKeys;              // # of keys currently in the node

  PageNum next;             // # of keys currently in the node
  PageNum prev;
};

#endif
