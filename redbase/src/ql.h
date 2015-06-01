//
// ql.h
//   Query Language Component Interface
//

// This file only gives the stub for the QL component

#ifndef QL_H
#define QL_H

#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql_node.h"

//
// QL_Manager: query language (DML)
//
class QL_Manager {
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm, LG_Manager &lgm);
    ~QL_Manager();                       // Destructor

    RC Select  (int nSelAttrs,           // # attrs in select clause
        const RelAttr selAttrs[],        // attrs in select clause
        int   nRelations,                // # relations in from clause
        const char * const relations[],  // relations in from clause
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Insert  (const char *relName,     // relation to insert into
        int   nValues,                   // # values
        const Value values[]);           // values to insert

    RC Delete  (const char *relName,     // relation to delete from
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Update  (const char *relName,     // relation to update
        const RelAttr &updAttr,          // attribute to update
        const int bIsValue,              // 1 if RHS is a value, 0 if attribute
        const RelAttr &rhsRelAttr,       // attr on RHS to set LHS equal to
        const Value &rhsValue,           // or value to set attr equal to
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

private:
    RM_Manager *rmm_;
    IX_Manager *ixm_;
    SM_Manager *smm_;
    LG_Manager *lgm_;
    RC ValidateAttrForSelect(const RelAttr &attr, int nRelations, 
        const char * const relations[], AttrType &attrType);
};

//
// Print-error function
//
void QL_PrintError(RC rc);

#define QL_OPERANDSINCOMPATIBLE         (START_QL_WARN + 1)
#define QL_DUPLICATEREL                 (START_QL_WARN + 2)
#define QL_RELNOTINWHERECLAUSE          (START_QL_WARN + 3)
#define QL_AMBIGUOUSATTR                (START_QL_WARN + 4) 
#define QL_ATTRCOUNTMISMATCH            (START_QL_WARN + 5) 
#define QL_ATTRNOTEXIST                 (START_QL_WARN + 6)
#define QL_LASTWARN                     QL_ATTRNOTEXIST

#define QL_LASTERROR                    (END_QL_ERR)

#endif
