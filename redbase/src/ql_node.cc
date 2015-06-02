#include "ql_node.h"
#include "lg.h"

void PrintCondition(string whitespace, const Condition &c)
{
    cout << whitespace << "|  lhsAttr: " << c.lhsAttr << endl;
    cout << whitespace << "|  op: " << c.op << endl;
    if (c.bRhsIsAttr) {
       cout << whitespace << "|  bRhsIsAttr = TRUE     rhsAttr: " << c.rhsAttr << endl;
    } else {
        cout << whitespace << "|  bRhsIsAttr = FALSE     rhsValue: " << c.rhsValue << endl;
    }
}

void FindAttributeInfo(const RelAttr &relattr, DataAttrInfo *attributes, int attrCount, DataAttrInfo &result) {
    for (int i = 0; i < attrCount; i++) {
        if (strncmp(relattr.attrName, attributes[i].attrName, MAXNAME) == 0) {
            if (relattr.relName == NULL || strncmp(relattr.relName, attributes[i].relName, MAXNAME) == 0) {
                result = attributes[i];
                break;
            }
        }
    }
}

/* Returns 1 if condition is met, 0 otherwise */
int CheckConditionMet(int comp, const CompOp op) {
    switch (op) {
        case NO_OP:
            return 1;
        case EQ_OP:
            return (comp == 0);
        case NE_OP:
            return (comp != 0);
        case LT_OP:
            return (comp < 0);
        case GT_OP:
            return (comp > 0);
        case LE_OP:
            return (comp <= 0);
        case GE_OP:
            return (comp >= 0);
    }
    return 1;
}

/* If relName != NULL, do a full scan. Otherwise, do a condition scan based on condition. */
qTableScan::qTableScan(int attrCount, DataAttrInfo *attributes, const Condition &condition, const char *relName, RM_Manager *rmm) {
    type = TABLE_SCAN;
    this->child = NULL; this->rchild = NULL;
    this->attrCount = attrCount;
    this->attributes = attributes;
    this->condition = condition;
    if (relName) {
        this->relName = relName;
        fullScan = 1;
    } else {
        FindAttributeInfo(condition.lhsAttr, attributes, attrCount, condAttrInfo);     
        this->relName = condAttrInfo.relName;
        fullScan = 0;
    }
    this->rmm = rmm;
    initialized = 0;
}

RC qTableScan::Begin() {
    RC rc;
    if ((rc = rmm->OpenFile(relName, fh))) return rc;
    if (fullScan) {
        if ((rc = fs.OpenScan(fh, INT, sizeof(int), 0, NO_OP, NULL))) return rc;    
    } else {
        if ((rc = fs.OpenScan(fh, condAttrInfo.attrType, condAttrInfo.attrLength, condAttrInfo.offset, condition.op, condition.rhsValue.data))) return rc;
    }
    initialized = 1;
    return 0;
}

RC qTableScan::GetNext(RM_Record &rec) {
    RC rc;
    /* Initialize on first call */
    if (!initialized) {
        if ((rc = Begin())) return rc;
    }

    rc = fs.GetNextRec(rec);
    /* Clean up if no more tuples */
    if (rc == RM_EOF) {
        if ((rc = fs.CloseScan())) return rc;
        if ((rc = rmm->CloseFile(fh))) return rc;
        return QL_ENDOFRESULT;
    }
    if (rc) return rc;
    return 0;
}

void qTableScan::PrintOp(string whitespace) {
    cout << whitespace << "<<TABLE SCAN>> on " << relName << endl;
    if (fullScan) {
        cout << whitespace << "(Full scan without any condition)" << endl;
    } else {
        cout << whitespace << "condition:" << endl;
        PrintCondition(whitespace, condition);
    }
}



