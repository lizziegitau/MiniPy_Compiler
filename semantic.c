#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "semantic.h"
#include "parser.h"

/* Counter for semantic errors */
int semantic_error_count = 0;

/* Flat symbol table for storing variable information */
static Symbol sym_table[SYM_TABLE_SIZE];

/* Return a human-readable string for a MiniType value */
const char *type_name(MiniType t)
{
    switch (t)
    {
    case TYPE_INT:
        return "int";
    case TYPE_FLOAT:
        return "float";
    case TYPE_STRING:
        return "string";
    case TYPE_BOOL:
        return "bool";
    default:
        return "unknown";
    }
}

/* Simple hash of a variable name which is then used as an index in [0, SYM_TABLE_SIZE) */
static unsigned int sym_hash(const char *name)
{
    unsigned int h = 5381;
    while (*name)
        h = h * 33 ^ (unsigned char)(*name++);
    return h % SYM_TABLE_SIZE;
}

/* Insert or update a symbol.  Returns a pointer to its slot */
static Symbol *sym_insert(const char *name, MiniType type)
{
    unsigned int idx = sym_hash(name);
    for (int probe = 0; probe < SYM_TABLE_SIZE; probe++)
    {
        int slot = (idx + probe) % SYM_TABLE_SIZE;
        Symbol *s = &sym_table[slot];
        if (!s->defined)
        {
            /* Empty slot then insert new entry */
            strncpy(s->name, name, SYM_NAME_MAX - 1);
            s->type = type;
            s->defined = 1;
            return s;
        }
        if (strcmp(s->name, name) == 0)
        {
            /* Existing entry then update type */
            s->type = type;
            return s;
        }
    }
    fprintf(stderr, "Semantic: symbol table full — cannot insert '%s'\n", name);
    return NULL;
}

/* Public lookup that returns NULL if the name is not in the table. */
const Symbol *sym_lookup(const char *name)
{
    unsigned int idx = sym_hash(name);
    for (int probe = 0; probe < SYM_TABLE_SIZE; probe++)
    {
        int slot = (idx + probe) % SYM_TABLE_SIZE;
        const Symbol *s = &sym_table[slot];
        if (!s->defined)
            return NULL; /* If you hit an empty slot then not found */
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

/* Print the symbol table in a formatted table. */
void sym_table_print(void)
{
    printf("=== SYMBOL TABLE (after semantic analysis) ===\n");
    printf("  %-20s  %s\n", "Variable", "Type");
    printf("  %-20s  %s\n", "--------", "----");

    int found = 0;
    for (int i = 0; i < SYM_TABLE_SIZE; i++)
    {
        if (sym_table[i].defined)
        {
            printf("  %-20s  %s\n",
                   sym_table[i].name,
                   type_name(sym_table[i].type));
            found++;
        }
    }
    if (!found)
        printf("  (empty)\n");
    printf("\n");
}

/* Return the i-th child of a parse node, or NULL if out of bounds */
static ParseNode *sa_child(ParseNode *node, int i)
{
    if (!node || i < 0 || i >= node->child_count)
        return NULL;
    return node->children[i];
}

/* Check if a parse node's label starts with a given prefix */
static int sa_label_is(ParseNode *node, const char *prefix)
{
    if (!node)
        return 0;
    return strncmp(node->label, prefix, strlen(prefix)) == 0;
}

/* Extract the raw lexeme from a terminal label like INTEGER("85") → "85" */
static void sa_extract_lexeme(ParseNode *node, char *buf, int bufsz)
{
    buf[0] = '\0';
    if (!node)
        return;
    const char *open = strchr(node->label, '"');
    const char *close = strrchr(node->label, '"');
    if (!open || !close || open == close)
        return;
    open++;
    int len = (int)(close - open);
    if (len <= 0)
        return;
    if (len >= bufsz)
        len = bufsz - 1;
    strncpy(buf, open, len);
    buf[len] = '\0';
}

/* Emit a semantic error with a clear message. */
static void sem_error(const char *msg)
{
    fprintf(stderr, "Semantic Error: %s\n", msg);
    semantic_error_count++;
}

/* Promote two types to a common type */
static MiniType promote(MiniType a, MiniType b)
{
    if (a == b)
        return a;
    if (a == TYPE_FLOAT || b == TYPE_FLOAT)
        return TYPE_FLOAT;
    if (a == TYPE_INT || b == TYPE_INT)
        return TYPE_INT;
    return TYPE_UNKNOWN;
}

/* Function declarations */
static MiniType sa_logical_expr(ParseNode *node);
static MiniType sa_comparison(ParseNode *node);
static MiniType sa_not_expr(ParseNode *node);
static MiniType sa_expr(ParseNode *node);
static MiniType sa_term(ParseNode *node);
static MiniType sa_factor(ParseNode *node);

/* Function to infer the type of a factor node */
static MiniType sa_factor(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;

    ParseNode *c0 = sa_child(node, 0);
    if (!c0)
        return TYPE_UNKNOWN;

    /* Unary minus: children = [OPERATOR_MINUS, factor] */
    if (sa_label_is(c0, "OPERATOR_MINUS"))
    {
        MiniType inner = sa_factor(sa_child(node, 1));
        if (inner == TYPE_STRING || inner == TYPE_BOOL)
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Unary minus applied to non-numeric type '%s'",
                     type_name(inner));
            sem_error(msg);
            return TYPE_UNKNOWN;
        }
        return inner;
    }

    /* Parenthesised expression */
    if (sa_label_is(c0, "PUNCTUATOR_LPAREN"))
        return sa_logical_expr(sa_child(node, 1));

    /* Integer literal */
    if (sa_label_is(c0, "INTEGER"))
        return TYPE_INT;

    /* Float literal */
    if (sa_label_is(c0, "FLOAT"))
        return TYPE_FLOAT;

    /* String literal */
    if (sa_label_is(c0, "STRING"))
        return TYPE_STRING;

    /* Boolean literals */
    if (sa_label_is(c0, "KEYWORD_TRUE") || sa_label_is(c0, "KEYWORD_FALSE"))
        return TYPE_BOOL;

    /* Identifier, look up in symbol table */
    if (sa_label_is(c0, "IDENTIFIER"))
    {
        char name[64];
        sa_extract_lexeme(c0, name, sizeof(name));

        const Symbol *sym = sym_lookup(name);
        if (!sym)
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Variable '%s' used before assignment", name);
            sem_error(msg);
            return TYPE_UNKNOWN;
        }
        return sym->type;
    }

    /* For any other case, return unknown type */
    return TYPE_UNKNOWN;
}

