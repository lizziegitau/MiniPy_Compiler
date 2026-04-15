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
    // Get the current character and move the position forward unless the character is the newline character then move to next line
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
        return "KEYWORD_IF";
    case TOKEN_ELSE:
        return "KEYWORD_ELSE";
    case TOKEN_WHILE:
        return "KEYWORD_WHILE";
    case TOKEN_FOR:
        return "KEYWORD_FOR";
    case TOKEN_IN:
        return "KEYWORD_IN";
    case TOKEN_RANGE:
        return "KEYWORD_RANGE";
    case TOKEN_AND:
        return "KEYWORD_AND";
    case TOKEN_OR:
        return "KEYWORD_OR";
    case TOKEN_NOT:
        return "KEYWORD_NOT";
    case TOKEN_PRINT:
        return "KEYWORD_PRINT";
    case TOKEN_TRUE:
        return "KEYWORD_TRUE";
    case TOKEN_FALSE:
        return "KEYWORD_FALSE";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_PLUS:
        return "OPERATOR_PLUS";
    case TOKEN_MINUS:
        return "OPERATOR_MINUS";
    case TOKEN_MULTIPLY:
        return "OPERATOR_MULTIPLY";
    case TOKEN_DIVIDE:
        return "OPERATOR_DIVIDE";
    case TOKEN_EQUALS:
        return "OPERATOR_EQUALS";
    case TOKEN_NOT_EQUAL:
        return "OPERATOR_NOT_EQUAL";
    case TOKEN_LESS_THAN:
        return "OPERATOR_LESS_THAN";
    case TOKEN_GREATER_THAN:
        return "OPERATOR_GREATER_THAN";
    case TOKEN_LESS_EQUAL:
        return "OPERATOR_LESS_EQUAL";
    case TOKEN_GREATER_EQUAL:
        return "OPERATOR_GREATER_EQUAL";
    case TOKEN_ASSIGN:
        return "OPERATOR_ASSIGN";
    case TOKEN_LPAREN:
        return "PUNCTUATOR_LPAREN";
    case TOKEN_RPAREN:
        return "PUNCTUATOR_RPAREN";
    case TOKEN_COLON:
        return "PUNCTUATOR_COLON";
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

// Function to scan digits (handles both integers and floats)
static Token scan_number()
{
    Token t;
    t.line = line;
    int i = 0;

    // Since the current character is a digit, continue consuming characters as long as they are digits and store them in the lexeme of the token
    while (isdigit(current()))
    {
        t.lexeme[i++] = advance();
    }

    // Check if the next character is a dot followed by a digit to determine if this is a float or an integer
    if (current() == '.' && isdigit(peek_next()))
    {

        // consume the dot
        t.lexeme[i++] = advance();

        // Consume the digits after the dot and store them in the lexeme of the token
        while (isdigit(current()))
        {
            t.lexeme[i++] = advance();
        }

        // Null-terminate the lexeme string and set the token type to TOKEN_FLOAT since a dot followed by digits is found
        t.lexeme[i] = '\0';
        t.type = TOKEN_FLOAT;
        return t;
    }

    // No dot is found after the digits so this is an integer token, null-terminate the lexeme string and set the token type to TOKEN_INTEGER
    t.lexeme[i] = '\0';
    t.type = TOKEN_INTEGER;
    return t;
}

// Function to scan identifiers and keywords
static Token scan_identifier_or_keyword()
{
    Token t;
    t.line = line;
    int i = 0;

    // Since the current character is an alphabetic character or an underscore, continue consuming characters as long as they are alphabetic characters, digits or underscores and store them in the lexeme of the token
    while (isalpha(current()) || isdigit(current()) || current() == '_')
    {
        if (i < MAX_LEXEME - 1)
        {
            t.lexeme[i++] = advance();
        }
        else
        {
            advance();
        }
    }
    t.lexeme[i] = '\0';

    // After consuming the characters, check if the lexeme matches any of the keywords and set the token type accordingly, if it does not match any keyword then set the token type to TOKEN_IDENTIFIER
    if (strcmp(t.lexeme, "if") == 0)
        t.type = TOKEN_IF;
    else if (strcmp(t.lexeme, "else") == 0)
        t.type = TOKEN_ELSE;
    else if (strcmp(t.lexeme, "while") == 0)
        t.type = TOKEN_WHILE;
    else if (strcmp(t.lexeme, "for") == 0)
        t.type = TOKEN_FOR;
    else if (strcmp(t.lexeme, "in") == 0)
        t.type = TOKEN_IN;
    else if (strcmp(t.lexeme, "range") == 0)
        t.type = TOKEN_RANGE;
    else if (strcmp(t.lexeme, "and") == 0)
        t.type = TOKEN_AND;
    else if (strcmp(t.lexeme, "or") == 0)
        t.type = TOKEN_OR;
    else if (strcmp(t.lexeme, "not") == 0)
        t.type = TOKEN_NOT;
    else if (strcmp(t.lexeme, "print") == 0)
        t.type = TOKEN_PRINT;
    else if (strcmp(t.lexeme, "True") == 0)
        t.type = TOKEN_TRUE;
    else if (strcmp(t.lexeme, "False") == 0)
        t.type = TOKEN_FALSE;
    else
        t.type = TOKEN_IDENTIFIER;

    return t;
}

