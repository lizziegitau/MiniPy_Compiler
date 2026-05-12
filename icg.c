#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icg.h"
#include "parser.h"

/* Global array to store the generated quadruples and their count */
Quad quads[MAX_QUADS];
int quad_count = 0;

static int temp_count = 0;  /* index for generating temporary variables */
static int label_count = 0; /* index for generating unique labels */

/* Function that generates a new temporary variable name and stores it in `buf` */
static void new_temp(char *buf, int bufsz)
{
    snprintf(buf, bufsz, "t%d", temp_count++);
}

/* Function that generates a new label name and stores it in `buf` */
static void new_label(char *buf, int bufsz)
{
    snprintf(buf, bufsz, "L%d", label_count++);
}

/* Function that appends one quadruple to the global array */
static void emit(const char *op,
                 const char *arg1,
                 const char *arg2,
                 const char *result)
{
    /* Check if we have space for another quadruple in the buffer */
    if (quad_count >= MAX_QUADS)
    {
        fprintf(stderr, "ICG Error: quad buffer overflow\n");
        return;
    }
    /* Append the quadruple to the array if we have space in the buffer */
    Quad *q = &quads[quad_count++];
    strncpy(q->op, op ? op : "", MAX_OP - 1);                  /* Append operator */
    strncpy(q->arg1, arg1 ? arg1 : "", MAX_OPERAND - 1);       /* Append left operand */
    strncpy(q->arg2, arg2 ? arg2 : "", MAX_OPERAND - 1);       /* Append right operand */
    strncpy(q->result, result ? result : "", MAX_OPERAND - 1); /* Append result */
}

/* Function that emits a label definition */
static void emit_label(const char *lbl)
{
    emit("LABEL", "", "", lbl);
}

/* Function that emits an unconditional jump to skip over an else block after a then */
static void emit_goto(const char *lbl)
{
    emit("GOTO", "", "", lbl);
}

/* Function that emits a conditional jump if the condition is false */
static void emit_if_false(const char *cond, const char *lbl)
{
    emit("IF_FALSE_GOTO", cond, "", lbl);
}

/* Function that returns the i-th child of a parse node, or NULL if out of range */
static ParseNode *child(ParseNode *node, int i)
{
    if (!node || i < 0 || i >= node->child_count)
        return NULL;
    return node->children[i];
}

/* Function that checks if a node's label starts with a given prefix */
static int label_is(ParseNode *node, const char *prefix)
{
    if (!node)
        return 0;
    return strncmp(node->label, prefix, strlen(prefix)) == 0;
}

/* Function that extracts the lexeme from a terminal node by removing the surrounding quotes and null-terminating it to the buffer so that it can be used as an operand */
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

/* Function declarations for ICG functions */
/* Function to handle stmt_list */
static void icg_stmt_list(ParseNode *node);
/* Function to handle stmt */
static void icg_stmt(ParseNode *node);
/* Function to handle assignment statements */
static void icg_assign_stmt(ParseNode *node);
/* Function to handle print statements */
static void icg_print_stmt(ParseNode *node);
/* Function to handle if statements */
static void icg_if_stmt(ParseNode *node);
/* Function to handle while statements */
static void icg_while_stmt(ParseNode *node);
/* Function to handle for statements */
static void icg_for_stmt(ParseNode *node);
/* Function to handle the body of if, for and while statements */
static void icg_block(ParseNode *node);
/* Function to handle logical expressions */
static void icg_logical_expr(ParseNode *node, char *out, int outsz);
/* Function to handle the right-recursive tail of logical expressions */
static void icg_logical_expr_prime(ParseNode *node, char *lhs, int lhssz);
/* Function to handle comparison expressions */
static void icg_comparison(ParseNode *node, char *out, int outsz);
/* Function to handle the right-recursive tail of comparison expressions */
static void icg_comparison_prime(ParseNode *node, char *lhs, int lhssz);
/* Function to handle not expressions */
static void icg_not_expr(ParseNode *node, char *out, int outsz);
/* Function to handle expressions */
static void icg_expr(ParseNode *node, char *out, int outsz);
/* Function to handle the right-recursive tail of expressions */
static void icg_expr_prime(ParseNode *node, char *lhs, int lhssz);
/* Function to handle term */
static void icg_term(ParseNode *node, char *out, int outsz);
/* Function to handle the right-recursive tail of term */
static void icg_term_prime(ParseNode *node, char *lhs, int lhssz);
/* Function to handle factor */
static void icg_factor(ParseNode *node, char *out, int outsz);

