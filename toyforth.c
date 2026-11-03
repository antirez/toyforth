#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

/* =============================== Data structrues ========================== */

#define TF_OK 0
#define TF_ERR 1

#define TFOBJ_TYPE_INT 0
#define TFOBJ_TYPE_STR 1
#define TFOBJ_TYPE_BOOL 2
#define TFOBJ_TYPE_LIST 3
#define TFOBJ_TYPE_SYMBOL 4
#define TFOBJ_TYPE_ALL 255 // Used by listPopType() and other functions.

typedef struct tfobj {
    int refcount;
    int type; // TFOBJ_TYPE_*
    union {
        int i;
        struct {
            char *ptr;
            size_t len;
        } str;
        struct {
            struct tfobj **ele;
            size_t len;
        } list;
    };
} tfobj;

typedef struct tfparser {
    char *prg; // The program to compile into a list.
    char *p;   // Next token to parse.
} tfparser;

/* Function table entry: each of this entry represents a symbol name
 * associated with a function implementation. */
struct tfctx;
typedef struct FunctionTableEntry {
    tfobj *name;
    int (*callback) (struct tfctx *ctx, char *name);
    tfobj *user_func;
} tffuncentry;

struct FunctionTable {
    tffuncentry **func_table;
    size_t func_count;
};

typedef struct Loop {
    int index;
    int limit;
    size_t start;
} tfloop;

/* Active loops, ordered from outermost to innermost. */
typedef struct LoopList {
    tfloop *loops;
    size_t count;
} tflooplist;

typedef struct {
    const char *name; /* Borrowed until the program is released. */
    size_t calls;
    double total_ms;
} tfprofileentry;

typedef struct {
    tfprofileentry *entries;
    size_t count;
} tfcallstats;

/* Optional profiling hooks. */
typedef struct tfprofiler {
    tfcallstats *stats;
    double (*clock)(void);
    void (*record)(tfcallstats *stats, const char *word, double elapsed_ms);
} tfprofiler;

/* Our execution context. */
typedef struct tfctx {
    tfobj *stack;
    struct FunctionTable functable;
    struct LoopList looplist;
    const tfprofiler *profiler; /* NULL disables profiling. */
} tfctx;

/* ========================== Function prototypes =========================== */

void retain(tfobj *o);
void release(tfobj *o);

// Standard library prototypes.
int basicMathFunctions(tfctx *ctx, char *name);
int dupFunction(tfctx *ctx, char *name);
int overFunction(tfctx *ctx, char *name);
int listFunction(tfctx *ctx, char *name);
int pickFunction(tfctx *ctx, char *name);
int getFunction(tfctx *ctx, char *name);
int setFunction(tfctx *ctx, char *name);
int lenFunction(tfctx *ctx, char *name);
int swapFunction(tfctx *ctx, char *name);
int dropFunction(tfctx *ctx, char *name);
int emitFunction(tfctx *ctx, char *name);

// user-defined functions
void registerUserFunction(tfctx *ctx, const tffuncentry *function);

// Execute a program.
int exec(tfctx *ctx, tfobj *prg);

/* ============================ Allocation wrappers ========================= */

