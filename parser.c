#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
#include "parser.h"

// Forward declaration from scanner.c
const char *token_type_name(TokenType type);
void scanner_set_source(char *src);

// Scanner interface, it re-uses the public next_token() from scanner.c
extern Token next_token(void);

// Global parser state
int parse_error_count = 0;

static Token current_token; // The one-token lookahead buffer

// Parse-node helpers
//  Creates the parent node with the given label and terminal status and returns a pointer to it.
static ParseNode *make_node(const char *label, int is_terminal)
{
    ParseNode *n = (ParseNode *)calloc(1, sizeof(ParseNode));
    if (!n)
    {
        fprintf(stderr, "Fatal: out of memory\n");
        exit(1);
    }
    strncpy(n->label, label, MAX_LABEL - 1);
    n->is_terminal = is_terminal;
    return n;
}

// Adds a child node to the parent node's children array, if there is space (remember MAX_CHILDREN = 8). If the child is NULL, it does nothing
static void add_child(ParseNode *parent, ParseNode *child)
{
    if (!child)
        return;
    if (parent->child_count < MAX_CHILDREN)
        parent->children[parent->child_count++] = child;
}

// Lookahead helpers
// peek() checks if the current token is of the specified type without consuming it.
static int peek(TokenType t) { return current_token.type == t; }

// advance() consumes the current token and loads the next one into the lookahead buffer, returning the consumed token
static Token advance_token(void)
{
    Token consumed = current_token;
    current_token = next_token();
    return consumed;
}

// match() verifies the current token is of the expected type, advances and returns a leaf node.  On mismatch it emits an error and returns an ERROR leaf so the tree remains as complete as possible
static ParseNode *match(TokenType expected)
{
    if (current_token.type == expected)
    {
        // Build a leaf node labelled: TYPE("lexeme")
        char label[MAX_LABEL];
        snprintf(label, sizeof(label), "%s(\"%s\")",
                 token_type_name(expected), current_token.lexeme);
        ParseNode *leaf = make_node(label, 1);
        advance_token();
        return leaf;
    }
    else
    {
        // Syntax error: report but do NOT advance, let callers recover
        parse_error_count++;
        fprintf(stderr,
                "Syntax Error on Line %d: Expected %s but got %s (\"%s\")\n",
                current_token.line,
                token_type_name(expected),
                token_type_name(current_token.type),
                current_token.lexeme);

        // Return a special ERROR leaf so the tree is still printable and we can continue parsing.  The label includes the expected type and the actual lexeme for clarity
        char label[MAX_LABEL];
        snprintf(label, sizeof(label), "ERROR(expected %s, got \"%s\")",
                 token_type_name(expected), current_token.lexeme);
        return make_node(label, 1);
    }
}

// Panic-mode error recovery. Skips tokens until a "synchronisation" point is found
static void synchronise(void)
{
    while (!peek(TOKEN_EOF) &&
           !peek(TOKEN_IF) && !peek(TOKEN_ELSE) &&
           !peek(TOKEN_WHILE) && !peek(TOKEN_FOR) &&
           !peek(TOKEN_PRINT) && !peek(TOKEN_IDENTIFIER))
    {
        advance_token();
    }
}

// Forward declarations for mutually recursive functions
static ParseNode *parse_stmt_list(void);
static ParseNode *parse_stmt(void);
static ParseNode *parse_if_stmt(void);
static ParseNode *parse_while_stmt(void);
static ParseNode *parse_for_stmt(void);
static ParseNode *parse_print_stmt(void);
static ParseNode *parse_assign_stmt(void);
static ParseNode *parse_block(void);
static ParseNode *parse_logical_expr(void);
static ParseNode *parse_logical_expr_prime(void);
static ParseNode *parse_comparison(void);
static ParseNode *parse_comparison_prime(void);
static ParseNode *parse_not_expr(void);
static ParseNode *parse_expr(void);
static ParseNode *parse_expr_prime(void);
static ParseNode *parse_term(void);
static ParseNode *parse_term_prime(void);
static ParseNode *parse_factor(void);

// program -> stmt_list EOF
static ParseNode *parse_program(void)
{
    ParseNode *node = make_node("program", 0);
    add_child(node, parse_stmt_list());
    add_child(node, match(TOKEN_EOF));
    return node;
}