/* Function to handle factor */
static void icg_factor(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    /* Handle the first child */
    ParseNode *c0 = child(node, 0);
    if (!c0)
        return;

    /* If the first child is a unary minus operator, create a new temporary and emit a NEG instruction */
    if (label_is(c0, "OPERATOR_MINUS"))
    {
        char inner[MAX_OPERAND];
        icg_factor(child(node, 1), inner, sizeof(inner));
        new_temp(out, outsz);
        emit("NEG", inner, "", out);
        return;
    }

    /* If the first child is a left parenthesis, handle the parenthesised expression */
    if (label_is(c0, "PUNCTUATOR_LPAREN"))
    {
        icg_logical_expr(child(node, 1), out, outsz);
        return;
    }

    /* If the first child is a literal or identifier, extract the lexeme */
    extract_lexeme(c0, out, outsz);
}

/* Function to handle the right-recursive tail of term */
static void icg_term_prime(ParseNode *node, char *lhs, int lhssz)
{
    /* Handles an epsilon production */
    if (!node || node->child_count == 0)
        return;

    /* Handle the operator, factor and recursive tail */
    ParseNode *op_node = child(node, 0);
    ParseNode *factor_node = child(node, 1);
    ParseNode *rest_node = child(node, 2);

    /* Extract the operator lexeme and right-hand side */
    char op_str[16];
    extract_lexeme(op_node, op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_factor(factor_node, rhs, sizeof(rhs));

    /* Create a new temporary for the result */
    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));

    /* Emit the appropriate TAC instruction, either MUL or DIV */
    if (strcmp(op_str, "*") == 0)
        emit("MUL", lhs, rhs, temp);
    else
        emit("DIV", lhs, rhs, temp);

    /* Update the left-hand side with the result */
    strncpy(lhs, temp, lhssz - 1);
    icg_term_prime(rest_node, lhs, lhssz);
}

/* Function to handle term */
static void icg_term(ParseNode *node, char *out, int outsz)
{
    /* Initialize the output string */
    out[0] = '\0';
    if (!node)
        return;

    /* Handle the first child and copy its value to the output */
    char lhs[MAX_OPERAND];
    icg_factor(child(node, 0), lhs, sizeof(lhs));
    icg_term_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/* Function to handle the right-recursive tail of expression */
static void icg_expr_prime(ParseNode *node, char *lhs, int lhssz)
{
    /* Handles an epsilon production */
    if (!node || node->child_count == 0)
        return; /* ε */

    /* Handle the operator, term and recursive tail */
    ParseNode *op_node = child(node, 0);
    ParseNode *term_node = child(node, 1);
    ParseNode *rest_node = child(node, 2);

    /* Extract the operator lexeme and right-hand side */
    char op_str[16];
    extract_lexeme(op_node, op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_term(term_node, rhs, sizeof(rhs));

    /* Create a new temporary for the result */
    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));

    /* Emit the appropriate TAC instruction, either ADD or SUB */
    if (strcmp(op_str, "+") == 0)
        emit("ADD", lhs, rhs, temp);
    else
        emit("SUB", lhs, rhs, temp);

    /* Update the left-hand side with the result */
    strncpy(lhs, temp, lhssz - 1);
    icg_expr_prime(rest_node, lhs, lhssz);
}