qIndexScan::qIndexScan(int attrCount, DataAttrInfo *attributes, const Condition &condition, RM_Manager *rmm, IX_Manager *ixm) {
    type = INDEX_SCAN;
    this->child = NULL; this->rchild = NULL;
    this->attrCount = attrCount;
    this->attributes = attributes;
    this->condition = condition;
    FindAttributeInfo(condition.lhsAttr, attributes, attrCount, condAttrInfo);     
    this->rmm = rmm;
    this->ixm = ixm;
    initialized = 0;
}

RC qIndexScan::Begin() {
    RC rc;
    if ((rc = rmm->OpenFile(condAttrInfo.relName, fh))) return rc;
    if ((rc = ixm->OpenIndex(condAttrInfo.relName, condAttrInfo.indexNo, ih))) return rc;
    if ((rc = is.OpenScan(ih, condition.op, condition.rhsValue.data))) return rc;
    initialized = 1;
    return 0;
}

RC qIndexScan::GetNext(RM_Record &rec) {
    RC rc;
    /* Initialize on first call */
    if (!initialized) {
        if ((rc = Begin())) return rc;
    }

    RID rid;
    rc = is.GetNextEntry(rid);
    /* Clean up if no more tuples */
    if (rc == IX_EOF) {
        if ((rc = is.CloseScan())) return rc;
        if ((rc = ixm->CloseIndex(ih))) return rc;
        if ((rc = rmm->CloseFile(fh))) return rc;
        return QL_ENDOFRESULT;
    }
    if (rc) return rc;

    /* Fetch record from record file */
    if ((rc = fh.GetRec(rid, rec))) return rc;
    return 0;
}

void qIndexScan::PrintOp(string whitespace) {
    cout << whitespace << "<<INDEX SCAN>> on " << condAttrInfo.relName << endl;
    cout << whitespace << "condition:" << endl;
    PrintCondition(whitespace, condition);
}

int qJoin::nextJoinID = 0;

qJoin::qJoin(qNode *child, qNode *rchild, int nConditions, const Condition conditions[], RM_Manager *rmm) {
    type = NESTED_LOOP_JOIN;
    this->child = child; this->rchild = rchild;
    sprintf(rightFilename, "rchild_temp%d", nextJoinID++);
    cout << rightFilename << endl;
    this->nConditions = nConditions;
    this->conditions = new Condition[nConditions];
    memcpy(this->conditions, conditions, sizeof(Condition) * nConditions);
    this->rmm = rmm;

    /* Combine DataAttrInfo arrays from two children */
    attrCount = child->attrCount + rchild->attrCount;
    attributes = new DataAttrInfo[attrCount];
    int currentOffset = 0;
    for (int i=0; i < child->attrCount; i++) {
        attributes[i] = child->attributes[i];
        currentOffset += child->attributes[i].attrLength;
    }
    childTupleSize = currentOffset;
    for (int i=0; i < rchild->attrCount; i++) {
        attributes[i + child->attrCount] = rchild->attributes[i];
        attributes[i + child->attrCount].offset = currentOffset;
        currentOffset += rchild->attributes[i].attrLength;
    }
    joinedTupleSize = currentOffset;
    rchildTupleSize = joinedTupleSize - childTupleSize;

    /* Find DataAttrInfo for lhsRelAttr and rhsRelAttr for each condition */
    if (nConditions > 0) {
        lhsCondAttr = new DataAttrInfo[nConditions];
        rhsCondAttr = new DataAttrInfo[nConditions];
        
        for (int i=0; i < nConditions; i++) {
            Condition c = conditions[i];
            FindAttributeInfo(c.lhsAttr, attributes, attrCount, lhsCondAttr[i]);
            if (c.bRhsIsAttr) {
                FindAttributeInfo(c.rhsAttr, attributes, attrCount, rhsCondAttr[i]);
            }
        }
    }

    initialized = 0;
}

qJoin::~qJoin() {
    delete[] conditions;
    delete[] attributes;
    if (nConditions > 0) {
        delete[] lhsCondAttr;
        delete[] rhsCondAttr;
    }
}