void *xmalloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr,"Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *oldptr, size_t size) {
    if (size == 0) {
        free(oldptr);
        return NULL;
    }
    void *ptr = realloc(oldptr, size);
    if (ptr == NULL) {
        fprintf(stderr,"Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

/* ========================= Object related functions =======================
 * The following functions allocate Toy Forth objects of different types. */

/* Allocate and initialize a new Toy Forth object. */
tfobj *createObject(int type) {
    tfobj *o = xmalloc(sizeof(tfobj));
    o->type = type;
    o->refcount = 1;
    return o;
}

tfobj *createIntObject(int i) {
    tfobj *o = createObject(TFOBJ_TYPE_INT);
    o->i = i;
    return o;
}

tfobj *createBoolObject(int i) {
    tfobj *o = createObject(TFOBJ_TYPE_BOOL);
    o->i = i;
    return o;
}

/* Free an object and all the other nexted objects. */
void freeObject(tfobj *o) {
    switch(o->type) {
    case TFOBJ_TYPE_LIST:
        for(size_t j = 0; j < o->list.len; j++) {
            tfobj *ele = o->list.ele[j];
            release(ele);
        }
        free(o->list.ele);
        break;
    case TFOBJ_TYPE_SYMBOL:
    case TFOBJ_TYPE_STR:
        free(o->str.ptr);
        break;
    }
    free(o);
}

void retain(tfobj *o) {
    o->refcount++;
}

void release(tfobj *o) {
    assert(o->refcount > 0);
    o->refcount--;
    if (o->refcount == 0) freeObject(o);
}

void printObject(tfobj *o) {
    switch(o->type) {
    case TFOBJ_TYPE_INT:
        printf("%d", o->i);
        break;
    case TFOBJ_TYPE_LIST:
        printf("[");
        for(size_t j = 0; j < o->list.len; j++) {
            tfobj *ele = o->list.ele[j];
            printObject(ele);
            if (j != o->list.len-1)
                printf(" ");
        }
        printf("]");
        break;
    case TFOBJ_TYPE_STR:
        printf("\"%s\"", o->str.ptr);
        break;
    case TFOBJ_TYPE_SYMBOL:
        printf("%s", o->str.ptr);
        break;
    default:
        printf("?");
        break;
    }
}

/* =============================== String object ============================ */

tfobj *createStringObject(char *s, size_t len) {
    tfobj *o = createObject(TFOBJ_TYPE_STR);
    o->str.ptr = xmalloc(len+1);
    o->str.len = len;
    memcpy(o->str.ptr, s, len);
    o->str.ptr[len] = 0;
    return o;
}

tfobj *createSymbolObject(char *s, size_t len) {
    tfobj *o = createStringObject(s,len);
    o->type = TFOBJ_TYPE_SYMBOL;
    return o;
}

/* Compare the two string objects 'a' and 'b', returns 0 if they are
 * the same, '1' if a>b, '-1' if a<b. The comparsion is performed
 * using memcmp(). */
int compareStringObject(tfobj *a, tfobj *b) {
    size_t minlen = a->str.len < b->str.len ? a->str.len : b->str.len;
    int cmp = memcmp(a->str.ptr, b->str.ptr, minlen);

    if (cmp == 0) {
        if (a->str.len == b->str.len) return 0;
        else if (a->str.len > b->str.len) return 1;
        else return -1;
    } else {
        if (cmp < 0) return -1;
        else return 1;
    }
}

/* ================================ List object ============================= */

tfobj *createListObject(void) {
    tfobj *o = createObject(TFOBJ_TYPE_LIST);
    o->list.ele = NULL;
    o->list.len = 0;
    return o;
}

/* Add the new element at the end of the list 'list'.
 * It is up to the caller to increment the reference count of the
 * element added to the list, if needed. */
void listPush(tfobj *l, tfobj *ele) {
    l->list.ele = xrealloc(l->list.ele, sizeof(tfobj*) * (l->list.len+1));
    l->list.ele[l->list.len] = ele;
    l->list.len++;
}

tfobj *listPopType(tfctx *ctx, int type) {
    tfobj *stack = ctx->stack;
    if (stack->list.len == 0) return NULL;
    tfobj *to_pop = stack->list.ele[stack->list.len-1];
    if (type != TFOBJ_TYPE_ALL && to_pop->type != type) return NULL;

    stack->list.len--;
    if (stack->list.len == 0) {
        free(stack->list.ele);
        stack->list.ele = NULL;
    } else {
        stack->list.ele = xrealloc(stack->list.ele,
                            sizeof(tfobj*) * (stack->list.len));
    }
    return to_pop;
}

tfobj *listPop(tfctx *ctx) {
    return listPopType(ctx,TFOBJ_TYPE_ALL);
}

/* ===================== Turn program into toy forth list =================== */
void parseSpaces(tfparser *parser) {
    while(isspace(parser->p[0])) parser->p++;
}

#define MAX_NUM_LEN 128
tfobj *parseNumber(tfparser *parser) {
    char buf[MAX_NUM_LEN];
    char *start = parser->p;
    char *end;

    if (parser->p[0] == '-' || parser->p[0] == '+') parser->p++;
    while(parser->p[0] && isdigit(parser->p[0])) parser->p++;
    if (isalpha((unsigned char)parser->p[0]) || parser->p[0] == '_')
        return NULL;
    end = parser->p;
    int numlen = end-start;
    if (numlen >= MAX_NUM_LEN) return NULL;

    memcpy(buf, start, numlen);
    buf[numlen] = 0;

    tfobj *o = createIntObject(atoi(buf));
    return o;
}

/* Return true if 'c' is acceptable within a function name. */
int is_symbol_char(int c) {
    return isalnum((unsigned char)c) || c == '_';
}

tfobj *parseSymbol(tfparser *parser) {
    char *start = parser->p;
    while(parser->p[0] && is_symbol_char(parser->p[0])) parser->p++;
    int len = parser->p - start;
    return createSymbolObject(start,len);
}

/* Read one integer, function name, or operator. */
tfobj *parseToken(tfparser *parser) {
    /* A sign starts a number only at the start or after a separator or whitespace. */
    if (isdigit(parser->p[0]) ||
        ((parser->p[0] == '-' || parser->p[0] == '+') &&
         isdigit(parser->p[1]) &&
         (parser->p == parser->prg || isspace(parser->p[-1]) ||
          parser->p[-1] == ';')))
    {
        return parseNumber(parser);
    } else if(isalpha((unsigned char)parser->p[0]) || parser->p[0] == '_') {
        return parseSymbol(parser);
    } else if(parser->p[0] && strchr("+-*/%=",parser->p[0]) != NULL) {
        tfobj *o = createSymbolObject(parser->p,1);
        parser->p++;
        return o;
    } else {
        return NULL;
    }
}

/* Read and register one definition, without executing its body. */
int parseUserFunction(tfctx *ctx, tfparser *parser) {
    parser->p++; // Skip ':'.
    parseSpaces(parser);
    if (!isalpha((unsigned char)parser->p[0]) && parser->p[0] != '_')
        return TF_ERR;

    tffuncentry function;
    function.name = parseSymbol(parser);
    function.user_func = createListObject();
    int result = TF_ERR;

    while(parser->p[0]) {
        parseSpaces(parser);
        if (parser->p[0] == ';') {
            parser->p++;
            registerUserFunction(ctx,&function);
            result = TF_OK;
            break;
        }
        tfobj *o = parseToken(parser);
        if (o == NULL) break;
        listPush(function.user_func,o);
    }
    release(function.name);
    release(function.user_func);
    return result;
}

tfobj *compile(tfctx *ctx, char *prg) {
    tfparser parser;
    parser.prg = prg;
    parser.p = prg;

    tfobj *parsed = createListObject();

    while(parser.p) {
        char *token_start = parser.p;

        parseSpaces(&parser);
        if (parser.p[0] == 0) break; // End of program reached.
        if (parser.p[0] == ';') {
            parser.p++;
            continue;
        }
        if (parser.p[0] == ':') {
            if (parseUserFunction(ctx,&parser) == TF_ERR) {
                release(parsed);
                printf("Syntax error near: %32s ...\n", token_start);
                return NULL;
            }
            continue;
        }

        tfobj *o = parseToken(&parser);
        // Check if the current token produced a parsing error.
        if (o == NULL) {
            release(parsed);
            printf("Syntax error near: %32s ...\n", token_start);
            return NULL;
        } else {
            listPush(parsed,o);
        }
    }
    return parsed;
}

/* ======================== Execution and context =========================== */

int ctxCheckStackMinLen(tfctx *ctx, size_t min) {
    return (ctx->stack->list.len < min) ? TF_ERR : TF_OK;
}

/* Pop the top element from the interpreter main stack, assuming it
 * will match 'type', otherwise NULL is returned. Also the function
 * returns NULL if the stack is empty.
 *
 * The reference counting of the popped object is not modified: it
 * is assumed that we just transfer the ownership from the stack to
 * the caller. */
tfobj *ctxStackPop(tfctx *ctx, int type) {
    return listPopType(ctx, type);
}

/* Just push the object on the interpreter main stack. */
void ctxStackPush(tfctx *ctx, tfobj *obj) {
    listPush(ctx->stack, obj);
}

/* Resolve the function scanning the function table looking for a matching
 * name. If a matching function was not found, NULL is returned, otherwise
 * the function returns the function entry object. */
tffuncentry *getFunctionByName(tfctx *ctx, tfobj *name) {
    for (size_t j = 0; j < ctx->functable.func_count; j++) {
        tffuncentry *fe = ctx->functable.func_table[j];
        if (compareStringObject(fe->name, name) == 0)
            return fe;
    }
    return NULL;
}

/* Find or create a table entry. The context retains its name. */
tffuncentry *getOrCreateFunction(tfctx *ctx, tfobj *name) {
    tffuncentry *fe = getFunctionByName(ctx,name);
    if (fe) return fe;

    ctx->functable.func_table =
        xrealloc(ctx->functable.func_table,
                 sizeof(tffuncentry*) * (ctx->functable.func_count+1));
    fe = xmalloc(sizeof(tffuncentry));
    fe->name = name;
    retain(name);
    fe->callback = NULL;
    fe->user_func = NULL;
    ctx->functable.func_table[ctx->functable.func_count++] = fe;
    return fe;
}

/* Register a built-in callback, replacing any previous implementation. */
void registerCFunction(tfctx *ctx, char *name,
                       int (*callback) (tfctx *ctx, char *name))
{
    tfobj *oname = createStringObject(name, strlen(name));
    tffuncentry *fe = getOrCreateFunction(ctx,oname);
    if (fe->user_func) release(fe->user_func);
    fe->user_func = NULL;
    fe->callback = callback;
    release(oname);
}

/* Register a user body. The context retains it; the caller keeps its reference. */
void registerUserFunction(tfctx *ctx, const tffuncentry *function) {
    retain(function->user_func);
    tffuncentry *fe = getOrCreateFunction(ctx,function->name);
    if (fe->user_func) release(fe->user_func);
    fe->user_func = function->user_func;
    fe->callback = NULL;
}

tfctx *createContext(void) {
    tfctx *ctx = xmalloc(sizeof(*ctx));
    ctx->stack = createListObject();
    ctx->profiler = NULL;
    ctx->looplist.loops = NULL;
    ctx->looplist.count = 0;
    ctx->functable.func_table = NULL;
    ctx->functable.func_count = 0;
    registerCFunction(ctx,"+",basicMathFunctions);
    registerCFunction(ctx,"-",basicMathFunctions);
    registerCFunction(ctx,"*",basicMathFunctions);
    registerCFunction(ctx,"/",basicMathFunctions);
    registerCFunction(ctx,"%",basicMathFunctions);
    registerCFunction(ctx,"=",basicMathFunctions);
    registerCFunction(ctx,"dup",dupFunction);
    registerCFunction(ctx,"over",overFunction);
    registerCFunction(ctx,"list",listFunction);
    registerCFunction(ctx,"pick",pickFunction);
    registerCFunction(ctx,"get",getFunction);
    registerCFunction(ctx,"set",setFunction);
    registerCFunction(ctx,"len",lenFunction);
    registerCFunction(ctx,"swap",swapFunction);
    registerCFunction(ctx,"drop",dropFunction);
    registerCFunction(ctx,"emit",emitFunction);
    return ctx;
}

/* Try to resolve and call the function associated with the symbol
 * name 'word'. Return 0 if the symbol was actually bound to some
 * function and was executed, return 1 otherwise (on error). */
int callSymbol(tfctx *ctx, tfobj *word) {
    tffuncentry *fe = getFunctionByName(ctx,word);
    if (fe == NULL || (fe->callback == NULL && fe->user_func == NULL)) return TF_ERR;
    if (fe->user_func) {
        return exec(ctx,fe->user_func);
    } else {
        return fe->callback(ctx, fe->name->str.ptr);
    }
}

/* Find the matching loop without executing the body. */
int skipLoop(tfobj *prg, size_t *pos) {
    int depth = 1;
    for(size_t end = *pos+1; end < prg->list.len; end++) {
        tfobj *word = prg->list.ele[end];
        if (word->type != TFOBJ_TYPE_SYMBOL) continue;
        if (strcmp(word->str.ptr,"do") == 0) depth++;
        if (strcmp(word->str.ptr,"loop") == 0) depth--;
        if (depth == 0) {
            *pos = end;
            return TF_OK;
        }
    }
    return TF_ERR;
}

/* Consume the bounds, then enter the loop or skip it when they are equal. */
int startLoop(tfctx *ctx, tfobj *prg, size_t *pos) {
    tfobj *limit = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (limit == NULL) return TF_ERR;
    tfobj *start = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (start == NULL) {
        ctxStackPush(ctx,limit);
        return TF_ERR;
    }

    int result = TF_OK;
    if (start->i == limit->i) {
        result = skipLoop(prg,pos);
    } else {
        tflooplist *list = &ctx->looplist;
        list->loops = xrealloc(list->loops,sizeof(tfloop) * (list->count+1));
        tfloop *loop = &list->loops[list->count++];
        loop->index = start->i;
        loop->limit = limit->i;
        loop->start = *pos;
    }
    release(start);
    release(limit);
    return result;
}

/* Advance the counter, then repeat the body or discard the completed loop. */
int advanceLoop(tfctx *ctx, size_t loop_base, size_t *pos) {
    tflooplist *list = &ctx->looplist;
    if (list->count == loop_base) return TF_ERR;
    tfloop *loop = &list->loops[list->count-1];
    loop->index += loop->index < loop->limit ? 1 : -1;
    if (loop->index != loop->limit) *pos = loop->start;
    else list->count--;
    return TF_OK;
}

/* Push a snapshot: depth 1 is i, depth 2 is j, depth 3 is k. */
int pushLoopIndex(tfctx *ctx, size_t depth) {
    tflooplist *list = &ctx->looplist;
    if (list->count < depth) return TF_ERR;
    tfloop *loop = &list->loops[list->count-depth];
    ctxStackPush(ctx,createIntObject(loop->index));
    return TF_OK;
}

/* Skip to the next else or then belonging to the current if. */
int skipBranch(tfobj *prg, size_t *pos, size_t *if_depth) {
    int depth = 1;
    for(size_t end = *pos+1; end < prg->list.len; end++) {
        tfobj *word = prg->list.ele[end];
        if (word->type != TFOBJ_TYPE_SYMBOL) continue;
        char *name = word->str.ptr;
        if (strcmp(name,"if") == 0) depth++;
        if (strcmp(name,"then") == 0) depth--;
        if (depth == 0 || (depth == 1 && strcmp(name,"else") == 0)) {
            *pos = end;
            if (depth == 0) (*if_depth)--;
            return TF_OK;
        }
    }
    return TF_ERR;
}

/* Consume an integer flag: zero skips the true branch. */
int startIf(tfctx *ctx, tfobj *prg, size_t *pos, size_t *if_depth) {
    tfobj *flag = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (flag == NULL) return TF_ERR;
    int condition = flag->i;
    release(flag);
    (*if_depth)++;
    if (condition == 0) return skipBranch(prg,pos,if_depth);
    return TF_OK;
}

/* Dispatch a symbol to control flow or a registered function. */
int execSymbol(tfctx *ctx, tfobj *prg, size_t loop_base,
               size_t *if_depth, size_t *pos) {
    tfobj *word = prg->list.ele[*pos];
    char *name = word->str.ptr;
    if (strcmp(name,"do") == 0) {
        return startLoop(ctx,prg,pos);
    } else if (strcmp(name,"loop") == 0) {
        return advanceLoop(ctx,loop_base,pos);
    } else if (strcmp(name,"i") == 0 ||
               strcmp(name,"j") == 0 || strcmp(name,"k") == 0)
    {
        return pushLoopIndex(ctx,name[0]-'i'+1);
    } else if (strcmp(name,"if") == 0) {
        return startIf(ctx,prg,pos,if_depth);
    } else if (strcmp(name,"else") == 0) {
        if (*if_depth == 0) return TF_ERR;
        return skipBranch(prg,pos,if_depth);
    } else if (strcmp(name,"then") == 0) {
        if (*if_depth == 0) return TF_ERR;
        (*if_depth)--;
        return TF_OK;
    } else {
        return callSymbol(ctx,word);
    }
}

/* Execute the Toy Forth program stored into the list 'prg'. */
int exec(tfctx *ctx, tfobj *prg) {
    assert(prg->type == TFOBJ_TYPE_LIST);
    const tfprofiler *profiler = ctx->profiler;
    size_t loop_base = ctx->looplist.count;
    size_t if_depth = 0;
    int result = TF_OK;
    for(size_t pos = 0; pos < prg->list.len; pos++) {
        tfobj *word = prg->list.ele[pos];
        if (word->type == TFOBJ_TYPE_SYMBOL) {

            /* if the profiler is not NULL, start the clock */
            double start = profiler ? profiler->clock() : 0;

            /* execute the symbol */
            result = execSymbol(ctx,prg,loop_base,&if_depth,&pos);

            if (profiler) {
                double stop = profiler->clock();
                profiler->record(profiler->stats,word->str.ptr,stop-start);
            }

            if (result == TF_ERR) break;
        } else {
            ctxStackPush(ctx,word);
            retain(word);
        }
    }
    if (if_depth || ctx->looplist.count != loop_base) result = TF_ERR;
    /* A called function leaves the caller's loop records intact. */
    ctx->looplist.count = loop_base;
    if (result == TF_ERR) printf("Run time error\n");
    return result;
}

/* ======================== Basic standard library ========================== */
int basicMathFunctions(tfctx *ctx, char *name) {
    if (ctxCheckStackMinLen(ctx,2)) return TF_ERR;
    tfobj *b = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (b == NULL) return TF_ERR;
    tfobj *a = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (a == NULL) {
        ctxStackPush(ctx,b);
        return TF_ERR;
    }

    if ((name[0] == '/' || name[0] == '%') &&
        (b->i == 0 || (a->i == INT_MIN && b->i == -1)))
    {
        ctxStackPush(ctx,a);
        ctxStackPush(ctx,b);
        return TF_ERR;
    }

    int result;
    switch(name[0]) {
    case '+': result = a->i + b->i; break;
    case '-': result = a->i - b->i; break;
    case '*': result = a->i * b->i; break;
    case '/': result = a->i / b->i; break;
    case '%': result = a->i % b->i; break;
    case '=': result = a->i == b->i; break;
    default:
        ctxStackPush(ctx,a);
        ctxStackPush(ctx,b);
        return TF_ERR;
    }
    release(a);
    release(b);

    ctxStackPush(ctx,createIntObject(result));
    return TF_OK;
}

int dupFunction(tfctx *ctx, char *name) {
    (void)name;
    tfobj *o = ctxStackPop(ctx,TFOBJ_TYPE_ALL);
    if (o == NULL) return TF_ERR;
    retain(o);
    ctxStackPush(ctx,o);
    ctxStackPush(ctx,o);
    return TF_OK;
}

int overFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,2)) return TF_ERR;
    tfobj *o = ctx->stack->list.ele[ctx->stack->list.len-2];
    retain(o);
    ctxStackPush(ctx,o);
    return TF_OK;
}

