#include "lg.h"
#include "rm.h"
#include "rm_internal.h"
#include "parser.h"

/* Helper functions */
int CalculateLogRecordSize(LG_FullRec &logRec);
char *GetLogRecData(LSN lsn, PF_PageHandle &ph);
void DumpLogContent(LG_Rec *rec);

LG_Manager::LG_Manager (PF_Manager &pfm, RM_Manager &rmm) {
    pfm_ = &pfm;
    rmm_ = &rmm;
    rmm_->lgmanager_ = this;
}

LG_Manager::~LG_Manager() {

}

RC LG_Manager::CreateLog() {
    RC rc = pfm_->CreateFile(LOGFILENAME);
    if (rc == PF_UNIX) {
        cout << "Crash detected. Starting recovery... " << endl;
        if ((rc = pfm_->OpenFile(LOGFILENAME, logFile_))) return rc;
        if ((rc = Recover())) return rc;
        cout << "Recovery complete. " << endl;
    } else if (rc) {
        return rc;
    } else {
        if ((rc = pfm_->OpenFile(LOGFILENAME, logFile_))) return rc;
    }

    /* Initialize instance variables */
    bInTransaction = 0;            
    lastRec.pn = LG_EOF;
    currXID = 0;

    return rc;
}

RC LG_Manager::DestroyLog() {
    return pfm_->DestroyFile(LOGFILENAME);
}

char *GetLogRecData(LSN lsn, PF_PageHandle &ph) {
    char *pData;
    ph.GetData(pData);
    return pData + lsn.offset;
}

RC LG_Manager::InsertLogRec(LG_FullRec &logRec, const char *oldValue, const char *newValue) {
    RC rc;
    int logRecordSize = CalculateLogRecordSize(logRec);

    /* Find where the next log record will be inserted */
    PageNum pn;
    /* When inserting the very first record or there is no avilable space, allocate new page */
    if (lastRec.pn == LG_EOF || availableSpace < logRecordSize) {
        if (lastRec.pn != LG_EOF) {
            /* Unpin current page */
            currLogPage_.GetPageNum(pn);
            if ((rc = logFile_.UnpinPage(pn))) return rc;
        }
        /* Allocate new page */
        if ((rc = logFile_.AllocatePage(currLogPage_))) return rc;
        currLogPage_.GetPageNum(pn);

        logRec.lsn = LSN(pn, 0);
        availableSpace = PF_PAGE_SIZE;
    } else {
        /* Get slot following the last record */
        currLogPage_.GetPageNum(pn);
        logRec.lsn = LSN(pn, lastRec.offset + lastRecSize);
    }


    logRec.nextLSN.pn = LG_EOF;
    logRec.prevLSN = lastRec;
    logRec.XID = currXID;

    /* Pack logRec, oldValue and newValue into logRec */
    char newLogRecord[logRecordSize];
    memset(newLogRecord, 0, logRecordSize);
    if (logRecordSize == sizeof(LG_Rec)) {
        memcpy(newLogRecord, &logRec, logRecordSize);
    } else {
        memcpy(newLogRecord, &logRec, sizeof(LG_FullRec));
        char *ptr = newLogRecord + sizeof(LG_FullRec);
        if (oldValue) {
            memcpy(ptr, oldValue, logRec.dataSize);
            ptr += logRec.dataSize;
        }
        if (newValue) memcpy(ptr, newValue, logRec.dataSize);
    }

    char *pData;
    currLogPage_.GetData(pData);
    /* Insert the new log record */
    memcpy(pData + logRec.lsn.offset, newLogRecord, logRecordSize);
    if ((rc = logFile_.MarkDirty(logRec.lsn.pn))) return rc;

    /* Update nextLSN of last rec */
    if ((rc = UpdateLastRecNextLSN(logRec.lsn))) return rc;

    /* Add file to a list of dirty files */
    if (logRec.type == L_INSERT || logRec.type == L_DELETE || logRec.type == L_UPDATE)
        dirtyFiles.insert(string(logRec.fileName));

    /* Update available space and lastRec */
    availableSpace -= logRecordSize;
    lastRec = logRec.lsn;
    lastRecSize = logRecordSize;

    return 0;
}

RC LG_Manager::UpdateLastRecNextLSN(LSN nextLSN) {
    RC rc;
    /* Update nextLSN of last log record */
    if (lastRec.pn != LG_EOF) {
        PF_PageHandle ph;
        if ((rc = logFile_.GetThisPage(lastRec.pn, ph))) return rc;
        LG_Rec *lastLogRecData = (LG_Rec *) GetLogRecData(lastRec, ph);
        lastLogRecData->nextLSN = nextLSN;
        if ((rc = logFile_.MarkDirty(lastRec.pn))) return rc;
        if ((rc = logFile_.UnpinPage(lastRec.pn))) return rc;
    }
    return 0;
}