RC qJoin::Begin() {
    RC rc;
    if ((rc = rmm->CreateFile(rightFilename, rchildTupleSize))) return rc;
    if ((rc = rmm->OpenFile(rightFilename, rchildResults))) return rc;

    RM_Record rec; RID rid; char *pData;
    while (1) {
        rc = rchild->GetNext(rec);
        if (rc == QL_ENDOFRESULT) break;
        if (rc) return rc;

        if ((rc = rec.GetData(pData))) return rc;
        if ((rc = rchildResults.InsertRec(pData, rid))) return rc;
    }

    if ((rc = rchildFs.OpenScan(rchildResults, INT, sizeof(int), 0, NO_OP, NULL))) return rc;
    if ((rc = child->GetNext(rec1))) return rc;

    initialized = 1;
    return 0;
}

RC qJoin::GetNext(RM_Record &rec) {
    RC rc;
    if (!initialized) {
        if ((rc = Begin())) return rc;
    }

    RID rid; char *pData1; char *pData2;
    while (1) {
        /* Get next tuple in rchild */
        RM_Record rec2;
        rc = rchildFs.GetNextRec(rec2);

        if (rc != 0 && rc != RM_EOF) return rc;

        /* Combine a tuple from child and rchild and check if it meets condition */
        if (rc == 0) {
            /* Combine data from two tuples */
            if (rec.valid_) delete[] rec.contents_;
            rec.contents_ = new char[joinedTupleSize];
            rec1.GetData(pData1); rec2.GetData(pData2);
            memcpy(rec.contents_, pData1, childTupleSize);
            memcpy(rec.contents_ + childTupleSize, pData2, rchildTupleSize);
            rec.valid_ = 1;

            /* Check if each condition is met */
            int conditionMet = 1;
            for (int i=0; i < nConditions; i++) {
                Condition c = conditions[i];
                int comp = CompareKey(rec.contents_ + lhsCondAttr[i].offset, rec.contents_ + rhsCondAttr[i].offset, 1, 
                        lhsCondAttr[i].attrType, lhsCondAttr[i].attrLength);
                conditionMet = CheckConditionMet(comp, c.op);
                if (!conditionMet) break;
            }

            if (conditionMet) return 0;

        /* After completing one iteration of scanning rchild */
        } else if (rc == RM_EOF) {
            if ((rc = rchildFs.CloseScan())) return rc;
            /* Get next tuple in child */
            rc = child->GetNext(rec1);

            /* Clean up when done scanning child */
            if (rc == QL_ENDOFRESULT) {
                if ((rc = rmm->CloseFile(rchildResults))) return rc;
                if ((rc = rmm->DestroyFile(rightFilename))) return rc;
                return QL_ENDOFRESULT;
            } if (rc) return rc;

            /* Start another iteration of scaning rchild */
            if ((rc = rchildFs.OpenScan(rchildResults, INT, sizeof(int), 0, NO_OP, NULL))) return rc;
        }
    }
}

void qJoin::PrintOp(string whitespace) {
    cout << whitespace << "<<JOIN>> on" << endl; 
    for (int i = 0; i < nConditions; i++) {
        cout << whitespace << "conditions[" << i << "]:" << endl;
        PrintCondition(whitespace, conditions[i]);
    }
}

qProject::qProject(qNode *child, int nProjAttrs, const RelAttr projAttrs[]) {
    type = PROJECT;
    this->child = child; this->rchild = NULL;
    this->nProjAttrs = nProjAttrs;
    this->projAttrs = projAttrs;


    /* When attrName == "*", project all attributes from child */
    if (nProjAttrs == 1 && strcmp(projAttrs[0].attrName, "*") == 0) {
        attrCount = child->attrCount;
        attributes = child->attributes;

    /* Else, make new list of attributes for printing */
    } else {
        attrCount = nProjAttrs;
        attributes = new DataAttrInfo[attrCount];
        oldOffsets = new int[attrCount];
        int currentOffset = 0;
        for (int i = 0; i < nProjAttrs; i++) {
            FindAttributeInfo(projAttrs[i], child->attributes, child->attrCount, attributes[i]);

            oldOffsets[i] = attributes[i].offset;
            attributes[i].offset = currentOffset;
            currentOffset += attributes[i].attrLength;
        }
        tupleLength = currentOffset;
    }

    p = new Printer(attributes, attrCount);    
    initialized = 0;
}

