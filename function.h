#ifndef FUNCTION_H
#define FUNCTION_H
#include "rpc.h"

#define INIT_SIZE 2

// data definitions
typedef struct function function_t;
typedef struct functionList functionList_t;

/* ------------------ */
/* function procedure */
/* ------------------ */

/* creates & returns an empty function node */
function_t *functionCreate();

/* assign name to function object */
void assignNameToFunction(function_t *function, char *name);

/* assign rpc_handler to function object */
void assignRPCHandlerToFunction(function_t *function, rpc_handler handler);

/* get function_id from function object */
int getFidFunction(function_t *function);

/* ---------------------- */
/* functionList procedure */
/* ---------------------- */

/* creates & returns an empty functionList (array) */
functionList_t *functionListCreate();

/* track array size & element, expand the size when needed */
/* (code from COMP20003 W3.8 skeleton code) */
void functionListEnsureSize(functionList_t *functionList);

/* check whether this function name is already exist in functionList or not
 * if YES, overwrite the existing function name with new function object (handler)
 * otherwise register this new function with new function name into functionList 
 */
/* (inspired from sortedArrayInsert(...) COMP20003 W3.8 skeleton code) */
void functionRegister(functionList_t *functionList, function_t *function);

/* search for matched function obj from functionList by function name
 * otherwise return 0 (not found)
 */
int searchFunction(functionList_t *functionList, char *name);

/* get function obj (rpc_handler) from functionList using fid */
rpc_handler getHandlerFunctionList(functionList_t *functionList, int fid);

/* free function */
void functionFree(function_t *function);

/* free functionList */
void functionListFree(functionList_t *functionList);

#endif
