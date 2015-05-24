//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <cstdio>
#include <stdio.h>
#include <iostream>
#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"

using namespace std;


void PrintQueryPlanHelper(qNode *curr, string whitespace) {
    // Insert whitespace for nice formatting
    curr->PrintOp(whitespace);
    cout << '\n';

    // Print children recursively through pre-order traversal
    if (curr->child) PrintQueryPlanHelper(curr->child, whitespace + "      ");
    if (curr->rchild) PrintQueryPlanHelper(curr->rchild, whitespace + "     ");
}

void PrintQueryPlan(qNode *root) {
    cout << "\n******************************************** SELECTED QUERY PLAN ********************************************\n" << endl;
    PrintQueryPlanHelper(root, "");
    cout << "\n*************************************************************************************************************\n" << endl;
}

RC ExecuteQueryPlan(qNode *root) {
    if (bQueryPlans) PrintQueryPlan(root);

    while (1) {
        RM_Record rec;
        RC rc = root->GetNext(rec);
        if (rc == QL_ENDOFRESULT) return 0;
        if (rc) return rc;
    }
}

void DeleteQueryPlan(qNode *curr) {
    // Delete children recursively
    if (curr->child) DeleteQueryPlan(curr->child);
    if (curr->rchild) DeleteQueryPlan(curr->rchild);
    delete curr;
}

RC checkAttrExists(const RelAttr &relattr, DataAttrInfo *attributes, int attrCount, DataAttrInfo &info) {
    for (int i = 0; i < attrCount; i++) { 
        if (strncmp(relattr.attrName, attributes[i].attrName, MAXNAME) == 0) {
            if (relattr.relName == NULL || strncmp(relattr.relName, attributes[i].relName, MAXNAME) == 0) {
                info = attributes[i];
                return 0;                
            }
        }
    }
    return QL_ATTRNOTEXIST;
}


int FindIndexedAttrCond(int nConditions, const Condition conditions[], DataAttrInfo *attributes, int attrCount, int updAttrIndexNo) { 
    for (int i=0; i < nConditions; i++) {
        Condition c = conditions[i];
        if (!c.bRhsIsAttr) {
            DataAttrInfo info;
            if (checkAttrExists(c.lhsAttr, attributes, attrCount, info) == 0 && info.indexNo != -1 && info.indexNo != updAttrIndexNo) return i;
        }
    }
    return -1;
}