qProject::~qProject() {
    if (!(nProjAttrs == 1 && strcmp(projAttrs[0].attrName, "*") == 0)) {
        delete[] attributes;
        delete[] oldOffsets;
    }
    delete p;
}

RC qProject::Begin() {
    p->PrintHeader(cout);
    initialized = 1;
    return 0;
}

RC qProject::GetNext(RM_Record &rec) {
    RC rc; char *pData;

    if (!initialized) Begin();

    /* Get next tuple from child */
    rc = child->GetNext(rec);
    
    /* Print footer when all tuples have been printed */
    if (rc == QL_ENDOFRESULT) p->PrintFooter(cout);
    if (rc) return rc;

    if ((rc = rec.GetData(pData))) return rc;

    /* Print all data returned by child when SELECT **/
    if (nProjAttrs == 1 && strcmp(projAttrs[0].attrName, "*") == 0) {
        p->Print(cout, pData);
    /* Pack data for projected attributes into result */
    } else {
        char result[tupleLength];
        for (int i = 0; i < nProjAttrs; i++) {
            memcpy(result + attributes[i].offset, pData + oldOffsets[i], attributes[i].attrLength);
        }
        p->Print(cout, result);

    }
    return 0;
}

void qProject::PrintOp(string whitespace) {
    cout << whitespace << "<<PROJECT>> "; 
    for (int i = 0; i < nProjAttrs; i++) {
        cout << projAttrs[i];
        if (i != nProjAttrs - 1) cout << ", ";
        else cout << "\n";
    }
}


qFilter::qFilter(qNode *child, int nConditions, const Condition conditions[]) {
    type = FILTER;
    this->child = child; this->rchild = NULL;
    this->nConditions = nConditions;
    this->conditions = new Condition[nConditions];
    memcpy(this->conditions, conditions, sizeof(Condition) * nConditions);

    attributes = child->attributes;
    attrCount = child->attrCount;        

    lhsCondAttr = new DataAttrInfo[nConditions];
    rhsCondAttr = new DataAttrInfo[nConditions];
    for (int i = 0; i < nConditions; i++) {
        Condition c = conditions[i];
        FindAttributeInfo(c.lhsAttr, attributes, attrCount, lhsCondAttr[i]);
        if (c.bRhsIsAttr) FindAttributeInfo(c.rhsAttr, attributes, attrCount, rhsCondAttr[i]);
    }
}

qFilter::~qFilter() {
    delete[] conditions;
    delete[] lhsCondAttr;
    delete[] rhsCondAttr;
}

RC qFilter::GetNext(RM_Record &rec) {
    while (true) {
        RC rc; char *pData;
        if ((rc = child->GetNext(rec))) return rc;
        if ((rc = rec.GetData(pData))) return rc;

        int conditionMet = 1;
        /* Go through each condition and see if the tuple matches all conditions */
        for (int i=0; i < nConditions; i++) {
            Condition c = conditions[i];
            int comp;
            if (c.bRhsIsAttr) {
                comp = CompareKey(pData + lhsCondAttr[i].offset, pData + rhsCondAttr[i].offset, 1, 
                    lhsCondAttr[i].attrType, lhsCondAttr[i].attrLength);
            } else {
                comp = CompareKey(pData + lhsCondAttr[i].offset, (char *) c.rhsValue.data, 1, 
                    lhsCondAttr[i].attrType, lhsCondAttr[i].attrLength);
            }
            conditionMet = CheckConditionMet(comp, c.op);
            if (!conditionMet) break;
        }
        if (conditionMet) return 0;
    }
}