/* Function to check the recursive term_prime production */
static MiniType sa_term_prime(ParseNode *node, MiniType lhs_type)
{
    if (!node || node->child_count == 0)
        return lhs_type; /* ε */

    /* children: [0] operator  [1] factor  [2] term' */
    MiniType rhs_type = sa_factor(sa_child(node, 1));

    /* Type-check: arithmetic on strings is an error */
    if (lhs_type == TYPE_STRING || rhs_type == TYPE_STRING)
    {
        sem_error("Arithmetic operator applied to a string operand");
        return TYPE_UNKNOWN;
    }
    if (lhs_type == TYPE_BOOL || rhs_type == TYPE_BOOL)
    {
        sem_error("Arithmetic operator applied to a boolean operand");
        return TYPE_UNKNOWN;
    }

    MiniType result = promote(lhs_type, rhs_type);
    return sa_term_prime(sa_child(node, 2), result);
}

static MiniType sa_term(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;
    MiniType lhs = sa_factor(sa_child(node, 0));
    return sa_term_prime(sa_child(node, 1), lhs);
}

/* Function to check the recursive expr_prime production */
static MiniType sa_expr_prime(ParseNode *node, MiniType lhs_type)
{
    if (!node || node->child_count == 0)
        return lhs_type; /* ε */

    /* children: [0] operator  [1] term  [2] expr' */
    char op[8];
    sa_extract_lexeme(sa_child(node, 0), op, sizeof(op));

    MiniType rhs_type = sa_term(sa_child(node, 1));

    /* Allow string concatenation with + only */
    if (strcmp(op, "+") == 0 &&
        lhs_type == TYPE_STRING && rhs_type == TYPE_STRING)
        return sa_expr_prime(sa_child(node, 2), TYPE_STRING);

    if (lhs_type == TYPE_STRING || rhs_type == TYPE_STRING)
    {
        sem_error("Cannot mix string with non-string in arithmetic expression");
        return TYPE_UNKNOWN;
    }
    if (lhs_type == TYPE_BOOL || rhs_type == TYPE_BOOL)
    {
        sem_error("Arithmetic operator applied to a boolean operand");
        return TYPE_UNKNOWN;
    }

    MiniType result = promote(lhs_type, rhs_type);
    return sa_expr_prime(sa_child(node, 2), result);
}