//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
{
    smm_ = &smm;
    ixm_ = &ixm;
    rmm_ = &rmm;
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager()
{ }



/* Checks if relations in WHERE clause exist and there are no duplicate */
RC ValidateRelForSelect(int nRelations, const char * const relations[])
{
    for (int i = 0; i < nRelations; i++) {
        const char *r1 = relations[i];
        for (int j = i+1; j < nRelations; j++) {
            const char *r2 = relations[j];
            if (strncmp(r1, r2, MAXNAME) == 0) return QL_DUPLICATEREL;
        }
    }
    return 0;
}

/* Validates attr */
RC QL_Manager::ValidateAttrForSelect(const RelAttr &attr, int nRelations, 
    const char * const relations[], AttrType &attrType) 
{
    RC rc; RM_Record rec;
    if (strcmp(attr.attrName, "*") == 0) return 0;

    /* When relName is specified in attr */
    if (attr.relName != NULL) {
        /* Checks if attr.relName is in relations of WHERE clause */
        int relationFound = 0;
        for (int i = 0; i < nRelations; i++) {
            if (strncmp(attr.relName, relations[i], MAXNAME) == 0) {
                relationFound = 1;
                break;
            }
        }
        if (!relationFound) return QL_RELNOTINWHERECLAUSE;

        /* Checks if attr.attrName is an attribute of attr.relName */
        if ((rc = smm_->FindAttrMetadata(attr.relName, attr.attrName, rec))) return rc;
        AttrcatTuple* attrcatTuple = smm_->GetAttrcatTuple(rec);
        attrType = attrcatTuple->attrType;

    /* When relName is ambiguous in attr */
    } else {
        /* Checks if attr.attrName is an attribute of (only) one of the relations in WHERE clause */
        int relationFound = 0;

        for (int i = 0; i < nRelations; i++) {
            rc = smm_->FindAttrMetadata(relations[i], attr.attrName, rec);

            if (rc != 0 && rc != SM_ATTRNOTEXIST) return rc;
            if (rc == 0 && relationFound) return QL_AMBIGUOUSATTR;
            if (rc == 0) {
                AttrcatTuple* attrcatTuple = smm_->GetAttrcatTuple(rec);
                attrType = attrcatTuple->attrType;
                relationFound = 1;
            }
        }
        if (!relationFound) return QL_RELNOTINWHERECLAUSE;
    }
    return 0;
}


//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
    int i; RC rc;

    cout << "Select\n";

    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    for (i = 0; i < nSelAttrs; i++) 
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";

    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++) 
        cout << "   relations[" << i << "] " << relations[i] << "\n";

    cout << "   nConditions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++) 
        cout << "   conditions[" << i << "] " << conditions[i] << "\n";

    /* Validate relations */
    if ((rc = ValidateRelForSelect(nRelations, relations))) return rc;

    /* Validate attributes in selAttrs */
    AttrType attrType;
    for (int i = 0; i < nSelAttrs; i++)
        if ((rc = ValidateAttrForSelect(selAttrs[i], nRelations, relations, attrType))) return rc;

    /* Validate attributes in conditions */
    for (int i = 0; i < nConditions; i++) {
        Condition c = conditions[i];
        if ((rc = ValidateAttrForSelect(c.lhsAttr, nRelations, relations, attrType))) return rc;

        if (c.bRhsIsAttr) {
            AttrType rhsAttrType;
            if ((rc = ValidateAttrForSelect(c.rhsAttr, nRelations, relations, rhsAttrType))) return rc;
            if (rhsAttrType != attrType) return QL_OPERANDSINCOMPATIBLE;
        } else {
            if (c.rhsValue.type != attrType) return QL_OPERANDSINCOMPATIBLE;
        }
    }

    int attrCount[nRelations];
    DataAttrInfo *attributes[nRelations];
    vector<int> remainingCond; // Indices of conditions that have not yet been reflected in query plan
    for (int i = 0; i < nConditions; i++) remainingCond.push_back(i);
    
    vector<qNode *> branches;
    for (int i = 0 ; i < nRelations; i++) {
        const char *relName = relations[i];
        if ((rc = smm_->FillDataAttributes(relName, attributes[i], attrCount[i]))) return rc;        

        qNode *branch;

        int indexedCond = FindIndexedAttrCond(nConditions, conditions, attributes[i], attrCount[i], -1);
        if (indexedCond != -1) {
            branch = static_cast<qNode*> (new qIndexScan(attrCount[i], attributes[i], conditions[indexedCond], rmm_, ixm_));
            remainingCond.erase(std::remove(remainingCond.begin(), remainingCond.end(), indexedCond), remainingCond.end());
        } else {
            int scanConditionFound = 0;
            for (unsigned int j = 0; j < remainingCond.size(); j++) {
                Condition c = conditions[remainingCond[j]];
                DataAttrInfo info;
                if (!c.bRhsIsAttr && checkAttrExists(c.lhsAttr, attributes[i], attrCount[i], info) == 0) {
                    branch = static_cast<qNode*> (new qTableScan(attrCount[i], attributes[i], conditions[remainingCond[j]], NULL, rmm_));
                    remainingCond.erase(remainingCond.begin() + j);
                    scanConditionFound = 1;
                    break;
                }
            }

            if (!scanConditionFound) {
                Condition nullCondition;
                branch = static_cast<qNode*> (new qTableScan(attrCount[i], attributes[i], nullCondition, relName, rmm_));
            }
        }

        vector<int> filterCandidates;
        vector<int>::iterator it = remainingCond.begin();
        while (it != remainingCond.end()) {
            Condition c = conditions[*it];
            DataAttrInfo info;
            if (checkAttrExists(c.lhsAttr, attributes[i], attrCount[i], info) == 0 &&
                (!c.bRhsIsAttr || checkAttrExists(c.rhsAttr, attributes[i], attrCount[i], info) == 0)) {
                filterCandidates.push_back(*it);
                it = remainingCond.erase(it);
            } else {
                ++it;
            }
        }

        if (filterCandidates.size() > 0) {
            Condition filterConditions[filterCandidates.size()];
            for (unsigned int j = 0; j < filterCandidates.size(); j++)
                filterConditions[j] = conditions[filterCandidates[j]];
            branches.push_back(static_cast<qNode*> (new qFilter(branch, filterCandidates.size(), filterConditions)));
        } else {
            branches.push_back(branch);
        }
    }

    while (branches.size() > 1) {
        qNode *joinedBranch;

        qNode *branch1 = branches.front();
        branches.erase(branches.begin());
        qNode *branch2 = branches.front();
        branches.erase(branches.begin());

        vector<int> filterCandidates;
        vector<int>::iterator it = remainingCond.begin();
        while (it != remainingCond.end()) {
            Condition c = conditions[*it];
            DataAttrInfo info;
            if ((checkAttrExists(c.lhsAttr, branch1->attributes, branch1->attrCount, info) == 0 ||
                checkAttrExists(c.lhsAttr, branch2->attributes, branch2->attrCount, info) == 0) &&
                (checkAttrExists(c.rhsAttr, branch1->attributes, branch1->attrCount, info) == 0 || 
                checkAttrExists(c.rhsAttr, branch2->attributes, branch2->attrCount, info) == 0)) {
                filterCandidates.push_back(*it);
                it = remainingCond.erase(it);
            } else {
                ++it;
            }
        }

        if (filterCandidates.size() > 0) {
            Condition filterConditions[filterCandidates.size()];
            for (unsigned int j = 0; j < filterCandidates.size(); j++)
                filterConditions[j] = conditions[filterCandidates[j]];
            joinedBranch = static_cast<qNode*> (new qJoin(branch1, branch2, filterCandidates.size(), filterConditions, rmm_));
        } else {
            joinedBranch = static_cast<qNode*> (new qJoin(branch1, branch2, 0, NULL, rmm_));
        }

        branches.push_back(joinedBranch);
    }

    qNode *root = static_cast<qNode*> (new qProject(branches.front(), nSelAttrs, selAttrs));

    rc = ExecuteQueryPlan(root);
    DeleteQueryPlan(root);
    for (int i = 0; i < nRelations; i++)
        delete[] attributes[i];
    return rc;
}