/* Calculates how much space a log record will need based on record type */
int CalculateLogRecordSize(LG_FullRec &logRec) {
    LogType type = logRec.type;
    if (type == L_COMMIT || type == L_ABORT || type == L_END)
        return sizeof(LG_Rec);

    if (type == L_INSERT || type == L_DELETE)
        return sizeof(LG_FullRec) + logRec.dataSize;

    if (type == L_UPDATE)
        return sizeof(LG_FullRec) + 2*logRec.dataSize;

    if (type == L_CLR) {
        LogType undoType = logRec.undoType;
        if (undoType == L_INSERT || undoType == L_DELETE)
            return sizeof(LG_FullRec) + logRec.dataSize;
        if (undoType == L_UPDATE)
            return sizeof(LG_FullRec);
    }
    return -1;
}

RC LG_Manager::FlushLog() {
    return logFile_.ForcePages();
}

RC LG_Manager::BeginT() {
    cout << "begin transaction" << endl;
    if (bInTransaction) return LG_INTRANSACTION;
    bInTransaction = 1;
    currXID++;
    return 0;
}

void DumpLogContent(LG_Rec *rec) {
    cout << "------------------------------------------------------------------------------" << endl;

    cout << "LSN: " << (rec->lsn).pn << "." << (rec->lsn).offset << endl;
    cout << "XID: " << rec->XID << endl;

    LogType type = rec->type;
    cout << "Log Type: ";
    switch (type) {
        case L_INSERT:
            cout << "Insert" << endl; break;
        case L_DELETE:
            cout << "Delete" << endl; break;
        case L_UPDATE:
            cout << "Update" << endl; break;
        case L_COMMIT:
            cout << "Commit" << endl; break;
        case L_ABORT:
            cout << "Abort" << endl; break;
        case L_END:
            cout << "End" << endl; break;
        case L_CLR:
            cout << "CLR" << endl; break;
    }

    if (type == L_INSERT || type == L_DELETE || type == L_UPDATE || type == L_CLR) {
        LG_FullRec *fullRec = (LG_FullRec *) rec;
        cout << "Data Offset: " << fullRec->offset << endl;
        cout << "Data Size: " << fullRec->dataSize << endl;
        cout << "fileName: " << fullRec->fileName << endl;

        PageNum pn; SlotNum sn;
        (fullRec->rid).GetPageNum(pn);
        (fullRec->rid).GetSlotNum(sn);
        cout << "Modified Page: " << pn << ":" << sn << endl;

        if (type == L_CLR) {
            cout << "Type of log record undone: ";
            switch (fullRec->undoType) {
                case L_INSERT:
                    cout << "Insert" << endl; break;
                case L_UPDATE:
                    cout << "Update" << endl; break;
                case L_DELETE:
                    cout << "Delete" << endl; break;
                default:
                    cout << "INVALID" << endl; break;
            }
            cout << "Next LSN to undo: " << (fullRec->undonextLSN).pn << "." << (fullRec->undonextLSN).offset << endl;
        }
    }
}


RC LG_Manager::PrintLog(int inRecovery) {
    cout << "print log" << endl;
    RC rc;

    if (!inRecovery && lastRec.pn == LG_EOF) {
        cout << "The log is empty" << endl;
        return 0;
    }

    PF_PageHandle ph;
    if ((rc = logFile_.GetFirstPage(ph))) return rc;

    char *pData; PageNum pn;
    ph.GetData(pData);
    ph.GetPageNum(pn);
    cout << "PL PN" << pn << endl;
    LG_Rec *rec = (LG_Rec *) pData;

    while (true) {
        DumpLogContent(rec);
        if (rec->nextLSN.pn != pn) {
            if ((rc = logFile_.UnpinPage(pn))) return rc;
            if ((rec->nextLSN).pn == LG_EOF) return 0;
            if ((rc = logFile_.GetThisPage((rec->nextLSN).pn, ph))) return rc;
            ph.GetData(pData);
            ph.GetPageNum(pn);
            rec = (LG_Rec *) pData;
        } else {
            rec = (LG_Rec *) (pData + (rec->nextLSN).offset);
        }
    }
}