/* Function to check the recursive expr production */
static MiniType sa_expr(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;
    MiniType lhs = sa_term(sa_child(node, 0));
    return sa_expr_prime(sa_child(node, 1), lhs);
}

/* Function to check the recursive not_expr production */
static MiniType sa_not_expr(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;
    ParseNode *c0 = sa_child(node, 0);
    if (!c0)
        return TYPE_UNKNOWN;

    if (sa_label_is(c0, "KEYWORD_NOT"))
    {
        MiniType inner = sa_not_expr(sa_child(node, 1));
        if (inner != TYPE_BOOL && inner != TYPE_UNKNOWN)
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Operator 'not' applied to non-boolean type '%s'",
                     type_name(inner));
            sem_error(msg);
        }
        return TYPE_BOOL;
    }
    return sa_expr(c0);
}

/* Function to check the recursive comparison_prime production */
static MiniType sa_comparison_prime(ParseNode *node, MiniType lhs_type)
{
    /* Check for epsilon production */
    if (!node || node->child_count == 0)
        return lhs_type;

    /* children: [0] operator  [1] not_expr  [2] comparison' */
    char op[8];
    sa_extract_lexeme(sa_child(node, 0), op, sizeof(op));

    MiniType rhs_type = sa_not_expr(sa_child(node, 1));

    /* Ordered comparisons (<, >, <=, >=) are only valid on numbers */
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0)
    {
        if ((lhs_type == TYPE_STRING || lhs_type == TYPE_BOOL) ||
            (rhs_type == TYPE_STRING || rhs_type == TYPE_BOOL))
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Operator '%s' cannot be applied to types '%s' and '%s'",
                     op, type_name(lhs_type), type_name(rhs_type));
            sem_error(msg);
        }
    }

    /* Equality (== / !=) requires matching types (int vs float is allowed) */
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0)
    {
        if (lhs_type != rhs_type &&
            !((lhs_type == TYPE_INT || lhs_type == TYPE_FLOAT) &&
              (rhs_type == TYPE_INT || rhs_type == TYPE_FLOAT)))
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Comparison '%s' between incompatible types '%s' and '%s'",
                     op, type_name(lhs_type), type_name(rhs_type));
            sem_error(msg);
        }
    }

    /* The result of any comparison is always bool */
    return sa_comparison_prime(sa_child(node, 2), TYPE_BOOL);
}

/* Function to check the comparison production */
static MiniType sa_comparison(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;
    MiniType lhs = sa_not_expr(sa_child(node, 0));
    return sa_comparison_prime(sa_child(node, 1), lhs);
}

/* Function to check the recursive logical_expr_prime production */
static MiniType sa_logical_expr_prime(ParseNode *node, MiniType lhs_type)
{
    if (!node || node->child_count == 0)
        return lhs_type; /* ε */

    /* children: [0] AND/OR  [1] comparison  [2] logical_expr' */
    char kw[8];
    sa_extract_lexeme(sa_child(node, 0), kw, sizeof(kw));

    MiniType rhs_type = sa_comparison(sa_child(node, 1));

    /* Both sides of AND/OR must be boolean */
    if (lhs_type != TYPE_BOOL && lhs_type != TYPE_UNKNOWN)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Left operand of '%s' is '%s', expected bool",
                 kw, type_name(lhs_type));
        sem_error(msg);
    }
    if (rhs_type != TYPE_BOOL && rhs_type != TYPE_UNKNOWN)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Right operand of '%s' is '%s', expected bool",
                 kw, type_name(rhs_type));
        sem_error(msg);
    }

    return sa_logical_expr_prime(sa_child(node, 2), TYPE_BOOL);
}

/* Function to check the logical_expr production */
static MiniType sa_logical_expr(ParseNode *node)
{
    if (!node)
        return TYPE_UNKNOWN;
    MiniType lhs = sa_comparison(sa_child(node, 0));
    return sa_logical_expr_prime(sa_child(node, 1), lhs);
}

/* Function declarations for statement walkers */
static void sa_stmt_list(ParseNode *node);
static void sa_stmt(ParseNode *node);
static void sa_block(ParseNode *node);