// stmt_list -> stmt stmt_list | ε
static ParseNode *parse_stmt_list(void)
{
    ParseNode *node = make_node("stmt_list", 0);

    /* FIRST(stmt) = { IF, WHILE, FOR, PRINT, IDENTIFIER }*/
    while (peek(TOKEN_IF) || peek(TOKEN_WHILE) || peek(TOKEN_FOR) ||
           peek(TOKEN_PRINT) || peek(TOKEN_IDENTIFIER))
    {
        add_child(node, parse_stmt());
    }
    /* ε case: just return the (possibly empty) node*/
    return node;
}

// stmt -> if_stmt | while_stmt | for_stmt | print_stmt | assign_stmt
static ParseNode *parse_stmt(void)
{
    ParseNode *node = make_node("stmt", 0);

    if (peek(TOKEN_IF))
        add_child(node, parse_if_stmt());
    else if (peek(TOKEN_WHILE))
        add_child(node, parse_while_stmt());
    else if (peek(TOKEN_FOR))
        add_child(node, parse_for_stmt());
    else if (peek(TOKEN_PRINT))
        add_child(node, parse_print_stmt());
    else if (peek(TOKEN_IDENTIFIER))
        add_child(node, parse_assign_stmt());
    else
    {
        // Unexpected token, report error and recover by skipping to the next statement
        parse_error_count++;
        fprintf(stderr,
                "Syntax Error on Line %d: Unexpected token \"%s\"\n",
                current_token.line, current_token.lexeme);
        synchronise();
    }
    return node;
}

// if_stmt -> IF logical_expr COLON block ( ELSE COLON block )
static ParseNode *parse_if_stmt(void)
{
    ParseNode *node = make_node("if_stmt", 0);
    add_child(node, match(TOKEN_IF));
    add_child(node, parse_logical_expr());
    add_child(node, match(TOKEN_COLON));
    add_child(node, parse_block());

    // A dangling else is resolved by greedy match
    if (peek(TOKEN_ELSE))
    {
        ParseNode *else_node = make_node("else_clause", 0);
        add_child(else_node, match(TOKEN_ELSE));
        add_child(else_node, match(TOKEN_COLON));
        add_child(else_node, parse_block());
        add_child(node, else_node);
    }
    return node;
}

// while_stmt -> WHILE logical_expr COLON block
static ParseNode *parse_while_stmt(void)
{
    ParseNode *node = make_node("while_stmt", 0);
    add_child(node, match(TOKEN_WHILE));
    add_child(node, parse_logical_expr());
    add_child(node, match(TOKEN_COLON));
    add_child(node, parse_block());
    return node;
}

// for_stmt -> FOR IDENTIFIER IN RANGE LPAREN expr RPAREN COLON block
static ParseNode *parse_for_stmt(void)
{
    ParseNode *node = make_node("for_stmt", 0);
    add_child(node, match(TOKEN_FOR));
    add_child(node, match(TOKEN_IDENTIFIER));
    add_child(node, match(TOKEN_IN));
    add_child(node, match(TOKEN_RANGE));
    add_child(node, match(TOKEN_LPAREN));
    add_child(node, parse_expr());
    add_child(node, match(TOKEN_RPAREN));
    add_child(node, match(TOKEN_COLON));
    add_child(node, parse_block());
    return node;
}

// print_stmt -> PRINT LPAREN expr RPAREN
static ParseNode *parse_print_stmt(void)
{
    ParseNode *node = make_node("print_stmt", 0);
    add_child(node, match(TOKEN_PRINT));
    add_child(node, match(TOKEN_LPAREN));
    add_child(node, parse_logical_expr());
    add_child(node, match(TOKEN_RPAREN));
    return node;
}

// assign_stmt -> IDENTIFIER ASSIGN logical_expr
static ParseNode *parse_assign_stmt(void)
{
    ParseNode *node = make_node("assign_stmt", 0);
    add_child(node, match(TOKEN_IDENTIFIER));
    add_child(node, match(TOKEN_ASSIGN));
    add_child(node, parse_logical_expr());
    return node;
}