RC LG_Manager::CommitT() {
    cout << "commit transaction" << endl;
    RC rc;
    if (!bInTransaction) return LG_NOTINTRANSACTION;

    if (bAbort) abort();

    LG_FullRec commitRec;
    commitRec.type = L_COMMIT;
    if ((rc = InsertLogRec(commitRec))) return rc;
    if ((rc = FlushLog())) return rc;

    bInTransaction = 0; 
    return rc;
}

RC LG_Manager::AbortT() {
    cout << "abort transaction" << endl;
    RC rc;
    if (!bInTransaction) return LG_NOTINTRANSACTION;

    LG_FullRec abortRec;
    abortRec.type = L_ABORT;
    if ((rc = InsertLogRec(abortRec))) return rc;
    if ((rc = FlushLog())) return rc;

    if ((rc = Undo(lastRec, currXID))) return rc;

    LG_FullRec endRec;
    endRec.type = L_END;
    if ((rc = InsertLogRec(endRec))) return rc;
    if ((rc = FlushLog())) return rc;

    bInTransaction = 0; 
    return rc;
}

RC LG_Manager::Checkpoint() {
    cout << "checkpoint" << endl;
    RC rc;
    if (bInTransaction) return LG_INTRANSACTION;

    /* Flush log records to disk by closing log file */
    PageNum pn;
    currLogPage_.GetPageNum(pn);
    if ((rc = logFile_.UnpinPage(pn))) return rc;
    if ((rc = pfm_->CloseFile(logFile_))) return rc;

    /* Flush all dirty record pages to disk */
    for (set<string>::iterator it = dirtyFiles.begin(); it != dirtyFiles.end(); ++it) {
        PF_FileHandle dirtyfh;
        if ((rc = pfm_->OpenFile((*it).c_str(), dirtyfh))) return rc;
        if ((rc = dirtyfh.ForcePages())) return rc;
    }
    dirtyFiles.clear();

    if ((rc = DestroyLog())) return rc;
    if ((rc = CreateLog())) return rc;
    return 0;
}

RC LG_Manager::Undo(LSN &undoLSN, const int XID) {
    RC rc;
    /* Stop undoing when LG_EOF is encountered */
    if (undoLSN.pn == LG_EOF) return 0;

    PF_PageHandle ph;
    if ((rc = logFile_.GetThisPage(undoLSN.pn, ph))) return rc;

    LG_Rec *logData = (LG_Rec *) GetLogRecData(undoLSN, ph);
    LogType type = logData->type;
    DumpLogContent(logData);

    /* Stop undoing when log record of another transaction is encountered
     * or if the record is COMMIT or END
     */
    if (logData->XID != XID || type == L_COMMIT || type == L_END) {
        return logFile_.UnpinPage(undoLSN.pn);
    } 

    /* Undo INSERT, DELETE and UPDATE log records */
    LG_FullRec *logRec = (LG_FullRec *) logData;

    if (type == L_INSERT || type == L_DELETE || type == L_UPDATE) {
        RM_FileHandle fh;
        RID rid = logRec->rid;
        char *filename = logRec->fileName;
        if ((rc = rmm_->OpenFile(filename, fh))) return rc;

        cout << "A" << endl;
        // Insert a compensation log record
        LG_FullRec clrRec;
        clrRec.XID = XID;
        clrRec.type = L_CLR;
        clrRec.offset = logRec->offset;
        clrRec.dataSize = logRec->dataSize;
        strncpy(clrRec.fileName, filename, MAXNAME);
        clrRec.rid = rid;
        clrRec.undonextLSN = logRec->prevLSN;
        cout << "B" << endl;

        if (type == L_INSERT) {
            clrRec.undoType = L_DELETE;
            rc = InsertLogRec(clrRec, NULL, NULL);
        } else if (type == L_DELETE) {
            clrRec.undoType = L_INSERT;
            rc = InsertLogRec(clrRec, NULL, (char *) logRec + sizeof(LG_FullRec));
        } else if (type == L_UPDATE) {
            clrRec.undoType = L_UPDATE;
            rc = InsertLogRec(clrRec, NULL, (char *) logRec + sizeof(LG_FullRec));
        }
        if (rc) return rc;
        if ((rc = fh.UpdatePageLSN(rid, clrRec.lsn))) return rc;

        cout << "C" << endl;

        PageNum pn;
        rid.GetPageNum(pn);
        // Execute Undo
        if (type == L_INSERT) {
            if ((rc = fh.DeleteRec(rid))) return rc;
        } else if (type == L_DELETE) {
            char *oldValue = (char *) logRec + sizeof(LG_FullRec);
            if ((rc = fh.InsertRecAtRid(oldValue, rid))) return rc;
        } else if (type == L_UPDATE) {
            char *oldValue = (char *) logRec + sizeof(LG_FullRec);
            RM_Record rmrec;
            if ((rc = fh.GetRec(rid, rmrec))) return rc;
            memcpy(rmrec.contents_ + logRec->offset, oldValue, logRec->dataSize);
            if ((rc = fh.UpdateRec(rmrec))) return rc;
        }
        cout << "D" << endl;

        if ((rc = rmm_->CloseFile(fh))) return rc;
    }

    cout << "E" << endl;
    if ((rc = logFile_.UnpinPage(undoLSN.pn))) return rc;
    
    cout << "F" << endl;
    /* Keep undoing previous log record */
    LSN prevLSN;
    if (type == L_CLR) prevLSN = logRec->undonextLSN;
    else prevLSN = logData->prevLSN;
    cout << "prevLSN" << prevLSN.pn << "." << prevLSN.offset << endl;
    return Undo(prevLSN, XID);
}

