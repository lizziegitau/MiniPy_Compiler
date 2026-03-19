#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "scanner.h"

static char *source = NULL; // String from source code
static int pos = 0;         // Index of current character
static int line = 1;        // Current line number

// Function to get the current character without consuming it
static char current()
{
    return source[pos];
}

// Function to get the next character without consuming it
static char peek_next()
{
    return source[pos + 1];
}

// Function to consume the current character and move to the next one
static char advance()
{
    // Get the current character and move the position forward unless the charcater is the newline character then move to next line
    char c = source[pos];
    pos++;
    if (c == '\n')
        line++;
    return c;
}

// Function to get the name of the token type as a string for output
const char *token_type_name(TokenType type)
{
    switch (type)
    {
    case TOKEN_INTEGER:
        return "INTEGER";
    case TOKEN_FLOAT:
        return "FLOAT";
    case TOKEN_STRING:
        return "STRING";
    case TOKEN_IF:
        return "IF";
    case TOKEN_ELSE:
        return "ELSE";
    case TOKEN_WHILE:
        return "WHILE";
    case TOKEN_FOR:
        return "FOR";
    case TOKEN_IN:
        return "IN";
    case TOKEN_RANGE:
        return "RANGE";
    case TOKEN_AND:
        return "AND";
    case TOKEN_OR:
        return "OR";
    case TOKEN_NOT:
        return "NOT";
    case TOKEN_PRINT:
        return "PRINT";
    case TOKEN_TRUE:
        return "TRUE";
    case TOKEN_FALSE:
        return "FALSE";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_MULTIPLY:
        return "MULTIPLY";
    case TOKEN_DIVIDE:
        return "DIVIDE";
    case TOKEN_EQUALS:
        return "EQUALS";
    case TOKEN_NOT_EQUAL:
        return "NOT_EQUAL";
    case TOKEN_LESS_THAN:
        return "LESS_THAN";
    case TOKEN_GREATER_THAN:
        return "GREATER_THAN";
    case TOKEN_LESS_EQUAL:
        return "LESS_EQUAL";
    case TOKEN_GREATER_EQUAL:
        return "GREATER_EQUAL";
    case TOKEN_ASSIGN:
        return "ASSIGN";
    case TOKEN_LPAREN:
        return "LPAREN";
    case TOKEN_RPAREN:
        return "RPAREN";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_NEWLINE:
        return "NEWLINE";
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

// Function to print the token name as output
static void print_token(Token t)
{
    printf("Line %-3d  %-15s  %s\n",
           t.line,
           token_type_name(t.type),
           t.lexeme);
}

static Token scan_number();
static Token scan_string();
static Token scan_identifier_or_keyword();
static Token scan_operator();
static Token next_token();

// Main function
int main(int argc, char *argv[])
{

    // Ensure a file name is provided
    if (argc < 2)
    {
        printf("Usage: scanner <filename>\n");
        return 1;
    }

    // Open the provided source file
    FILE *f = fopen(argv[1], "r");
    if (!f)
    {
        printf("Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    // Find the size of the provided file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory to hold the whole file and null terminator (+1)
    source = (char *)malloc(size + 1);
    if (!source)
    {
        printf("Error: out of memory\n");
        return 1;
    }

    // Read the file into source
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    // Loop function for scanning the source code and printing the line number, token type and the lexeme
    printf("%-8s  %-15s  %s\n", "Line", "Token Type", "Lexeme");
    printf("─────────────────────────────────────────────\n");

    Token t;
    do
    {
        // Get the next token and print it
        t = next_token();
        print_token(t);
    } while (t.type != TOKEN_EOF); // Ensure we stop at the end of the file

    free(source);
    return 0;
}