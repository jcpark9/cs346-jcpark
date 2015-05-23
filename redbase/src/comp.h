
#ifndef COMP_H
#define COMP_H

#include <cstring>
#include <cstdlib>
#include <assert.h>
#include <stdint.h>
#include "redbase.h"
#include "rm_rid.h"

/* Global functions used for various comparisons */
int StringCompare(char *key, void *val, int n);
int IntCompare(char *key, void *val, int n);
int FloatCompare(char *key, void *val, int n);
int CompareKey(char *key1, char *key2, int DataOnly, AttrType attrType, int attrLength);

#endif