/* ( value count -- list ) */
int listFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,2)) return TF_ERR;
    tfobj *count = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (count == NULL) return TF_ERR;
    if (count->i < 0) {
        ctxStackPush(ctx,count);
        return TF_ERR;
    }
    tfobj *value = ctxStackPop(ctx,TFOBJ_TYPE_ALL);
    tfobj *list = createListObject();
    for(int i = 0; i < count->i; i++) {
        retain(value);
        listPush(list,value);
    }
    release(value);
    release(count);
    ctxStackPush(ctx,list);
    return TF_OK;
}

/* Copy a stack item: depth 0 is the top, depth 1 is the next. */
int pickFunction(tfctx *ctx, char *name) {
    (void)name;
    tfobj *depth = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (depth == NULL) return TF_ERR;
    tfobj *stack = ctx->stack;
    if (depth->i < 0 || (size_t)depth->i >= stack->list.len) {
        ctxStackPush(ctx,depth);
        return TF_ERR;
    }
    tfobj *o = stack->list.ele[stack->list.len-1-depth->i];
    retain(o);
    release(depth);
    ctxStackPush(ctx,o);
    return TF_OK;
}

/* ( list index -- value ) */
int getFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,2)) return TF_ERR;
    tfobj *stack = ctx->stack;
    tfobj *index = stack->list.ele[stack->list.len-1];
    tfobj *list = stack->list.ele[stack->list.len-2];
    if (index->type != TFOBJ_TYPE_INT || list->type != TFOBJ_TYPE_LIST ||
        index->i < 0 || (size_t)index->i >= list->list.len) return TF_ERR;
    tfobj *value = list->list.ele[index->i];
    retain(value);
    stack->list.ele[stack->list.len-2] = value;
    stack->list.len--;
    release(index);
    release(list);
    return TF_OK;
}

