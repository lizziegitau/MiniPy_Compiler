/*  icg.c  —  MiniPy Intermediate Code Generator (Phase 3)
 *
 *  Strategy: recursive tree-walk over the ParseNode tree produced by the
 *  parser.  Each icg_*() function matches on node->label and emits the
 *  corresponding Three-Address Code (TAC) quadruples.
 *
 *  Members:
 *    WP-01  Expressions          → icg_factor / icg_term / icg_expr        (Hodhan)
 *    WP-02  Comparisons & Logic  → icg_comparison / icg_logical_expr       (Salma)
 *    WP-03  Control Flow         → icg_if / icg_while / icg_for            (Elizabeth)
 *    WP-04  Assign, Print, Output→ icg_assign / icg_print / icg_print_quads (Iman)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icg.h"
#include "parser.h"

/* ═══════════════════════════════════════════════════════════════════════════
   §0  GLOBAL STATE
   ═══════════════════════════════════════════════════════════════════════════ */

Quad quads[MAX_QUADS];
int quad_count = 0;

static int temp_count = 0;  /* next temporary index:  t0, t1, t2, …  */
static int label_count = 0; /* next label index:      L0, L1, L2, …  */

/* ═══════════════════════════════════════════════════════════════════════════
   §1  HELPERS  (shared by all four modules)
   ═══════════════════════════════════════════════════════════════════════════ */

/* Allocate a fresh temporary name like "t3" into `buf` (caller supplies).  */
static void new_temp(char *buf, int bufsz)
{
    snprintf(buf, bufsz, "t%d", temp_count++);
}

/* Allocate a fresh label name like "L5" into `buf`.                        */
static void new_label(char *buf, int bufsz)
{
    snprintf(buf, bufsz, "L%d", label_count++);
}

/* Append one quadruple to the global array.                                */
static void emit(const char *op,
                 const char *arg1,
                 const char *arg2,
                 const char *result)
{
    if (quad_count >= MAX_QUADS)
    {
        fprintf(stderr, "ICG Error: quad buffer overflow\n");
        return;
    }
    Quad *q = &quads[quad_count++];
    strncpy(q->op, op ? op : "", MAX_OP - 1);
    strncpy(q->arg1, arg1 ? arg1 : "", MAX_OPERAND - 1);
    strncpy(q->arg2, arg2 ? arg2 : "", MAX_OPERAND - 1);
    strncpy(q->result, result ? result : "", MAX_OPERAND - 1);
}

/* ── label node helpers ──────────────────────────────────────────────────── */

/* Emit a label definition:  LABEL  ""  ""  L3                              */
static void emit_label(const char *lbl)
{
    emit("LABEL", "", "", lbl);
}

/* Emit an unconditional jump:  GOTO  ""  ""  L3                            */
static void emit_goto(const char *lbl)
{
    emit("GOTO", "", "", lbl);
}

/* Emit a conditional jump:  IF_FALSE_GOTO  cond  ""  L3                   */
static void emit_if_false(const char *cond, const char *lbl)
{
    emit("IF_FALSE_GOTO", cond, "", lbl);
}

/* ── child accessor ──────────────────────────────────────────────────────── */

/* Return the i-th child of node, or NULL if out of range.                  */
static ParseNode *child(ParseNode *node, int i)
{
    if (!node || i < 0 || i >= node->child_count)
        return NULL;
    return node->children[i];
}

/* Return 1 if node->label starts with `prefix`.                            */
static int label_is(ParseNode *node, const char *prefix)
{
    if (!node)
        return 0;
    return strncmp(node->label, prefix, strlen(prefix)) == 0;
}

/* Extract the raw lexeme from a terminal node whose label looks like:
   KEYWORD_IF("if")  →  returns "if"
   INTEGER("85")     →  returns "85"
   IDENTIFIER("x")  →  returns "x"                                         */
