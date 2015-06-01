//
// lg.h
//   Logging and Transction Component Interface
//


#ifndef LG_H
#define LG_H

#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "pf.h"
#include "rm_rid.h"
#include <iostream>
#include <stdio.h>
#include <set>
#include <string>

using namespace std;

class RM_Manager;

#define LG_EOF          -1
#define LOGFILENAME     "log"

enum LogType {
    L_INSERT, L_DELETE, L_UPDATE, L_COMMIT, L_ABORT, L_END, L_CLR
};

struct LSN {
    PageNum pn;     // Page number of this record
    int offset;     // Offset
    
    LSN() {
        this->pn = LG_EOF;
        this->offset = LG_EOF;
    }

    LSN(PageNum pn, int offset) {
        this->pn = pn;
        this->offset =offset;
    }

    LSN& operator=(const LSN &l) {
       if (this != &l) {
          this->pn = l.pn;
          this->offset = l.offset;
       }
       return (*this);
    };
};

struct LG_Rec {
    LSN lsn;
    int XID;
    LogType type;
    LSN prevLSN;
    LSN nextLSN;
};

struct LG_FullRec : LG_Rec {
    int offset;         // Offset of the attribute modified
    int dataSize;       // Size of data modified/inserted or deleted
    RID rid;
    LogType undoType;   // Only for CLR; Type of undo operation
    LSN undonextLSN;    // Only for CLR
    char fileName[MAXNAME];
};


//
// QL_Manager: query language (DML)
//
class LG_Manager {
    friend class QL_Manager;
    friend class SM_Manager;

public:
    LG_Manager (PF_Manager &pfm, RM_Manager &rmm);
    ~LG_Manager();                       // Destructor

    RC CreateLog();
    RC DestroyLog();
    RC InsertLogRec(LG_FullRec &logRec, const char *oldValue = NULL, const char *newValue = NULL);
    RC FlushLog();
    RC BeginT();
    RC CommitT();
    RC AbortT();
    RC Checkpoint();
    RC Recover();
    RC PrintLog(int inRecovery);

private:
    RC Undo(LSN &undoLSN, const int XID);
    RC Redo(LSN &redoLSN, LSN &lastLSN, int &lastXID);
    RC UpdateLastRecNextLSN(LSN nextLSN);
    
    PF_Manager *pfm_;
    RM_Manager *rmm_;

    PF_FileHandle logFile_;         // FileHandle to log file
    PF_PageHandle currLogPage_;     // Page of the log containing the last record
    
    int availableSpace;             // Available space in currentLogPage_
    int bInTransaction;             // 1 if there is a transaction running currently
    LSN lastRec;                    // last log record
    int lastRecSize;                // Size of the last log record
    int currXID;                    // ID for the currnet transaction
    
    set<string> dirtyFiles;              // unixfd of files where INSERT/DELETE/UPDATE operations occurred
};

//
// Print-error function
//
void LG_PrintError(RC rc);

#define LG_INTRANSACTION                (START_LG_WARN + 0)
#define LG_NOTINTRANSACTION             (START_LG_WARN + 1)
#define LG_LOGEMPTY                     (START_LG_WARN + 2)

#define LG_V                            (START_LG_WARN+50)
#define LG_LASTWARN                     LG_V

#define LG_LASTERROR                    (END_LG_ERR)

#endif
