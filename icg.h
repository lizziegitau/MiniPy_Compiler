#ifndef ICG_H
#define ICG_H

#include "parser.h"

#define MAX_QUADS 1024 /* maximum number of TAC instructions */
#define MAX_OP 24      /* max length of an operator string */
#define MAX_OPERAND 64 /* max length of an operand string */

/* Structure for a three-address code quadruple */
typedef struct
{
    char op[MAX_OP];          /* operator */
    char arg1[MAX_OPERAND];   /* left operand */
    char arg2[MAX_OPERAND];   /* right operand */
    char result[MAX_OPERAND]; /* result */
} Quad;

/* Array of quadruples and their count */
extern Quad quads[MAX_QUADS];
extern int quad_count;

/* Entry point where we generate TAC from the parse tree */
void icg_generate(ParseNode *root);

/* Function to print the generated TAC */
void icg_print(void);

#endif