// Function to scan strings
static Token scan_string()
{
    Token t;
    t.line = line;
    int i = 0;

    // Consume the opening double quote and store it in the lexeme of the token
    t.lexeme[i++] = advance();

    // Continue consuming characters until we hit the closing double quote, end of file or a newline character and store them in the lexeme of the token
    while (current() != '"' && current() != '\0' && current() != '\n')
    {
        if (i >= MAX_LEXEME - 2)
        {
            break;
        }
        t.lexeme[i++] = advance();
    }

    // Check the reason we stopped consuming characters:
    //  Case 1: We hit the closing double quote thus this is a valid string token. Consume the closing double quote, terminate and set the token type to TOKEN_STRING
    if (current() == '"')
    {
        t.lexeme[i++] = advance();
        t.lexeme[i] = '\0';
        t.type = TOKEN_STRING;
        return t;
    }

    // Case 2: We hit an end of line or newline character before consuming a closing double quote thus this is an invalid string token. Terminate and set the token type to TOKEN_ERROR since this a string with an unterminated double quote
    t.lexeme[i] = '\0';
    t.type = TOKEN_ERROR;
    return t;
}

// Function to scan operators and punctuators
static Token scan_operator()
{
    Token t;
    t.line = line;

    // Consume the first operator character
    char op = advance();

    // Check the operator character and determine the token type accordingly, also handle two-character operators by peeking at the next character and consuming it if it matches
    switch (op)
    {

    // Single-character operators:
    // Case 1: The current character is an addition operator, set the token type to TOKEN_PLUS and the lexeme to "+"
    case '+':
        t.type = TOKEN_PLUS;
        t.lexeme[0] = '+';
        t.lexeme[1] = '\0';
        break;

    // Case 2: The current character is a subtraction operator, set the token type to TOKEN_MINUS and the lexeme to "-"
    case '-':
        t.type = TOKEN_MINUS;
        t.lexeme[0] = '-';
        t.lexeme[1] = '\0';
        break;

    // Case 3: The current character is a multiplication operator, set the token type to TOKEN_MULTIPLY and the lexeme to "*"
    case '*':
        t.type = TOKEN_MULTIPLY;
        t.lexeme[0] = '*';
        t.lexeme[1] = '\0';
        break;

    // Case 4: The current character is a division operator, set the token type to TOKEN_DIVIDE and the lexeme to "/"
    case '/':
        t.type = TOKEN_DIVIDE;
        t.lexeme[0] = '/';
        t.lexeme[1] = '\0';
        break;

    // Case 5: The current character is a left parenthesis, set the token type to TOKEN_LPAREN and the lexeme to "("
    case '(':
        t.type = TOKEN_LPAREN;
        t.lexeme[0] = '(';
        t.lexeme[1] = '\0';
        break;

    // Case 6: The current character is a right parenthesis, set the token type to TOKEN_RPAREN and the lexeme to ")"
    case ')':
        t.type = TOKEN_RPAREN;
        t.lexeme[0] = ')';
        t.lexeme[1] = '\0';
        break;

    // Case 7: The current character is a colon, set the token type to TOKEN_COLON and the lexeme to ":"
    case ':':
        t.type = TOKEN_COLON;
        t.lexeme[0] = ':';
        t.lexeme[1] = '\0';
        break;

    // Two-character operators:
    // Case 8: The current character is a less than operator, check if the next character is an equals sign to determine if this is a less than or less than or equal to operator. If the next character is an equals sign, consume it, set the token type to TOKEN_LESS_EQUAL and the lexeme to "<=", otherwise set the token type to TOKEN_LESS_THAN and the lexeme to "<"
    case '<':
        if (current() == '=')
        {
            advance();
            t.type = TOKEN_LESS_EQUAL;
            strcpy(t.lexeme, "<=");
        }
        else
        {
            t.type = TOKEN_LESS_THAN;
            t.lexeme[0] = '<';
            t.lexeme[1] = '\0';
        }
        break;

    // Case 9: The current character is a greater than operator, check if the next character is an equals sign to determine if this is a greater than or greater than or equal to operator. If the next character is an equals sign, consume it, set the token type to TOKEN_GREATER_EQUAL and the lexeme to ">=", otherwise set the token type to TOKEN_GREATER_THAN and the lexeme to ">"
    case '>':
        if (current() == '=')
        {
            advance();
            t.type = TOKEN_GREATER_EQUAL;
            strcpy(t.lexeme, ">=");
        }
        else
        {
            t.type = TOKEN_GREATER_THAN;
            t.lexeme[0] = '>';
            t.lexeme[1] = '\0';
        }
        break;

    // Case 10: The current character is an equals sign, check if the next character is also an equals sign to determine if this is an equality operator or an assignment operator. If the next character is an equals sign, consume it, set the token type to TOKEN_EQUALS and the lexeme to "==", otherwise set the token type to TOKEN_ASSIGN and the lexeme to "="
    case '=':
        if (current() == '=')
        {
            advance();
            t.type = TOKEN_EQUALS;
            strcpy(t.lexeme, "==");
        }
        else
        {
            t.type = TOKEN_ASSIGN;
            t.lexeme[0] = '=';
            t.lexeme[1] = '\0';
        }
        break;

    // Case 11: The current character is an exclamation mark, check if the next character is an equals sign to determine if this is a not equal operator. If the next character is an equals sign, consume it, set the token type to TOKEN_NOT_EQUAL and the lexeme to "!=", otherwise this is an invalid token since '!' alone does not have any meaning in MiniPy, set the token type to TOKEN_ERROR and the lexeme to "!"
    case '!':
        if (current() == '=')
        {
            advance();
            t.type = TOKEN_NOT_EQUAL;
            strcpy(t.lexeme, "!=");
        }
        else
        {
            t.type = TOKEN_ERROR;
            t.lexeme[0] = '!';
            t.lexeme[1] = '\0';
        }
        break;

    // Default case: The current character does not match any valid operator or punctuator, set the token type to TOKEN_ERROR and the lexeme to the invalid character. It should be noted that this default case should never be reached since we only call scan_operator() when the current character is a valid operator or punctuator, but this is here as a safety measure
    default:
        t.type = TOKEN_ERROR;
        t.lexeme[0] = op;
        t.lexeme[1] = '\0';
        break;
    }

    return t;
}