static void extract_lexeme(ParseNode *node, char *buf, int bufsz)
{
    buf[0] = '\0';
    if (!node)
        return;
    const char *open = strchr(node->label, '"');
    const char *close = strrchr(node->label, '"');
    if (!open || !close || open == close)
        return;
    open++; /* skip the opening quote */
    int len = (int)(close - open);
    if (len <= 0)
        return;
    if (len >= bufsz)
        len = bufsz - 1;
    strncpy(buf, open, len);
    buf[len] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
   §2  FORWARD DECLARATIONS
   ═══════════════════════════════════════════════════════════════════════════ */

static void icg_stmt_list(ParseNode *node);
static void icg_stmt(ParseNode *node);
static void icg_assign_stmt(ParseNode *node);
static void icg_print_stmt(ParseNode *node);
static void icg_if_stmt(ParseNode *node);
static void icg_while_stmt(ParseNode *node);
static void icg_for_stmt(ParseNode *node);
static void icg_block(ParseNode *node);

static void icg_logical_expr(ParseNode *node, char *out, int outsz);
static void icg_logical_expr_prime(ParseNode *node, char *lhs, int lhssz);
static void icg_comparison(ParseNode *node, char *out, int outsz);
static void icg_comparison_prime(ParseNode *node, char *lhs, int lhssz);
static void icg_not_expr(ParseNode *node, char *out, int outsz);
static void icg_expr(ParseNode *node, char *out, int outsz);
static void icg_expr_prime(ParseNode *node, char *lhs, int lhssz);
static void icg_term(ParseNode *node, char *out, int outsz);
static void icg_term_prime(ParseNode *node, char *lhs, int lhssz);
static void icg_factor(ParseNode *node, char *out, int outsz);

/* ═══════════════════════════════════════════════════════════════════════════
   §3  WP-01  EXPRESSION TAC  (Hodhan)
   ───────────────────────────────────────────────────────────────────────────
   Grammar rules handled:
     expr       → term expr'
     expr'      → + term expr'  |  - term expr'  |  ε
     term       → factor term'
     term'      → * factor term'  |  / factor term'  |  ε
     factor     → INTEGER | FLOAT | STRING | IDENTIFIER | TRUE | FALSE
                | ( logical_expr )  |  - factor
   ═══════════════════════════════════════════════════════════════════════════ */

/*  icg_factor: leaf-level.  Returns the operand name in `out`.
    Literals and identifiers are returned as-is (no code emitted).
    Unary minus emits:  NEG  arg  ""  tN                                    */
static void icg_factor(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    /* factor has exactly one child (the matched terminal or a sub-expr).   */
    ParseNode *c0 = child(node, 0);
    if (!c0)
        return;

    /* ── unary minus: children are OPERATOR_MINUS and factor ────────────── */
    if (label_is(c0, "OPERATOR_MINUS"))
    {
        char inner[MAX_OPERAND];
        icg_factor(child(node, 1), inner, sizeof(inner));
        new_temp(out, outsz);
        emit("NEG", inner, "", out);
        return;
    }

    /* ── parenthesised expression: children are '(' logical_expr ')' ────── */
    if (label_is(c0, "PUNCTUATOR_LPAREN"))
    {
        icg_logical_expr(child(node, 1), out, outsz);
        return;
    }

    /* ── literal or identifier terminal: just extract the lexeme ─────────── */
    extract_lexeme(c0, out, outsz);
}

/*  icg_term_prime: handles the right-recursive tail  * factor term'  etc.
    `lhs` is the accumulated result so far; updated in-place.               */
static void icg_term_prime(ParseNode *node, char *lhs, int lhssz)
{
    if (!node || node->child_count == 0)
        return; /* ε production */

    /* children: [0] operator  [1] factor  [2] term'                        */
    ParseNode *op_node = child(node, 0);
    ParseNode *factor_node = child(node, 1);
    ParseNode *rest_node = child(node, 2);

    char op_str[16];
    extract_lexeme(op_node, op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_factor(factor_node, rhs, sizeof(rhs));

    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));

    if (strcmp(op_str, "*") == 0)
        emit("MUL", lhs, rhs, temp);
    else
        emit("DIV", lhs, rhs, temp);

    strncpy(lhs, temp, lhssz - 1);
    icg_term_prime(rest_node, lhs, lhssz);
}