// block -> stmt stmt_list (Called AFTER the COLON token has already been matched by the caller.) In MiniPy (like Python) the body is the next non-empty statement(s). We parse at least ONE statement for the block
static ParseNode *parse_block(void)
{
    ParseNode *node = make_node("block", 0);

    if (peek(TOKEN_IF) || peek(TOKEN_WHILE) || peek(TOKEN_FOR) ||
        peek(TOKEN_PRINT) || peek(TOKEN_IDENTIFIER))
    {
        add_child(node, parse_stmt());
        // Additional statements at the same "indent level"
        add_child(node, parse_stmt_list());
    }
    else
    {
        // Empty block – syntax error
        parse_error_count++;
        fprintf(stderr,
                "Syntax Error on Line %d: Expected statement in block, got \"%s\"\n",
                current_token.line, current_token.lexeme);
        synchronise();
    }
    return node;
}

// logical_expr -> comparison logical_expr'
static ParseNode *parse_logical_expr(void)
{
    ParseNode *node = make_node("logical_expr", 0);
    add_child(node, parse_comparison());
    add_child(node, parse_logical_expr_prime());
    return node;
}

// logical_expr' -> AND comparison logical_expr' | OR  comparison logical_expr' | ε
static ParseNode *parse_logical_expr_prime(void)
{
    ParseNode *node = make_node("logical_expr'", 0);

    if (peek(TOKEN_AND) || peek(TOKEN_OR))
    {
        add_child(node, match(current_token.type)); // AND or OR
        add_child(node, parse_comparison());
        add_child(node, parse_logical_expr_prime());
    }
    // else ε – return empty node
    return node;
}

// comparison -> not_expr comparison'
static ParseNode *parse_comparison(void)
{
    ParseNode *node = make_node("comparison", 0);
    add_child(node, parse_not_expr());
    add_child(node, parse_comparison_prime());
    return node;
}

// comparison' -> (== | != | < | > | <= | >=) not_expr comparison' | ε
static ParseNode *parse_comparison_prime(void)
{
    ParseNode *node = make_node("comparison'", 0);

    if (peek(TOKEN_EQUALS) || peek(TOKEN_NOT_EQUAL) ||
        peek(TOKEN_LESS_THAN) || peek(TOKEN_GREATER_THAN) ||
        peek(TOKEN_LESS_EQUAL) || peek(TOKEN_GREATER_EQUAL))
    {
        add_child(node, match(current_token.type));
        add_child(node, parse_not_expr());
        add_child(node, parse_comparison_prime());
    }
    return node;
}

// not_expr -> NOT not_expr | expr
static ParseNode *parse_not_expr(void)
{
    ParseNode *node = make_node("not_expr", 0);

    if (peek(TOKEN_NOT))
    {
        add_child(node, match(TOKEN_NOT));
        add_child(node, parse_not_expr());
    }
    else
    {
        add_child(node, parse_expr());
    }
    return node;
}

// expr -> term expr'
static ParseNode *parse_expr(void)
{
    ParseNode *node = make_node("expr", 0);
    add_child(node, parse_term());
    add_child(node, parse_expr_prime());
    return node;
}

// expr' -> PLUS term expr' | MINUS term expr' | ε
static ParseNode *parse_expr_prime(void)
{
    ParseNode *node = make_node("expr'", 0);

    if (peek(TOKEN_PLUS) || peek(TOKEN_MINUS))
    {
        add_child(node, match(current_token.type));
        add_child(node, parse_term());
        add_child(node, parse_expr_prime());
    }
    return node;
}

// term -> factor term'
static ParseNode *parse_term(void)
{
    ParseNode *node = make_node("term", 0);
    add_child(node, parse_factor());
    add_child(node, parse_term_prime());
    return node;
}

// term' -> MULTIPLY factor term' | DIVIDE factor term' | ε
static ParseNode *parse_term_prime(void)
{
    ParseNode *node = make_node("term'", 0);

    if (peek(TOKEN_MULTIPLY) || peek(TOKEN_DIVIDE))
    {
        add_child(node, match(current_token.type));
        add_child(node, parse_factor());
        add_child(node, parse_term_prime());
    }
    return node;
}