/* ( value list index -- ) Transfer the value's reference into the list. */
int setFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,3)) return TF_ERR;
    tfobj *stack = ctx->stack;
    tfobj *index = stack->list.ele[stack->list.len-1];
    tfobj *list = stack->list.ele[stack->list.len-2];
    tfobj *value = stack->list.ele[stack->list.len-3];
    if (index->type != TFOBJ_TYPE_INT || list->type != TFOBJ_TYPE_LIST ||
        index->i < 0 || (size_t)index->i >= list->list.len) return TF_ERR;
    tfobj *old = list->list.ele[index->i];
    list->list.ele[index->i] = value;
    stack->list.len -= 3;
    release(old);
    release(index);
    release(list);
    return TF_OK;
}

/* ( list -- count ) */
int lenFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,1)) return TF_ERR;
    tfobj *stack = ctx->stack;
    tfobj *list = stack->list.ele[stack->list.len-1];
    if (list->type != TFOBJ_TYPE_LIST || list->list.len > INT_MAX)
        return TF_ERR;
    stack->list.ele[stack->list.len-1] = createIntObject((int)list->list.len);
    release(list);
    return TF_OK;
}

int swapFunction(tfctx *ctx, char *name) {
    (void)name;
    if (ctxCheckStackMinLen(ctx,2)) return TF_ERR;
    tfobj *stack = ctx->stack;
    tfobj *o = stack->list.ele[stack->list.len-1];
    stack->list.ele[stack->list.len-1] = stack->list.ele[stack->list.len-2];
    stack->list.ele[stack->list.len-2] = o;
    return TF_OK;
}