/* Returns 1 if pageLSN is lesser than redoLSN */
int CompareLSN(const LSN &pageLSN, const LSN &redoLSN) {
    cout << "PageLSN" << pageLSN.pn << ":" << pageLSN.offset << endl;
    cout << "PageLSN" << redoLSN.pn << ":" << redoLSN.offset << endl;

    if (pageLSN.pn < redoLSN.pn) return 1;
    if (pageLSN.pn > redoLSN.pn) return 0;
    if (pageLSN.offset < redoLSN.offset) return 1;
    return 0;
}

RC LG_Manager::Redo(LSN &redoLSN, LSN &lastLSN, int &lastXID) {
    RC rc;

    PF_PageHandle ph;
    if ((rc = logFile_.GetThisPage(redoLSN.pn, ph))) return rc;

    LG_Rec *logData = (LG_Rec *) GetLogRecData(redoLSN, ph);
    LogType type = logData->type;
    DumpLogContent(logData);

    if (type == L_INSERT || type == L_DELETE || type == L_UPDATE || type == L_CLR) {
        LG_FullRec *logRec = (LG_FullRec *) logData;
        RM_FileHandle fh;
        RID rid = logRec->rid;
        char *filename = logRec->fileName;
        if ((rc = rmm_->OpenFile(filename, fh))) return rc;

        /* Get PageLSN on record file page */
        LSN pageLSN;
        if ((rc = fh.GetPageLSN(rid, pageLSN))) return rc;
        
        // Redo if pageLSN < LSN of log record
        if (CompareLSN(pageLSN, redoLSN)) {
            LogType undoType = logRec->undoType;
            /* Execute Redo */
            if (type == L_INSERT || (type == L_CLR && undoType == L_INSERT)) {
                char *newValue = (char *) logRec + sizeof(LG_FullRec);
                if ((rc = fh.InsertRecAtRid(newValue, rid))) return rc;

            } else if (type == L_DELETE || (type == L_CLR && undoType == L_DELETE)) {
                if ((rc = fh.DeleteRec(rid))) return rc;
            
            } else if (type == L_UPDATE || (type == L_CLR && undoType == L_UPDATE)) {
                char *newValue = (char *) logRec + sizeof(LG_FullRec) + logRec->dataSize;
                RM_Record rmrec;
                if ((rc = fh.GetRec(rid, rmrec))) return rc;
                memcpy(rmrec.contents_ + logRec->offset, newValue, logRec->dataSize);
                if ((rc = fh.UpdateRec(rmrec))) return rc;
            }
            if ((rc = fh.UpdatePageLSN(rid, redoLSN))) return rc;
        }

        if ((rc = rmm_->CloseFile(fh))) return rc;
    }

    if ((rc = logFile_.UnpinPage(redoLSN.pn))) return rc;
    
    /* If end of file, return */
    if ((logData->nextLSN).pn == LG_EOF) {
        lastLSN = redoLSN;
        lastXID = logData->XID;
        return 0;

    /* Keep redoing the next log record */
    } else return Redo(logData->nextLSN, lastLSN, lastXID);
}

RC LG_Manager::Recover() {
    RC rc;
    if ((rc = PrintLog(1))) return rc;
    LSN firstLSN = LSN(0, 0);
    LSN lastLSN; int lastXID;
    if ((rc = Redo(firstLSN, lastLSN, lastXID))) return rc;

    cout << lastLSN.pn << "." << lastLSN.offset << endl;
    cout << lastXID << endl;
    cout << "TIME FOR UNDO" << endl;
    if ((rc = Undo(lastLSN, lastXID))) return rc;
    return 0;
}