/*  icg_term: term → factor term'                                           */
static void icg_term(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    char lhs[MAX_OPERAND];
    icg_factor(child(node, 0), lhs, sizeof(lhs));
    icg_term_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/*  icg_expr_prime: handles the right-recursive tail  + term expr'  etc.   */
static void icg_expr_prime(ParseNode *node, char *lhs, int lhssz)
{
    if (!node || node->child_count == 0)
        return; /* ε */

    ParseNode *op_node = child(node, 0);
    ParseNode *term_node = child(node, 1);
    ParseNode *rest_node = child(node, 2);

    char op_str[16];
    extract_lexeme(op_node, op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_term(term_node, rhs, sizeof(rhs));

    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));

    if (strcmp(op_str, "+") == 0)
        emit("ADD", lhs, rhs, temp);
    else
        emit("SUB", lhs, rhs, temp);

    strncpy(lhs, temp, lhssz - 1);
    icg_expr_prime(rest_node, lhs, lhssz);
}

/*  icg_expr: expr → term expr'                                             */
static void icg_expr(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    char lhs[MAX_OPERAND];
    icg_term(child(node, 0), lhs, sizeof(lhs));
    icg_expr_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
   §4  WP-02  COMPARISON & LOGICAL TAC  (Salma)
   ───────────────────────────────────────────────────────────────────────────
   Grammar rules handled:
     logical_expr  → comparison logical_expr'
     logical_expr' → AND comparison logical_expr'
                   | OR  comparison logical_expr'  |  ε
     comparison    → not_expr comparison'
     comparison'   → (== | != | < | > | <= | >=) not_expr  |  ε
     not_expr      → NOT not_expr  |  expr
   ═══════════════════════════════════════════════════════════════════════════ */

/*  Map a MiniPy comparison lexeme to a TAC operator string.               */
static const char *cmp_op(const char *lexeme)
{
    if (strcmp(lexeme, "==") == 0)
        return "EQ";
    if (strcmp(lexeme, "!=") == 0)
        return "NEQ";
    if (strcmp(lexeme, "<") == 0)
        return "LT";
    if (strcmp(lexeme, ">") == 0)
        return "GT";
    if (strcmp(lexeme, "<=") == 0)
        return "LEQ";
    if (strcmp(lexeme, ">=") == 0)
        return "GEQ";
    return "CMP";
}

/*  icg_comparison_prime: (op not_expr comparison') | ε
    Updates `lhs` with the comparison result.                               */
static void icg_comparison_prime(ParseNode *node, char *lhs, int lhssz)
{
    if (!node || node->child_count == 0)
        return; /* ε */

    /* children: [0] operator  [1] not_expr  [2] comparison'               */
    char op_str[16];
    extract_lexeme(child(node, 0), op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_not_expr(child(node, 1), rhs, sizeof(rhs));

    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));
    emit(cmp_op(op_str), lhs, rhs, temp);

    strncpy(lhs, temp, lhssz - 1);
    icg_comparison_prime(child(node, 2), lhs, lhssz);
}