int dropFunction(tfctx *ctx, char *name) {
    (void)name;
    tfobj *o = ctxStackPop(ctx,TFOBJ_TYPE_ALL);
    if (o == NULL) return TF_ERR;
    release(o);
    return TF_OK;
}

int emitFunction(tfctx *ctx, char *name) {
    (void)name;
    tfobj *o = ctxStackPop(ctx,TFOBJ_TYPE_INT);
    if (o == NULL) return TF_ERR;
    if (o->i < 0 || o->i > UCHAR_MAX) {
        ctxStackPush(ctx,o);
        return TF_ERR;
    }
    int result = putchar(o->i) == EOF ? TF_ERR : TF_OK;
    release(o);
    return result;
}

/* ============================== Profiling ================================ */

/* POSIX monotonic elapsed time includes waiting. Timer resolution and
 * profiling overhead limit measurements of short symbols. */
double timeMilliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now) != 0) {
        perror("Reading profiling clock");
        exit(1);
    }
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

static tfprofileentry *findProfileEntry(tfcallstats *stats, const char *name) {
    for(size_t i = 0; i < stats->count; i++) {
        tfprofileentry *entry = &stats->entries[i];
        if (strcmp(entry->name,name) == 0) return entry;
    }
    return NULL;
}

static void collectProfileSymbols(tfcallstats *stats, const tfobj *prg) {
    for(size_t i = 0; i < prg->list.len; i++) {
        const tfobj *symbol = prg->list.ele[i];
        if (symbol->type != TFOBJ_TYPE_SYMBOL || findProfileEntry(stats,symbol->str.ptr)) continue;
        stats->entries = xrealloc(stats->entries,sizeof(*stats->entries) * (stats->count+1));
        stats->entries[stats->count++] = (tfprofileentry){.name = symbol->str.ptr};
    }
}

