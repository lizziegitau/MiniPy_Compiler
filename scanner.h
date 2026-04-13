#ifndef SCANNER_H
#define SCANNER_H

#define MAX_LEXEME 256

// Token enum
typedef enum
{
    // Numbers
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    // Strings
    TOKEN_STRING,
    // Keywords
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_RANGE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_PRINT,
    TOKEN_TRUE,
    TOKEN_FALSE,
    // Identifier
    TOKEN_IDENTIFIER,

    // Arithmetic operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,

    // Comparison operators
    TOKEN_EQUALS,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    // Assignment operator
    TOKEN_ASSIGN,

    // Punctuators
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COLON,

    // Special tokens
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// Token struct
typedef struct
{
    TokenType type;          // Token type
    char lexeme[MAX_LEXEME]; // Actual lexeme from source code
    int line;                // Line of the source code that said lexeme is from
} Token;

#endif