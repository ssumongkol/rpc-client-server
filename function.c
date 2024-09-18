#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include "function.h"
#include "rpc.h"

struct function {
    int id;
    char *name;
    rpc_handler obj;
};

struct functionList {
    function_t **function;
    int size;
    int n;
};

/* ------------------ */
/* function procedure */
/* ------------------ */

/* creates & returns an empty function node */
function_t *functionCreate(int name_len) {
	function_t *function = malloc(sizeof(*function));
	assert(function);
    function->name = malloc(sizeof(name_len));
    assert(function->name);
	function->obj = malloc(sizeof(*(function->obj)));
	assert(function->obj);
	return function;
}

/* assign name to function object */
void assignNameToFunction(function_t *function, char *name) {
    strcpy(function->name, name);
}

/* assign rpc_handler to function object */
void assignRPCHandlerToFunction(function_t *function, rpc_handler handler) {
	function->obj = handler;
}

/* get function_id from function object */
int getFidFunction(function_t *function) {
	return function->id;
}

/* ---------------------- */
/* functionList procedure */
/* ---------------------- */

/* creates & returns an empty functionList (array) */
functionList_t *functionListCreate() {
	functionList_t *functionList = malloc(sizeof(*functionList));
	assert(functionList);
	int size = INIT_SIZE;
	functionList->size = size;
	functionList->function = malloc(size * sizeof(*(functionList->function)));
	assert(functionList->function);
	functionList->n = 0;
	return functionList;
}

/* track array size & element, expand the size when needed */
/* (code from COMP20003 W3.8 skeleton code) */
void functionListEnsureSize(functionList_t *functionList) {
	if (functionList->n == functionList->size) {
		functionList->size *= 2;
		functionList->function = realloc(functionList->function, functionList->size * sizeof(*(functionList->function)));
		assert(functionList->function);
	}
}

/* check whether this function name is already exist in functionList or not
 * if YES, overwrite the existing function name with new function object (handler)
 * otherwise register this new function with new function name into functionList 
 */
/* (inspired from sortedArrayInsert(...) COMP20003 W3.8 skeleton code) */
void functionRegister(functionList_t *functionList, function_t *function) {
    functionListEnsureSize(functionList);
    int i;
    // loop to check whether this function name is already registered or not
    for (i = 0; i < functionList->n; i++) {
        if (strcmp(function->name, functionList->function[i]->name) == 0) {
            break;
        } 
    }
    if (i == functionList->n) {
        // add new function
        function->id = i+1;
        functionList->function[i] = function;
        (functionList->n)++;
    } else {
        // overwrite existing function_name with new function_obj (handler)
        functionList->function[i]->obj = function->obj;
    }
}

/* search for matched function obj from functionList by function name
 * otherwise return 0 (not found)
 */
int searchFunction(functionList_t *functionList, char *name) {
    for (int i = 0; i < functionList->n; i++) {
        if (strcmp(name, functionList->function[i]->name) == 0) {
            return functionList->function[i]->id;
        }
    }
    return 0;
}

/* get function obj (rpc_handler) from functionList using fid */
rpc_handler getHandlerFunctionList(functionList_t *functionList, int fid) {
    return functionList->function[fid-1]->obj;
}

/* free function */
void functionFree(function_t *function) {
    free(function->name);
    free(function->obj);
    free(function);
}

/* free functionList */
void functionListFree(functionList_t *functionList) {
    for (int i = 0; i < functionList->n; i++) {
        functionFree(functionList->function[i]);
    }
    free(functionList);
}