//
// Insert the values into relName
//
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[])
{
    int i; RC rc;

    cout << "Insert\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";
    for (i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";
    
    RM_Record relRecord;
    if ((rc = smm_->FindRelMetadata(relName, relRecord))) return rc;
    RelcatTuple *relMetadata = smm_->GetRelcatTuple(relRecord);
    if (relMetadata->attrCount != (unsigned int) nValues) return QL_ATTRCOUNTMISMATCH;

    DataAttrInfo *attributes; int attrCount;
    if ((rc = smm_->FillDataAttributes(relName, attributes, attrCount))) return rc;

    /* Pack data of each attribute into newTuple and check if attrType matches */
    char newTuple[relMetadata->tupleLength];
    memset(newTuple, 0, relMetadata->tupleLength);
    for (int i=0; i < nValues; i++) {
        if (values[i].type != attributes[i].attrType) { 
            delete[] attributes; return QL_OPERANDSINCOMPATIBLE;
        }
        memcpy(newTuple + attributes[i].offset, values[i].data, attributes[i].attrLength);
    }

    /* Insert a new tuple into record file */
    RM_FileHandle fh; RID rid;
    if ((rc = rmm_->OpenFile(relName, fh))) { delete[] attributes; return rc; }
    if ((rc = fh.InsertRec(newTuple, rid))) { delete[] attributes; return rc; }
    if ((rc = rmm_->CloseFile(fh))) { delete[] attributes; return rc; }

    /* Insert index entry for an indexed attribute */
    for (int i=0; i < nValues; i++) {
        if (attributes[i].indexNo != -1) {
            IX_IndexHandle ih;
            if ((rc = ixm_->OpenIndex(relName, attributes[i].indexNo, ih))) { delete[] attributes; return rc; }
            if ((rc = ih.InsertEntry(newTuple + attributes[i].offset, rid))) { delete[] attributes; return rc; }
            if ((rc = ixm_->CloseIndex(ih))) { delete[] attributes; return rc; }
        }
    }

    /* Update relation metadata (numTuples) */
    relMetadata->numTuples++;
    if ((rc = smm_->relcatFile_.UpdateRec(relRecord))) { delete[] attributes; return rc; }
    if ((rc = smm_->relcatFile_.ForcePages())) { delete[] attributes; return rc; }

    /* Print the inserted tuple */
    Printer p(attributes, attrCount);
    p.PrintHeader(cout);
    p.Print(cout, newTuple);
    p.PrintFooter(cout);

    delete[] attributes;
    return 0;
}



RC ValidateAttrForDeleteAndUpdate(const char *relName, const RelAttr &lhsAttr, 
    int bIsValue, const RelAttr &rhsAttr, const Value &rhsValue, 
    DataAttrInfo *attributes, int attrCount) 
{
    RC rc;
    DataAttrInfo lhsAttrInfo;
    if ((rc = checkAttrExists(lhsAttr, attributes, attrCount, lhsAttrInfo))) return rc;

    if (bIsValue) {
        if (rhsValue.type != lhsAttrInfo.attrType) return QL_OPERANDSINCOMPATIBLE;
    } else {
        DataAttrInfo rhsAttrInfo;
        if ((rc = checkAttrExists(rhsAttr, attributes, attrCount, rhsAttrInfo))) return rc;
        if (rhsAttrInfo.attrType != lhsAttrInfo.attrType) return QL_OPERANDSINCOMPATIBLE;
    }

    return 0;
}

RC CheckDeleteAndUpdateConditions(const char *relName, 
                                int nConditions, const Condition conditions[],
                                DataAttrInfo *attributes, int attrCount) 
{
    RC rc;
    for (int i = 0; i < nConditions; i++) {
        Condition c = conditions[i];
        if ((rc = ValidateAttrForDeleteAndUpdate(relName, c.lhsAttr, !c.bRhsIsAttr, c.rhsAttr, 
            c.rhsValue, attributes, attrCount))) return rc;
    }
    return 0;
}

//
// Delete from the relName all tuples that satisfy conditions
//
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[])
{
    int i; RC rc;

    cout << "Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nConditions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++) 
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n"; 

    
    RM_Record relRecord;
    if ((rc = smm_->FindRelMetadata(relName, relRecord))) return rc;
    RelcatTuple *relMetadata = smm_->GetRelcatTuple(relRecord);

    DataAttrInfo *attributes; int attrCount;
    if ((rc = smm_->FillDataAttributes(relName, attributes, attrCount))) return rc;

    /* Check if conditions are valid */
    if ((rc = CheckDeleteAndUpdateConditions(relName, nConditions, conditions, attributes, attrCount))) {
        delete[] attributes;
        return rc;
    }

    vector<int> remainingCond; // Indices of conditions that have not yet been reflected in query plan
    for (int i = 0; i < nConditions; i++) remainingCond.push_back(i);

    qNode *root;

    vector<int> indexedAttrCond;
    int indexedCond = FindIndexedAttrCond(nConditions, conditions, attributes, attrCount, -1);
    if (indexedCond != -1) {
        root = static_cast<qNode*> (new qIndexScan(attrCount, attributes, conditions[indexedCond], rmm_, ixm_));
        remainingCond.erase(std::remove(remainingCond.begin(), remainingCond.end(), indexedCond), remainingCond.end());

    } else {
        int scanConditionFound = 0;
        for (int i = 0; i < nConditions; i++) {
            if (!conditions[i].bRhsIsAttr) {
                root = static_cast<qNode*> (new qTableScan(attrCount, attributes, conditions[i], NULL, rmm_));
                remainingCond.erase(remainingCond.begin() + i);
                scanConditionFound = 1;
                break;
            }
        }
        if (!scanConditionFound) {
            Condition nullCondition;
            root = static_cast<qNode*> (new qTableScan(attrCount, attributes, nullCondition, relName, rmm_));
        }
    }

    if (remainingCond.size() > 0) {
        Condition filterConditions[remainingCond.size()];
        for (unsigned int i = 0; i < remainingCond.size(); i++)
            filterConditions[i] = conditions[remainingCond[i]];
        root = static_cast<qNode*> (new qFilter(root, remainingCond.size(), filterConditions));
    }

    root = static_cast<qNode*> (new qDelete(root, relName, rmm_, ixm_));

    if ((rc = ExecuteQueryPlan(root)) == 0) {
        relMetadata->numTuples -= (static_cast<qDelete*> (root))->tuplesDeleted;
        /* Update relation metadata (numTuples) */
        if ((rc = smm_->relcatFile_.UpdateRec(relRecord))) { delete[] attributes; return rc; }
        if ((rc = smm_->relcatFile_.ForcePages())) { delete[] attributes; return rc; }        
    }

    DeleteQueryPlan(root);
    delete[] attributes;
    return 0;
}