/* Function to check the assignment statement */
static void sa_assign_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* children: [0] IDENTIFIER  [1] ASSIGN_OP  [2] logical_expr */
    char var_name[64];
    sa_extract_lexeme(sa_child(node, 0), var_name, sizeof(var_name));

    MiniType rhs_type = sa_logical_expr(sa_child(node, 2));

    const Symbol *existing = sym_lookup(var_name);
    if (existing && existing->type != rhs_type)
    {
        /* Type widening int -> float is allowed (like Fortran-style rules) */
        if (!((existing->type == TYPE_INT && rhs_type == TYPE_FLOAT) ||
              (existing->type == TYPE_FLOAT && rhs_type == TYPE_INT)))
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Type mismatch: variable '%s' is '%s' but assigned '%s'",
                     var_name,
                     type_name(existing->type),
                     type_name(rhs_type));
            sem_error(msg);
        }
    }

    /* Record in symbol table (insert or update) */
    sym_insert(var_name, rhs_type);
}

/* Function to check the print statement */
static void sa_print_stmt(ParseNode *node)
{
    if (!node)
        return;
    /* children: [0] PRINT  [1] LPAREN  [2] logical_expr  [3] RPAREN */
    sa_logical_expr(sa_child(node, 2));
}

/* Function to check the if statement */
static void sa_if_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* children: [0] IF  [1] logical_expr  [2] COLON  [3] block  [4] else_clause */
    MiniType cond_type = sa_logical_expr(sa_child(node, 1));
    if (cond_type != TYPE_BOOL && cond_type != TYPE_UNKNOWN)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "If-condition must be bool, got '%s'", type_name(cond_type));
        sem_error(msg);
    }

    sa_block(sa_child(node, 3));

    /* else_clause may be empty */
    ParseNode *else_node = sa_child(node, 4);
    if (else_node && else_node->child_count > 0)
        sa_block(sa_child(else_node, 2));
}

/* Function to check the while statement */
static void sa_while_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* children: [0] WHILE  [1] logical_expr  [2] COLON  [3] block */
    MiniType cond_type = sa_logical_expr(sa_child(node, 1));
    if (cond_type != TYPE_BOOL && cond_type != TYPE_UNKNOWN)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "While-condition must be bool, got '%s'", type_name(cond_type));
        sem_error(msg);
    }

    sa_block(sa_child(node, 3));
}

/* Function to check the for statement */
static void sa_for_stmt(ParseNode *node)
{
    if (!node)
        return;

    /* children: [0]FOR [1]IDENTIFIER [2]IN [3]RANGE [4]LPAREN [5]expr [6]RPAREN [7]COLON [8]block */
    char var_name[64];
    sa_extract_lexeme(sa_child(node, 1), var_name, sizeof(var_name));

    /* Introduce the loop variable as int */
    sym_insert(var_name, TYPE_INT);

    MiniType range_type = sa_expr(sa_child(node, 5));
    if (range_type != TYPE_INT && range_type != TYPE_UNKNOWN)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "range() argument must be int, got '%s'",
                 type_name(range_type));
        sem_error(msg);
    }

    sa_block(sa_child(node, 8));
}

/* Function to check a block of statements */
static void sa_block(ParseNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
    {
        ParseNode *c = node->children[i];
        if (sa_label_is(c, "stmt"))
            sa_stmt(c);
        if (sa_label_is(c, "stmt_list"))
            sa_stmt_list(c);
    }
}

/* Function to check a single statement */
static void sa_stmt(ParseNode *node)
{
    if (!node || node->child_count == 0)
        return;
    ParseNode *inner = sa_child(node, 0);
    if (!inner)
        return;

    if (sa_label_is(inner, "assign_stmt"))
        sa_assign_stmt(inner);
    else if (sa_label_is(inner, "print_stmt"))
        sa_print_stmt(inner);
    else if (sa_label_is(inner, "if_stmt"))
        sa_if_stmt(inner);
    else if (sa_label_is(inner, "while_stmt"))
        sa_while_stmt(inner);
    else if (sa_label_is(inner, "for_stmt"))
        sa_for_stmt(inner);
}

/* Function to check stmt_list */
static void sa_stmt_list(ParseNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
    {
        ParseNode *c = node->children[i];
        if (sa_label_is(c, "stmt"))
            sa_stmt(c);
        if (sa_label_is(c, "stmt_list"))
            sa_stmt_list(c);
    }
}

/* Main function for semantic analysis */
int semantic_analyse(ParseNode *root)
{
    /* Initialise the symbol table */
    memset(sym_table, 0, sizeof(sym_table));
    semantic_error_count = 0;

    /* Check the root node */
    if (!root)
    {
        fprintf(stderr, "Semantic: NULL parse tree — skipping.\n");
        return 1;
    }

    /* Check the statement list */
    sa_stmt_list(sa_child(root, 0));

    /* Print the decorated symbol table */
    sym_table_print();

    /* Return the number of semantic errors */
    return semantic_error_count;
}