/*  icg_comparison: comparison → not_expr comparison'                      */
static void icg_comparison(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    char lhs[MAX_OPERAND];
    icg_not_expr(child(node, 0), lhs, sizeof(lhs));
    icg_comparison_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/*  icg_logical_expr_prime: short-circuit AND / OR.
    AND semantics:  if lhs is false, skip rhs  → result is false
    OR  semantics:  if lhs is true,  skip rhs  → result is true           */
static void icg_logical_expr_prime(ParseNode *node, char *lhs, int lhssz)
{
    if (!node || node->child_count == 0)
        return; /* ε */

    /* children: [0] AND/OR keyword  [1] comparison  [2] logical_expr'     */
    char kw[16];
    extract_lexeme(child(node, 0), kw, sizeof(kw));
    char rhs[MAX_OPERAND];
    char temp[MAX_OPERAND];
    char skip[MAX_OPERAND];
    new_label(skip, sizeof(skip));
    new_temp(temp, sizeof(temp));

    if (strcmp(kw, "and") == 0)
    {
        /* Short-circuit AND:
             IF_FALSE_GOTO lhs → skip
             rhs = eval(comparison)
             temp = lhs AND rhs
           skip:
             temp = lhs  (already false)                                    */
        emit_if_false(lhs, skip);
        icg_comparison(child(node, 1), rhs, sizeof(rhs));
        emit("AND", lhs, rhs, temp);
        char done[MAX_OPERAND];
        new_label(done, sizeof(done));
        emit_goto(done);
        emit_label(skip);
        emit("ASSIGN", lhs, "", temp);
        emit_label(done);
    }
    else
    {
        /* Short-circuit OR:
             if lhs is already true → skip evaluation of rhs
             temp = lhs OR rhs                                              */
        char true_lbl[MAX_OPERAND];
        new_label(true_lbl, sizeof(true_lbl));
        /* if lhs is true, jump to done with lhs as result */
        emit("IF_TRUE_GOTO", lhs, "", true_lbl);
        icg_comparison(child(node, 1), rhs, sizeof(rhs));
        emit("OR", lhs, rhs, temp);
        char done[MAX_OPERAND];
        new_label(done, sizeof(done));
        emit_goto(done);
        emit_label(true_lbl);
        emit("ASSIGN", lhs, "", temp);
        emit_label(done);
    }

    strncpy(lhs, temp, lhssz - 1);
    icg_logical_expr_prime(child(node, 2), lhs, lhssz);
}

/*  icg_logical_expr: logical_expr → comparison logical_expr'             */
static void icg_logical_expr(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    char lhs[MAX_OPERAND];
    icg_comparison(child(node, 0), lhs, sizeof(lhs));
    icg_logical_expr_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/*  icg_not_expr: NOT not_expr  |  expr                                    */
static void icg_not_expr(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    ParseNode *c0 = child(node, 0);
    if (!c0)
        return;

    if (label_is(c0, "KEYWORD_NOT"))
    {
        char inner[MAX_OPERAND];
        icg_not_expr(child(node, 1), inner, sizeof(inner));
        new_temp(out, outsz);
        emit("NOT", inner, "", out);
    }
    else
    {
        icg_expr(c0, out, outsz);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   §5  WP-03  CONTROL-FLOW TAC  (Elizabeth)
   ───────────────────────────────────────────────────────────────────────────
   Grammar rules handled:
     if_stmt    → IF logical_expr : block else_clause
     else_clause→ ELSE : block  |  ε
     while_stmt → WHILE logical_expr : block
     for_stmt   → FOR IDENTIFIER IN RANGE ( expr ) : block
   ═══════════════════════════════════════════════════════════════════════════ */

/*  icg_block: walk the block node's children (stmt + stmt_list).          */
static void icg_block(ParseNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
    {
        ParseNode *c = node->children[i];
        if (label_is(c, "stmt"))
            icg_stmt(c);
        if (label_is(c, "stmt_list"))
            icg_stmt_list(c);
    }
}

/*  icg_if_stmt:
    Pattern (with else):
        condition = eval(logical_expr)
        IF_FALSE_GOTO condition → L_else
        [then block]
        GOTO L_end
      L_else:
        [else block]
      L_end:

    Pattern (without else):
        condition = eval(logical_expr)
        IF_FALSE_GOTO condition → L_end
        [then block]
      L_end:                                                                 */
static void icg_if_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* node children (from parser):
       [0] KEYWORD_IF  [1] logical_expr  [2] COLON  [3] block  [4] else_clause */
    char cond[MAX_OPERAND];
    icg_logical_expr(child(node, 1), cond, sizeof(cond));

    /* check whether the else_clause is non-empty */
    ParseNode *else_node = child(node, 4);
    int has_else = (else_node && else_node->child_count > 0);

    char l_else[MAX_OPERAND], l_end[MAX_OPERAND];
    new_label(l_else, sizeof(l_else));
    new_label(l_end, sizeof(l_end));

    if (has_else)
    {
        emit_if_false(cond, l_else);
        icg_block(child(node, 3)); /* then block */
        emit_goto(l_end);
        emit_label(l_else);
        /* else_clause children: [0] ELSE  [1] COLON  [2] block */
        icg_block(child(else_node, 2)); /* else block */
        emit_label(l_end);
    }
    else
    {
        emit_if_false(cond, l_end);
        icg_block(child(node, 3)); /* then block */
        emit_label(l_end);
    }
}

/*  icg_while_stmt:
      L_start:
        condition = eval(logical_expr)
        IF_FALSE_GOTO condition → L_end
        [body]
        GOTO L_start
      L_end:                                                                 */
static void icg_while_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* node children: [0] WHILE  [1] logical_expr  [2] COLON  [3] block    */
    char l_start[MAX_OPERAND], l_end[MAX_OPERAND];
    new_label(l_start, sizeof(l_start));
    new_label(l_end, sizeof(l_end));

    emit_label(l_start);

    char cond[MAX_OPERAND];
    icg_logical_expr(child(node, 1), cond, sizeof(cond));

    emit_if_false(cond, l_end);
    icg_block(child(node, 3));
    emit_goto(l_start);
    emit_label(l_end);
}

/*  icg_for_stmt:
    for IDENTIFIER in range(expr):  →  a C-style counted loop:

        var = 0
        t_limit = eval(expr)
      L_start:
        t_cmp = var < t_limit
        IF_FALSE_GOTO t_cmp → L_end
        [body]
        t_inc = var + 1
        var = t_inc
        GOTO L_start
      L_end:                                                                 */
static void icg_for_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* node children (from parser):
       [0] FOR  [1] IDENTIFIER  [2] IN  [3] RANGE
       [4] LPAREN  [5] expr  [6] RPAREN  [7] COLON  [8] block             */
    char var[MAX_OPERAND];
    extract_lexeme(child(node, 1), var, sizeof(var));

    /* initialise loop variable to 0 */
    emit("ASSIGN", "0", "", var);

    /* evaluate the range limit */
    char t_limit[MAX_OPERAND];
    icg_expr(child(node, 5), t_limit, sizeof(t_limit));

    char l_start[MAX_OPERAND], l_end[MAX_OPERAND];
    new_label(l_start, sizeof(l_start));
    new_label(l_end, sizeof(l_end));

    emit_label(l_start);

    /* condition: var < limit */
    char t_cmp[MAX_OPERAND];
    new_temp(t_cmp, sizeof(t_cmp));
    emit("LT", var, t_limit, t_cmp);

    emit_if_false(t_cmp, l_end);
    icg_block(child(node, 8));

    /* increment */
    char t_inc[MAX_OPERAND];
    new_temp(t_inc, sizeof(t_inc));
    emit("ADD", var, "1", t_inc);
    emit("ASSIGN", t_inc, "", var);

    emit_goto(l_start);
    emit_label(l_end);
}

/* ═══════════════════════════════════════════════════════════════════════════
   §6  WP-04  ASSIGN, PRINT & OUTPUT  (Iman)
   ───────────────────────────────────────────────────────────────────────────
   Grammar rules handled:
     assign_stmt → IDENTIFIER = logical_expr
     print_stmt  → PRINT ( logical_expr )
   ═══════════════════════════════════════════════════════════════════════════ */

/*  icg_assign_stmt:
    Evaluate the RHS expression, then emit:
        ASSIGN  rhs_temp  ""  variable_name                                 */
static void icg_assign_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* node children: [0] IDENTIFIER  [1] ASSIGN_OP  [2] logical_expr      */
    char var_name[MAX_OPERAND];
    extract_lexeme(child(node, 0), var_name, sizeof(var_name));

    char rhs[MAX_OPERAND];
    icg_logical_expr(child(node, 2), rhs, sizeof(rhs));

    emit("ASSIGN", rhs, "", var_name);
}