//
// Update from the relName all tuples that satisfy conditions
//
RC QL_Manager::Update(const char *relName,
                    const RelAttr &updAttr, 
                    const int bIsValue, 
                    const RelAttr &rhsRelAttr, 
                    const Value &rhsValue, 
                    int nConditions, const Condition conditions[])
{
    int i; RC rc; 

    cout << "Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++) 
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    
    DataAttrInfo *attributes; int attrCount;
    if ((rc = smm_->FillDataAttributes(relName, attributes, attrCount))) return rc;

    if ((rc = CheckDeleteAndUpdateConditions(relName, nConditions, conditions, attributes, attrCount))) {
        delete[] attributes; return rc;
    }

    if ((rc = ValidateAttrForDeleteAndUpdate(relName, updAttr, bIsValue, rhsRelAttr, rhsValue, attributes, attrCount))) {
        delete[] attributes; return rc;
    }


    DataAttrInfo updAttrInfo;
    checkAttrExists(updAttr, attributes, attrCount, updAttrInfo);

    vector<int> remainingCond; // Indices of conditions that have not yet been reflected in query plan
    for (int i = 0; i < nConditions; i++) remainingCond.push_back(i);

    qNode *root;

    int indexedCond = FindIndexedAttrCond(nConditions, conditions, attributes, attrCount, updAttrInfo.indexNo);
    if (indexedCond != -1) {
        root = static_cast<qNode*> (new qIndexScan(attrCount, attributes, conditions[indexedCond], rmm_, ixm_));
        remainingCond.erase(std::remove(remainingCond.begin(), remainingCond.end(), indexedCond), remainingCond.end());
    } else {
        int scanConditionFound = 0;
        for (int i = 0; i < nConditions; i++) {
            if (!conditions[i].bRhsIsAttr) {
                root = static_cast<qNode*> (new qTableScan(attrCount, attributes, conditions[i], NULL, rmm_));
                remainingCond.erase(remainingCond.begin() + i);
                scanConditionFound = 1;
                break;
            }
        }
        if (!scanConditionFound) {
            Condition nullCondition;
            root = static_cast<qNode*> (new qTableScan(attrCount, attributes, nullCondition, relName, rmm_));
        }
    }

    if (remainingCond.size() > 0) {
        Condition filterConditions[remainingCond.size()];
        for (unsigned int i = 0; i < remainingCond.size(); i++)
            filterConditions[i] = conditions[remainingCond[i]];
        root = static_cast<qNode*> (new qFilter(root, remainingCond.size(), filterConditions));
    }

    root = static_cast<qNode*> (new qUpdate(root, updAttr, bIsValue, rhsRelAttr, rhsValue, rmm_, ixm_));

    rc = ExecuteQueryPlan(root);
    DeleteQueryPlan(root);
    delete[] attributes;
    return 0;
}