int initCallStats(tfcallstats *stats, const tfctx *ctx, const tfobj *prg) {
    collectProfileSymbols(stats,prg);
    /* Compilation stores user-defined bodies in the function table. */
    for(size_t i = 0; i < ctx->functable.func_count; i++) {
        const tfobj *user_func = ctx->functable.func_table[i]->user_func;
        if (user_func) collectProfileSymbols(stats,user_func);
    }
    return stats->count;
}

void destroyCallStats(tfcallstats *stats) {
    free(stats->entries);
    stats->entries = NULL;
    stats->count = 0;
}

void profileSymbol(tfcallstats *stats, const char *name, double elapsed_ms) {
    tfprofileentry *entry = findProfileEntry(stats,name);
    if (entry == NULL) return;
    entry->calls++;
    entry->total_ms += elapsed_ms;
}

void printProfile(const tfcallstats *stats) {
    fprintf(stderr,"%-16s %10s %14s %13s\n",
            "WORD","CALLS","TOTAL (ms)","AVG (µs)");

    for(size_t j = 0; j < stats->count; j++) {
        const tfprofileentry *entry = &stats->entries[j];
        if (entry->calls == 0) continue;
        fprintf(stderr,"%-16s %10zu %14.3f %12.3f\n",
                entry->name,entry->calls,entry->total_ms,
                entry->total_ms * 1000 / entry->calls);
    }
}