// factor -> INTEGER | FLOAT | STRING | TRUE | FALSE | IDENTIFIER | LPAREN logical_expr RPAREN | MINUS factor
static ParseNode *parse_factor(void)
{
    ParseNode *node = make_node("factor", 0);

    if (peek(TOKEN_INTEGER) || peek(TOKEN_FLOAT) || peek(TOKEN_STRING) ||
        peek(TOKEN_TRUE) || peek(TOKEN_FALSE))
    {
        add_child(node, match(current_token.type));
    }
    else if (peek(TOKEN_IDENTIFIER))
    {
        add_child(node, match(TOKEN_IDENTIFIER));
    }
    else if (peek(TOKEN_LPAREN))
    {
        add_child(node, match(TOKEN_LPAREN));
        add_child(node, parse_logical_expr());
        add_child(node, match(TOKEN_RPAREN));
    }
    else if (peek(TOKEN_MINUS))
    {
        // Unary minus
        add_child(node, match(TOKEN_MINUS));
        add_child(node, parse_factor());
    }
    else
    {
        // Unexpected token in factor position
        parse_error_count++;
        fprintf(stderr,
                "Syntax Error on Line %d: Unexpected token \"%s\" in expression\n",
                current_token.line, current_token.lexeme);
        // Create an error leaf and advance to avoid infinite loop
        char label[MAX_LABEL];
        snprintf(label, sizeof(label), "ERROR(\"%s\")", current_token.lexeme);
        add_child(node, make_node(label, 1));
        if (!peek(TOKEN_EOF))
            advance_token();
    }
    return node;
}

// Public API
// Print the indent-based visual parse tree.  We use a simple text-based format with indentation to represent the tree structure.  Each node is printed on its own line, with child nodes indented below their parents.  Terminal nodes (tokens) include their lexeme for clarity
void print_tree(ParseNode *node, int depth)
{
    if (!node)
        return;

    // Print indentation: 4 spaces per level, with a connector for the last child
    for (int i = 0; i < depth; i++)
    {
        if (i == depth - 1)
            printf("|-- ");
        else
            printf("|   ");
    }
    printf("%s\n", node->label);

    for (int i = 0; i < node->child_count; i++)
        print_tree(node->children[i], depth + 1);
}

// Free the memory allocated for the parse tree. This is a recursive function that traverses the tree and frees each node.  It first frees all child nodes before freeing the parent node to ensure there are no dangling pointers
void free_tree(ParseNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
        free_tree(node->children[i]);
    free(node);
}

// The main entry point for parsing a file.  It initializes the scanner, primes the lookahead, prints a token log and then performs the actual parsing to build the parse tree.  It returns a pointer to the root of the parse tree, which can be printed or freed by the caller
extern char *source_global_ptr;

void scanner_init(const char *path);

ParseNode *parse_file(const char *path)
{
    scanner_init(path);

    parse_error_count = 0;
    current_token = next_token();

    printf("=== TOKEN LOG ===\n");
    printf("%-8s  %-25s  %s\n", "Line", "Token Type", "Lexeme");
    printf("--------------------------------------------------\n");

    scanner_init(path);

    int cap = 256;
    int count = 0;
    Token *toks = (Token *)malloc(cap * sizeof(Token));
    if (!toks)
    {
        fprintf(stderr, "Fatal: OOM\n");
        exit(1);
    }

    Token t;
    do
    {
        t = next_token();
        if (count >= cap)
        {
            cap *= 2;
            toks = (Token *)realloc(toks, cap * sizeof(Token));
        }
        toks[count++] = t;
    } while (t.type != TOKEN_EOF);

    for (int i = 0; i < count; i++)
    {
        if (toks[i].type != TOKEN_EOF)
            printf("Line %-3d  %-25s  %s\n",
                   toks[i].line,
                   token_type_name(toks[i].type),
                   toks[i].lexeme);
    }
    printf("\n");

    scanner_init(path);
    parse_error_count = 0;
    current_token = next_token();

    ParseNode *root = parse_program();

    free(toks);
    return root;
}

// A single global buffer is used for the source code, which is loaded by scanner_init() and freed at the end of parse_file()
static char *_src_buf = NULL;

// It loads the entire source file into memory and sets up the scanner's internal state to point to this buffer
void scanner_init(const char *path)
{
    if (_src_buf)
    {
        free(_src_buf);
        _src_buf = NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    _src_buf = (char *)malloc(sz + 1);
    if (!_src_buf)
    {
        fprintf(stderr, "Fatal: OOM\n");
        exit(1);
    }

    fread(_src_buf, 1, sz, f);
    _src_buf[sz] = '\0';
    fclose(f);

    // Set the scanner's source pointer to the loaded buffer so that the next_token() function can read from it.  This decouples the file loading from the scanning logic, allowing the scanner to operate on any source string
    scanner_set_source(_src_buf);
}