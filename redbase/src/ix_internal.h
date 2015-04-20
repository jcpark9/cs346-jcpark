
#ifndef IX_INTERNAL_H
#define IX_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include <assert>
#include "ix.h"

//
// Constants and defines
//

#define HEADER_PAGENUM	0		// Page number of file header

#define NUMLEN 4				// length of FLOAT and INT
#define IX_INDEX_LIST_END -1
#define IX_NO_CHILD -1

enum NodeType {root,internal,leaf};

struct IX_FileHdr {
	PageNum rootPage; // PageNum of root page
	AttrType attrType;
	int attrLength;

	int maxKeysInternal; // Maximum # of keys that an internal/root node can contain
	int maxKeysLeaf; // Maximum # of keys that a leaf node can contain

	int numKeys; // Total # of leaf keys (indices) in the tree
};

struct IX_NodeHdr {
	NodeType nodeType;
	int numKeys; // # of keys currently in the node

	/* When node is root/internal, points to page with keys < first key
	   When node is leaf, points to next leaf page */
	PageNum firstPtr;
};



int StringCompare(char *key, void *val, int n) {
  return strncmp(key, (char *)val, n);
}

int IntCompare(char *key, void *val, int n) {
  assert(sizeof(int32_t) == n);
  int32_t key_v, val_v;
  memcpy(&val_v, val, n);
  memcpy(&key_v, key, n);

  if (key_v == val_v) return 0;
  if (key_v < val_v) return -1;
  return 1;
}

int FloatCompare(char *key, void *val, int n) {
  assert(sizeof(float) == n);
  float key_v, val_v;
  memcpy(&val_v, val, n);
  memcpy(&key_v, key, n);

  if (key_v == val_v) return 0;
  if (key_v < val_v) return -1;
  return 1;
}



#endif
