#ifndef QLNODE_H
#define QLNODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "parser.h"
#include "printer.h"
#include "rm.h"
#include "ix.h"
#include "comp.h"

using namespace std;

#define QL_ENDOFRESULT  (START_QL_WARN + 0)

enum OpType {
    UPDATE, DELETE, PROJECT, FILTER, TABLE_SCAN, INDEX_SCAN, NESTED_LOOP_JOIN
};

/* Base qNode struct (for query tree traversal) */
class qNode {
public:
    virtual ~qNode() { };
    virtual RC Begin() { return 0; };
    virtual RC GetNext(RM_Record &rec) = 0;
    virtual void PrintOp(string whitespace) = 0;

    OpType type;                // type of the operator
    qNode *child;
    qNode *rchild;              // For binary operators
    int attrCount;
    DataAttrInfo *attributes;   // Information about attributes of the result of this operator
    int initialized;
};

/* Operand (leaf) of a query tree. 
   Do a table scan and extract tuples that meet specified condition. */
class qTableScan : public qNode {
public:
    /* If relName != NULL, do a full scan. Otherwise, do a condition scan based on condition. */
    qTableScan(int attrCount, DataAttrInfo *attributes, const Condition &condition, const char *relName, RM_Manager *rmm);
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    int fullScan;
    const char *relName;
    Condition condition;
    DataAttrInfo condAttrInfo;
    RM_FileHandle fh; 
    RM_FileScan fs;
    RM_Manager *rmm;
};


/* Operands (leaf) of a query tree. 
   Do an index scan and extract tuples that meet specified condition. */
class qIndexScan : public qNode {
public:
    qIndexScan(int attrCount, DataAttrInfo *attributes, const Condition &condition, RM_Manager *rmm, IX_Manager *ixm);
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    Condition condition;
    DataAttrInfo condAttrInfo;
    RM_FileHandle fh;
    IX_IndexHandle ih;
    IX_IndexScan is;
    RM_Manager *rmm;
    IX_Manager *ixm;
};


/* Joins results in rchild and child2 that meet conditions specified */
class qJoin : public qNode {
public:
    qJoin(qNode *child, qNode *rchild, int nConditions, const Condition conditions[], RM_Manager *rmm);
    ~qJoin();
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    int nConditions;
    Condition *conditions;
    int childTupleSize;
    int rchildTupleSize;
    int joinedTupleSize;
    DataAttrInfo *lhsCondAttr;
    DataAttrInfo *rhsCondAttr;

    RM_Manager *rmm;
    RM_FileHandle childResults;
    RM_FileHandle rchildResults;
    RM_FileScan childFs;
    RM_FileScan rchildFs;
    RM_Record rec1;

    //Printer *p1;
    //Printer *p2;
};

/* Projects attrs of results in child1 */
class qProject : public qNode {
public:
    qProject(qNode *child, int nProjAttrs, const RelAttr projAttrs[]);
    ~qProject();
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    int nProjAttrs;
    const RelAttr *projAttrs;
    int *oldOffsets;
    int tupleLength;
    Printer *p;
};


/* Filters tuples returned by child according to conditions specified */
class qFilter : public qNode {
public:
    qFilter(qNode *child, int nConditions, const Condition conditions[]);
    ~qFilter();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    int nConditions;
    Condition *conditions;
    DataAttrInfo *lhsCondAttr;
    DataAttrInfo *rhsCondAttr;
};

/* Updates tuples returned by child */
class qUpdate : public qNode {
public:
    qUpdate(qNode *child, const RelAttr &updAttr, int bIsValue, const RelAttr &rhsAttr, const Value &rhsValue, RM_Manager *rmm, IX_Manager *ixm);
    ~qUpdate();
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    RelAttr updAttr; 
    int bIsValue;
    RelAttr rhsAttr; 
    Value rhsValue;

    RM_Manager *rmm;
    IX_Manager *ixm;
    IX_IndexHandle ih;
    RM_FileHandle fh;

    DataAttrInfo updAttrInfo;
    DataAttrInfo rhsAttrInfo;
    Printer *p;
};


/* Updates tuples returned by child */
class qDelete : public qNode {
public:
    qDelete(qNode *child, const char *relName, RM_Manager *rmm, IX_Manager *ixm);
    ~qDelete();
    RC Begin();
    RC GetNext(RM_Record &rec);
    void PrintOp(string whitespace);

private:
    const char *relName;
    Printer *p;

    RM_Manager *rmm;
    IX_Manager *ixm;
    IX_IndexHandle *ih;
    RM_FileHandle fh;
};

#endif