void qFilter::PrintOp(string whitespace) {
    cout << whitespace << "<<FILTER>> on" << endl;
    for (int i = 0; i < nConditions; i++) {
        cout << whitespace << "conditions[" << i << "]:" << endl;
        PrintCondition(whitespace, conditions[i]);
    }
}


qUpdate::qUpdate(qNode *child, const DataAttrInfo &updAttrInfo, int bIsValue, const RelAttr &rhsAttr, const Value &rhsValue, RM_Manager *rmm, IX_Manager *ixm, LG_Manager *lgm) {
    type = UPDATE;
    this->child = child; this->rchild = NULL;
    attrCount = child->attrCount;
    attributes = child->attributes;

    this->updAttrInfo = updAttrInfo;
    this->bIsValue = bIsValue;
    this->rhsAttr = rhsAttr;
    this->rhsValue = rhsValue;
    this->rmm = rmm;
    this->ixm = ixm;
    this->lgm = lgm;

    p = new Printer(attributes, attrCount);    
    if (!bIsValue) FindAttributeInfo(rhsAttr, attributes, attrCount, rhsAttrInfo);

    initialized = 0;
}

qUpdate::~qUpdate() {
    delete p;
}

RC qUpdate::Begin() {
    RC rc;
    if ((rc = rmm->OpenFile(updAttrInfo.relName, fh))) return rc;

    if (updAttrInfo.indexNo != -1) {
        if ((rc = ixm->OpenIndex(updAttrInfo.relName, updAttrInfo.indexNo, ih))) return rc;
    }

    p->PrintHeader(cout);        
    initialized = 1;
    return 0;
}

RC qUpdate::GetNext(RM_Record &rec) {
    RC rc;
    if (!initialized) {
        if ((rc = Begin())) return rc;
    }

    if (bAbort && (rand() % 100) < abortProb) {
        cout << "[ CRASH WHILE DELETE ]" << endl;
        abort();
    }

    char *pData; RID rid;
    rc = child->GetNext(rec);
    
    /* Clean up after all results have been returned */
    if (rc == QL_ENDOFRESULT) {
        if (updAttrInfo.indexNo != -1) {
            if ((rc = ixm->CloseIndex(ih))) return rc;
        }
        if ((rc = rmm->CloseFile(fh))) return rc;
        p->PrintFooter(cout);
        return QL_ENDOFRESULT;
    } else if (rc) return rc;

    if ((rc = rec.GetData(pData))) return rc;
    if ((rc = rec.GetRid(rid))) return rc;

    /* Delete an index entry if the updated attribute is indexed */
    if (updAttrInfo.indexNo != -1) {
        if ((rc = ih.DeleteEntry(pData + updAttrInfo.offset, rid))) return rc;
    }

    /* Insert log record */
    LG_FullRec logRec;
    memset(&logRec, 0, sizeof(LG_FullRec));
    logRec.type = L_UPDATE;
    logRec.offset = updAttrInfo.offset;
    logRec.dataSize = updAttrInfo.attrLength;
    strncpy(logRec.fileName, updAttrInfo.relName, MAXNAME);
    logRec.rid = rid;
    if (bIsValue) {
        if ((rc = lgm->InsertLogRec(logRec, pData + updAttrInfo.offset, (char *)rhsValue.data))) return rc;
    } else {
        if ((rc = lgm->InsertLogRec(logRec, pData + updAttrInfo.offset, pData + rhsAttrInfo.offset))) return rc;
    }
    if ((rc = fh.UpdatePageLSN(rid, logRec.lsn))) return rc;

    /* Write new data for the updated attribute */
    if (bIsValue) {
        memcpy(pData + updAttrInfo.offset, rhsValue.data, updAttrInfo.attrLength);
    } else {
        memcpy(pData + updAttrInfo.offset, pData + rhsAttrInfo.offset, updAttrInfo.attrLength);
    }

    /* Update record */
    if ((rc = fh.UpdateRec(rec))) return rc;

    /* Re-insert an index entry if the updated attribute is indexed */
    if (updAttrInfo.indexNo != -1) {
        if ((rc = ih.InsertEntry(pData + updAttrInfo.offset, rid))) return rc;
    }
    
    /* Print the updated tuple */
    p->Print(cout, pData);
    return 0;
}

