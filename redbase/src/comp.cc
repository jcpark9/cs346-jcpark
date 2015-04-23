#include "comp.h"

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

/* Returns 1 if the unique key defined by (pData1, rid1) is greater than key
 *        -1 if it is lesser than key
 *         0 if two keys are identical (which they never should be)
 */
int CompareKey(char *key1, char *key2, int DataOnly, AttrType attrType, int attrLength) {
  int comp = 0;
  switch(attrType) {
    case INT:
      comp = IntCompare(key1, key2, attrLength);
      break;
    case FLOAT:
      comp = FloatCompare(key1, key2, attrLength);
      break;
    case STRING:
      comp = StringCompare(key1, key2, attrLength);
      break;
  }
  if (DataOnly == 1 || comp != 0) return comp;

  /* Compare PageNum when key is the same */
  PageNum pn1, pn2;
  memcpy(&pn1, key1 + attrLength, sizeof(PageNum));
  memcpy(&pn2, key2 + attrLength, sizeof(PageNum));
  if (pn1 > pn2) return 1;
  if (pn1 < pn2) return -1;
  
  /* Compare SlotNum when PageNum is the same */
  SlotNum sn1, sn2;
  memcpy(&sn1, key1 + attrLength + sizeof(PageNum), sizeof(SlotNum));
  memcpy(&sn2, key2 + attrLength + sizeof(PageNum), sizeof(SlotNum));
  if (sn1 > sn2) return 1;
  if (sn1 < sn2) return -1;
  return 0;
}