/* ================================== Main ================================== */

int main(int argc, char **argv) {
    int profile = argc > 1 &&
        (strcmp(argv[1],"-p") == 0 || strcmp(argv[1],"--profile") == 0);
    if (argc != 2 + profile) {
        fprintf(stderr,"Usage: %s [-p | --profile] <filename>\n", argv[0]);
        return 1;
    }

    /* Read the program in memory, for later parsing. */
    FILE *fp = fopen(argv[1+profile],"r");
    if (fp == NULL) {
        perror("Opening Toy Forth program");
        return 1;
    }

    if (fseek(fp,0,SEEK_END) != 0) {
        perror("Seeking Toy Forth program");
        fclose(fp);
        return 1;
    }
    long file_size = ftell(fp);
    if (file_size == -1) {
        perror("Getting Toy Forth program size");
        fclose(fp);
        return 1;
    }
    if (fseek(fp,0,SEEK_SET) != 0) {
        perror("Seeking Toy Forth program");
        fclose(fp);
        return 1;
    }
    char *prgtext = xmalloc(file_size+1);
    if (fread(prgtext,1,file_size,fp) != (size_t)file_size) {
        fprintf(stderr,"Error reading Toy Forth program\n");
        free(prgtext);
        fclose(fp);
        return 1;
    }
    prgtext[file_size] = 0;
    fclose(fp);

    tfctx *ctx = createContext();
    tfobj *prg = compile(ctx,prgtext);
    free(prgtext);
    if (prg == NULL) return 1;
    printObject(prg);
    printf("\n");

    tfcallstats stats = {NULL, 0};
    tfprofiler profiler = {&stats,timeMilliseconds,profileSymbol};
    if (profile) {
        initCallStats(&stats,ctx,prg);
        ctx->profiler = &profiler;
    }
    exec(ctx,prg);

    printf("Stack content at end: ");
    printObject(ctx->stack);
    printf("\n");

    if (profile) {
        printProfile(&stats);
        destroyCallStats(&stats);
    }
    return 0;
}