void qUpdate::PrintOp(string whitespace) {
    cout << whitespace << "<<UPDATE>> " << updAttrInfo.relName << "." << updAttrInfo.attrName << " = ";
    if (bIsValue) {
        cout << rhsValue << endl;
    } else {
        cout << rhsAttr << endl;
    }
}


qDelete::qDelete(qNode *child, const char *relName, RM_Manager *rmm, IX_Manager *ixm, LG_Manager *lgm) {
    type = DELETE;
    this->child = child; this->rchild = NULL;
    this->attrCount = child->attrCount;
    this->attributes = child->attributes;

    this->relName = relName;
    this->rmm = rmm;
    this->ixm = ixm;
    this->lgm = lgm;

    ih = new IX_IndexHandle[attrCount];
    p = new Printer(attributes, attrCount);
    initialized = 0;
}

qDelete::~qDelete() {
    delete p;
    delete[] ih;
}

RC qDelete::Begin() {
    RC rc;
    if ((rc = rmm->OpenFile(relName, fh))) return rc;
    for (int i = 0; i < attrCount; i++) {
        if (attributes[i].indexNo != -1) {
            if ((rc = ixm->OpenIndex(relName, attributes[i].indexNo, ih[i]))) return rc;
        }
    }

    p->PrintHeader(cout);        
    initialized = 1;
    tuplesDeleted = 0;
    return 0;
}

RC qDelete::GetNext(RM_Record &rec) {
    RC rc;
    if (!initialized) {
        if ((rc = Begin())) return rc;
    }

    if (bAbort && (rand() % 100) < abortProb) {
        cout << "[ CRASH WHILE DELETE ]" << endl;
        abort();
    }

    char *pData; RID rid;
    /* Get a tuple from its child */
    rc = child->GetNext(rec);

    /* Clean up after all results have been returned */
    if (rc == QL_ENDOFRESULT) {
        for (int i = 0; i < attrCount; i++) {
            if (attributes[i].indexNo != -1) {
                if ((rc = ixm->CloseIndex(ih[i]))) return rc;
            }
        }
        if ((rc = rmm->CloseFile(fh))) return rc;
        p->PrintFooter(cout);
        return QL_ENDOFRESULT;
    } else if (rc) return rc;

    if ((rc = rec.GetData(pData))) return rc;

    if ((rc = rec.GetRid(rid))) return rc;

    /* Insert Log Record */
    LG_FullRec logRec;
    memset(&logRec, 0, sizeof(LG_FullRec));
    logRec.type = L_DELETE;
    logRec.offset = 0;
    logRec.dataSize = fh.GetRecordSize();
    strncpy(logRec.fileName, relName, MAXNAME);
    logRec.rid = rid;
    if ((rc = lgm->InsertLogRec(logRec, pData, NULL))) return rc;
    if ((rc = fh.UpdatePageLSN(rid, logRec.lsn))) return rc;

    /* Delete record */
    if ((rc = fh.DeleteRec(rid))) return rc;
    tuplesDeleted++;

    /* Delete entry on indexed attributes */
    for (int i = 0; i < attrCount; i++) {
        if (attributes[i].indexNo != -1) {
            if ((rc = ih[i].DeleteEntry(pData + attributes[i].offset, rid))) return rc;
        }
    }

    /* Print the deleted tuple */
    p->Print(cout, pData);
    return 0;
}

void qDelete::PrintOp(string whitespace) {
    cout << whitespace << "<<DELETE>> from " << relName << endl;;
}