// Function that dispatches by looking at current character and route it to the appropriate DFA function.
static Token next_token()
{
    Token t;

    // Skip whitespace and tab characters and move to next character by calling advance()
    while (current() == ' ' || current() == '\t' ||
           current() == '\r' || current() == '\n')
    {
        advance();
    }

    // Record the line number the token is from
    t.line = line;

    // Handles when we reach the end of the file by outputting the EOF token
    if (current() == '\0')
    {
        t.type = TOKEN_EOF;
        strcpy(t.lexeme, "EOF");
        return t;
    }

    // Handles when a comment (anything starting with #) is scanned by skipping the whole line entirely
    if (current() == '#')
    {
        while (current() != '\n' && current() != '\0')
        {
            advance();
        }
        // After skipping the comment, get the next real token by calling next_token()
        return next_token();
    }

    // Handles when a digit is scanned by calling scan_number which will handle bith integers and floats
    if (isdigit(current()))
    {
        return scan_number();
    }

    // Handles when an alphabetic character or an underscore is scanned by calling scan_identifier_or_keyword() which will handle both identifiers and keywords
    if (isalpha(current()) || current() == '_')
    {
        return scan_identifier_or_keyword();
    }

    // Handles the situation when a double quote is scanned by calling scan_string() which will handles strings
    if (current() == '"')
    {
        return scan_string();
    }

    // Handles when an operator or a punctuator is scanned by calling scan_operator() which will handle both operators and punctuators
    if (current() == '+' || current() == '-' ||
        current() == '*' || current() == '/' ||
        current() == '=' || current() == '!' ||
        current() == '<' || current() == '>' ||
        current() == '(' || current() == ')' ||
        current() == ':')
    {
        return scan_operator();
    }

    // A catch-all function which handles when the character scanned does not match any valid output by outputting an error token with the invalid chacter as the lexeme and moves to the next character by calling advance()
    t.type = TOKEN_ERROR;
    t.lexeme[0] = current();
    t.lexeme[1] = '\0';
    advance();
    return t;
}

// Main function
int main(int argc, char *argv[])
{

    // Ensure a file name is provided
    if (argc < 2)
    {
        printf("Usage: scanner <filename>\n");
        return 1;
    }

    // Open the provided source file in binary mode
    FILE *f = fopen(argv[1], "rb");
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
    printf("---------------------------------------------\n");

    Token t;
    do
    {
        t = next_token();
        // Only print real tokens — EOF is internal only
        if (t.type != TOKEN_EOF)
        {
            print_token(t);
        }
    } while (t.type != TOKEN_EOF);

    free(source);
    return 0;
}