/* Function to handle expression */
static void icg_expr(ParseNode *node, char *out, int outsz)
{
    /* Initialize the output string */
    out[0] = '\0';
    if (!node)
        return;

    /* Handle the first child and copy its value to the output */
    char lhs[MAX_OPERAND];
    icg_term(child(node, 0), lhs, sizeof(lhs));
    icg_expr_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/*  Map a MiniPy comparison lexeme to a TAC operator string */
static const char *cmp_op(const char *lexeme)
{
    /* For when the lexeme matches an equals to operator */
    if (strcmp(lexeme, "==") == 0)
        return "EQ";
    /* For when the lexeme matches a not-equals operator */
    if (strcmp(lexeme, "!=") == 0)
        return "NEQ";
    /* For when the lexeme matches a less-than operator */
    if (strcmp(lexeme, "<") == 0)
        return "LT";
    /* For when the lexeme matches a greater-than operator */
    if (strcmp(lexeme, ">") == 0)
        return "GT";
    /* For when the lexeme matches a less-than-or-equals operator */
    if (strcmp(lexeme, "<=") == 0)
        return "LEQ";
    /* For when the lexeme matches a greater-than-or-equals operator */
    if (strcmp(lexeme, ">=") == 0)
        return "GEQ";
    /* Default case */
    return "CMP";
}

/* Function to handle the right-recursive tail of comparison */
static void icg_comparison_prime(ParseNode *node, char *lhs, int lhssz)
{
    /* Handles an epsilon production */
    if (!node || node->child_count == 0)
        return; /* ε */

    /* Extract the operator and right-hand side */
    char op_str[16];
    extract_lexeme(child(node, 0), op_str, sizeof(op_str));
    char rhs[MAX_OPERAND];
    icg_not_expr(child(node, 1), rhs, sizeof(rhs));

    /* Create a new temporary for the result */
    char temp[MAX_OPERAND];
    new_temp(temp, sizeof(temp));
    emit(cmp_op(op_str), lhs, rhs, temp);

    /* Update the left-hand side with the result */
    strncpy(lhs, temp, lhssz - 1);
    icg_comparison_prime(child(node, 2), lhs, lhssz);
}

/* Function to handle comparison expressions */
static void icg_comparison(ParseNode *node, char *out, int outsz)
{
    /* Initialize the output string */
    out[0] = '\0';
    if (!node)
        return;

    /* Handle the first child and copy its value to the output */
    char lhs[MAX_OPERAND];
    icg_not_expr(child(node, 0), lhs, sizeof(lhs));
    icg_comparison_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/* Function to handle the right-recursive tail of logical expressions */
static void icg_logical_expr_prime(ParseNode *node, char *lhs, int lhssz)
{
    /* Handles an epsilon production */
    if (!node || node->child_count == 0)
        return; /* ε */

    /* Extract the logical operator and right-hand side */
    char kw[16];
    extract_lexeme(child(node, 0), kw, sizeof(kw));
    char rhs[MAX_OPERAND];
    char temp[MAX_OPERAND];
    char skip[MAX_OPERAND];
    new_label(skip, sizeof(skip));
    new_temp(temp, sizeof(temp));

    /* If the operator is "and",  */
    if (strcmp(kw, "and") == 0)
    {
        /* If lhs is false, skip evaluation of rhs */
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
        /* If the operator is "or", jump to done if lhs is true */
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

    /* Update the left-hand side with the result */
    strncpy(lhs, temp, lhssz - 1);
    icg_logical_expr_prime(child(node, 2), lhs, lhssz);
}

/* Function to handle logical expressions */
static void icg_logical_expr(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    /* Evaluate the first comparison and copy its value to the output */
    char lhs[MAX_OPERAND];
    icg_comparison(child(node, 0), lhs, sizeof(lhs));
    icg_logical_expr_prime(child(node, 1), lhs, sizeof(lhs));
    strncpy(out, lhs, outsz - 1);
}

/* Function to handle not expressions */
static void icg_not_expr(ParseNode *node, char *out, int outsz)
{
    out[0] = '\0';
    if (!node)
        return;

    /* Evaluate the first child and copy its value to the output */
    ParseNode *c0 = child(node, 0);
    if (!c0)
        return;

    /* If the node is a NOT keyword */
    if (label_is(c0, "KEYWORD_NOT"))
    {
        /* Evaluate the nested expression and apply the NOT operation */
        char inner[MAX_OPERAND];
        icg_not_expr(child(node, 1), inner, sizeof(inner));
        new_temp(out, outsz);
        emit("NOT", inner, "", out);
    }
    else
    {
        /* Evaluate the first child and copy its value to the output */
        icg_expr(c0, out, outsz);
    }
}

/* Function to handle block nodes */
static void icg_block(ParseNode *node)
{
    /* Check if the node is valid */
    if (!node)
        return;
    /* Walk through the block node's children */
    for (int i = 0; i < node->child_count; i++)
    {
        /* Evaluate each child node and execute the corresponding code */
        ParseNode *c = node->children[i];
        if (label_is(c, "stmt"))
            icg_stmt(c);
        if (label_is(c, "stmt_list"))
            icg_stmt_list(c);
    }
}

/* Function to handle if statements */
static void icg_if_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* Evaluate the logical expression */
    char cond[MAX_OPERAND];
    icg_logical_expr(child(node, 1), cond, sizeof(cond));

    /* check whether the else_clause is non-empty */
    ParseNode *else_node = child(node, 4);
    int has_else = (else_node && else_node->child_count > 0);

    char l_else[MAX_OPERAND], l_end[MAX_OPERAND];
    new_label(l_else, sizeof(l_else));
    new_label(l_end, sizeof(l_end));

    /* If the else clause exists */
    if (has_else)
    {
        /* Check if the condition is false */
        emit_if_false(cond, l_else);
        icg_block(child(node, 3)); /* then block */
        emit_goto(l_end);
        emit_label(l_else);
        /* Evaluate the else block */
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

/* Function to handle while statements */
static void icg_while_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* Create labels for the start and end of the loop */
    char l_start[MAX_OPERAND], l_end[MAX_OPERAND];
    new_label(l_start, sizeof(l_start));
    new_label(l_end, sizeof(l_end));

    /* Emit the start label */
    emit_label(l_start);

    /* Evaluate the logical expression */
    char cond[MAX_OPERAND];
    icg_logical_expr(child(node, 1), cond, sizeof(cond));

    emit_if_false(cond, l_end);
    icg_block(child(node, 3));
    emit_goto(l_start);
    emit_label(l_end);
}

/* Function to handle for statements */
static void icg_for_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* Extract the loop variable name */
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

/* Function to handle assignment statements */
static void icg_assign_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* Extract the variable name */
    char var_name[MAX_OPERAND];
    extract_lexeme(child(node, 0), var_name, sizeof(var_name));

    /* Evaluate the right-hand side expression */
    char rhs[MAX_OPERAND];
    icg_logical_expr(child(node, 2), rhs, sizeof(rhs));

    /* Emit the assignment instruction */
    emit("ASSIGN", rhs, "", var_name);
}

/* Function to handle print statements */
static void icg_print_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* Evaluate the argument */
    char arg[MAX_OPERAND];
    icg_logical_expr(child(node, 2), arg, sizeof(arg));

    /* Emit the print instruction */
    emit("PRINT", arg, "", "");
}

/* Function to print the generated intermediate code */
void icg_print(void)
{
/* Column widths */
#define W_NO 4
#define W_OP 14
#define W_RES 16
#define W_ARG1 16
#define W_ARG2 16

    /* Separator */
    const char *SEP =
        "----------------------------------------------------------------"
        "---"; /* 67 chars */

    printf("\nINTERMEDIATE CODE GENERATED (QUADRUPLES):\n");
    printf("%-*s  %-*s  %-*s  %-*s  %-*s\n",
           W_NO, "No.",
           W_OP, "OP",
           W_RES, "RESULT",
           W_ARG1, "ARG1",
           W_ARG2, "ARG2");
    printf("%s\n", SEP);

    for (int i = 0; i < quad_count; i++)
    {
        Quad *q = &quads[i];

        /* Normalise empty strings to "-" for display */
        const char *op = (q->op[0]) ? q->op : "-";
        const char *res = (q->result[0]) ? q->result : "-";
        const char *arg1 = (q->arg1[0]) ? q->arg1 : "-";
        const char *arg2 = (q->arg2[0]) ? q->arg2 : "-";

        /* Rename ops to the short display names used in the reference table */
        char op_disp[MAX_OP];
        if (strcmp(q->op, "IF_FALSE_GOTO") == 0)
            strcpy(op_disp, "IF_FALSE");
        else if (strcmp(q->op, "IF_TRUE_GOTO") == 0)
            strcpy(op_disp, "IF_TRUE");
        else
            strncpy(op_disp, op, MAX_OP - 1);
        op_disp[MAX_OP - 1] = '\0';

        /* For IF_FALSE / IF_TRUE the jump target lives in resultand arg1 holds the condition variable, swap for display so the table reads:  IF_FALSE  <label>  <cond>  - */
        const char *disp_res = res;
        const char *disp_arg1 = arg1;
        const char *disp_arg2 = arg2;

        if (strcmp(q->op, "IF_FALSE_GOTO") == 0 ||
            strcmp(q->op, "IF_TRUE_GOTO") == 0)
        {
            disp_res = (q->result[0]) ? q->result : "-"; /* label  */
            disp_arg1 = (q->arg1[0]) ? q->arg1 : "-";    /* cond   */
            disp_arg2 = "-";
        }

        printf("%*d  %-*s  %-*s  %-*s  %-*s\n",
               W_NO, i + 1, /* 1-based numbering */
               W_OP, op_disp,
               W_RES, disp_res,
               W_ARG1, disp_arg1,
               W_ARG2, disp_arg2);
    }

    printf("%s\n", SEP);
    printf("Total instructions: %d\n\n", quad_count);

#undef W_NO
#undef W_OP
#undef W_RES
#undef W_ARG1
#undef W_ARG2
}

/* Function to handle statement nodes */
static void icg_stmt(ParseNode *node)
{
    if (!node || node->child_count == 0)
        return;
    ParseNode *inner = child(node, 0);
    if (!inner)
        return;

    /* Dispatch if the statement is an assignment */
    if (label_is(inner, "assign_stmt"))
        icg_assign_stmt(inner);
    /* Dispatch if the statement is a print */
    else if (label_is(inner, "print_stmt"))
        icg_print_stmt(inner);
    /* Dispatch if the statement is an if statement */
    else if (label_is(inner, "if_stmt"))
        icg_if_stmt(inner);
    /* Dispatch if the statement is a while statement */
    else if (label_is(inner, "while_stmt"))
        icg_while_stmt(inner);
    /* Dispatch if the statement is a for statement */
    else if (label_is(inner, "for_stmt"))
        icg_for_stmt(inner);
}

/* Function to handle stmt_list */
static void icg_stmt_list(ParseNode *node)
{
    if (!node)
        return;
    /* Loop through all child nodes and handle each statement */
    for (int i = 0; i < node->child_count; i++)
    {
        ParseNode *c = node->children[i];
        if (label_is(c, "stmt"))
            icg_stmt(c);
        if (label_is(c, "stmt_list"))
            icg_stmt_list(c);
    }
}

/* Function that is the entry point for intermediate code generation */
void icg_generate(ParseNode *root)
{
    /* Check if the root node is valid */
    if (!root)
        return;
    /* Get the statement list from the root node (Program) */
    ParseNode *stmt_list = child(root, 0);
    icg_stmt_list(stmt_list);
    /* Print the generated intermediate code */
    icg_print();
}