/*  icg_print_stmt:
    Evaluate the argument, then emit:
        PRINT  arg  ""  ""                                                  */
static void icg_print_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* node children: [0] PRINT  [1] LPAREN  [2] logical_expr  [3] RPAREN  */
    char arg[MAX_OPERAND];
    icg_logical_expr(child(node, 2), arg, sizeof(arg));

    emit("PRINT", arg, "", "");
}

/*  icg_print_quads: format and print the full TAC listing to stdout.      */
void icg_print(void)
{
    printf("=== INTERMEDIATE CODE (Three-Address Code) ===\n");
    printf("%-5s  %-16s  %-20s  %-20s  %s\n",
           "Line", "Operation", "Arg1", "Arg2", "Result");
    printf("----------------------------------------------------------------------\n");

    for (int i = 0; i < quad_count; i++)
    {
        Quad *q = &quads[i];

        /* Special pretty-print for label definitions */
        if (strcmp(q->op, "LABEL") == 0)
        {
            printf("[%03d]  %s:\n", i, q->result);
            continue;
        }

        /* Special pretty-print for unconditional goto */
        if (strcmp(q->op, "GOTO") == 0)
        {
            printf("[%03d]  %-16s  goto %s\n", i, "", q->result);
            continue;
        }

        /* Special pretty-print for conditional jump */
        if (strcmp(q->op, "IF_FALSE_GOTO") == 0)
        {
            printf("[%03d]  if NOT %-13s  goto %s\n", i, q->arg1, q->result);
            continue;
        }
        if (strcmp(q->op, "IF_TRUE_GOTO") == 0)
        {
            printf("[%03d]  if %-16s  goto %s\n", i, q->arg1, q->result);
            continue;
        }

        /* Special pretty-print for PRINT */
        if (strcmp(q->op, "PRINT") == 0)
        {
            printf("[%03d]  print %s\n", i, q->arg1);
            continue;
        }

        /* Special pretty-print for ASSIGN */
        if (strcmp(q->op, "ASSIGN") == 0)
        {
            printf("[%03d]  %-20s  =  %s\n", i, q->result, q->arg1);
            continue;
        }

        /* General binary / unary instruction */
        if (q->arg2[0] != '\0')
        {
            /* binary: result = arg1 OP arg2 */
            printf("[%03d]  %-20s  =  %s  %s  %s\n",
                   i, q->result, q->arg1, q->op, q->arg2);
        }
        else
        {
            /* unary: result = OP arg1 */
            printf("[%03d]  %-20s  =  %s %s\n",
                   i, q->result, q->op, q->arg1);
        }
    }

    printf("----------------------------------------------------------------------\n");
    printf("Total instructions: %d\n\n", quad_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
   §7  TOP-LEVEL TREE WALKERS
   ═══════════════════════════════════════════════════════════════════════════ */

static void icg_stmt(ParseNode *node)
{
    if (!node || node->child_count == 0)
        return;
    ParseNode *inner = child(node, 0);
    if (!inner)
        return;

    if (label_is(inner, "assign_stmt"))
        icg_assign_stmt(inner);
    else if (label_is(inner, "print_stmt"))
        icg_print_stmt(inner);
    else if (label_is(inner, "if_stmt"))
        icg_if_stmt(inner);
    else if (label_is(inner, "while_stmt"))
        icg_while_stmt(inner);
    else if (label_is(inner, "for_stmt"))
        icg_for_stmt(inner);
}

static void icg_stmt_list(ParseNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
    {
        ParseNode *c = node->children[i];
        if (label_is(c, "stmt"))
            icg_stmt(c);
        if (label_is(c, "stmt_list"))
            icg_stmt_list(c);
    }
}

/* ── Public entry point ──────────────────────────────────────────────────── */
void icg_generate(ParseNode *root)
{
    if (!root)
        return;
    /* root->label == "program", its first child is stmt_list */
    ParseNode *stmt_list = child(root, 0);
    icg_stmt_list(stmt_list);
    icg_print();
}