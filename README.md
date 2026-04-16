# MiniPy Compiler

## What is this?

This is a **MiniPy Compiler front-end** — built from scratch in C — for **MiniPy**, a mini subset of the Python programming language.

The compiler currently implements two phases:

1. **Lexical Analysis (Scanner)** — reads the source file and breaks it into tokens (the smallest meaningful units of the language).
2. **Syntax Analysis (Parser)** — consumes the token stream and builds a **parse tree** that reflects the grammatical structure of the program.

---

## Project Structure

```
MiniPy Scanner/
│
├── scanner.h        # Token type definitions (enum + struct)
├── scanner.c        # Full scanner implementation
├── parser.h         # ParseNode struct + public parser API
├── parser.c         # Recursive-descent parser implementation
├── main.c           # Entry point: drives scanning, parsing, and output
├── sample1.minipy   # Sample MiniPy program 1 (for testing)
├── sample2.minipy   # Sample MiniPy program 2 (covers all token types)
└── README.md        # This file
```

---

## Token Types Supported

| Token Type    | Example                           |
| ------------- | --------------------------------- |
| INTEGER       | `85`, `0`, `100`                  |
| FLOAT         | `1.5`, `3.14`, `2.0`              |
| STRING        | `"hello"`, `"Student has passed"` |
| IDENTIFIER    | `score`, `total`, `is_eligible`   |
| IF            | `if`                              |
| ELSE          | `else`                            |
| WHILE         | `while`                           |
| FOR           | `for`                             |
| IN            | `in`                              |
| RANGE         | `range`                           |
| AND           | `and`                             |
| OR            | `or`                              |
| NOT           | `not`                             |
| PRINT         | `print`                           |
| TRUE          | `True`                            |
| FALSE         | `False`                           |
| PLUS          | `+`                               |
| MINUS         | `-`                               |
| MULTIPLY      | `*`                               |
| DIVIDE        | `/`                               |
| ASSIGN        | `=`                               |
| EQUALS        | `==`                              |
| NOT_EQUAL     | `!=`                              |
| LESS_THAN     | `<`                               |
| GREATER_THAN  | `>`                               |
| LESS_EQUAL    | `<=`                              |
| GREATER_EQUAL | `>=`                              |
| LPAREN        | `(`                               |
| RPAREN        | `)`                               |
| COLON         | `:`                               |
| EOF           | end of file                       |
| ERROR         | any unrecognised character        |

---

## Requirements

You need a **C compiler** installed. We recommend **GCC**.

### Installing GCC on Windows

1. Download **MinGW-w64** from: https://www.mingw-w64.org/downloads/  
   The easiest option is the **MSYS2** installer at: https://www.msys2.org/

2. During MSYS2 setup, run this command in the MSYS2 terminal to install GCC:

   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```

3. Add GCC to your Windows PATH:
   - Search for **"Environment Variables"** in the Start menu
   - Under **System Variables**, find **Path** and click **Edit**
   - Click **New** and add: `C:\msys64\ucrt64\bin`
   - Click OK on all windows

4. Verify the installation by opening a new Command Prompt and running:
   ```
   gcc --version
   ```
   You should see something like: `gcc (Rev1) 13.2.0 ...`

### Installing GCC on Mac

Open Terminal and run:

```
xcode-select --install
```

Then verify:

```
gcc --version
```

### Installing GCC on Linux (Ubuntu/Debian)

Open Terminal and run:

```
sudo apt update
sudo apt install gcc
```

Then verify:

```
gcc --version
```

---

## How to Compile

1. Open **Command Prompt** (Windows) or **Terminal** (Mac/Linux)

2. Navigate to the project folder. For example on Windows:

   ```
   cd "C:\Users\YourName\Desktop\MiniPy Scanner"
   ```

3. Compile all three source files together:

   ```
   gcc -o minipy main.c scanner.c parser.c
   ```

   This produces an executable called `minipy.exe` (Windows) or `minipy` (Mac/Linux).

   If compilation is successful, you will see no output — just your prompt returning. If you see error messages, check that `scanner.h`, `scanner.c`, `parser.h`, `parser.c`, and `main.c` are all in the same folder.

---

## How to Run

### On Windows

```
.\minipy.exe sample1.minipy
```

or

```
minipy.exe sample1.minipy
```

### On Mac / Linux

```
./minipy sample1.minipy
```

Replace `sample1.minipy` with any `.minipy` file you want to compile.

---

## Example Output

Given this MiniPy source in `sample1.minipy`:

```python
score = 85
if score >= 50:
    print("Student has passed")
else:
    print("Student has failed")
```

The compiler produces three sections of output:

### 1. Token Log

```
=== TOKEN LOG ===
Line      Token Type                 Lexeme
--------------------------------------------------
Line 1    IDENTIFIER                 score
Line 1    OPERATOR_ASSIGN            =
Line 1    INTEGER                    85
Line 2    KEYWORD_IF                 if
Line 2    IDENTIFIER                 score
Line 2    OPERATOR_GREATER_EQUAL     >=
Line 2    INTEGER                    50
Line 2    PUNCTUATOR_COLON           :
...
```

### 2. Parse Tree

```
=== PARSE TREE ===
program
|-- stmt_list
|   |-- stmt
|   |   |-- if_stmt
|   |   |   |-- KEYWORD_IF("if")
|   |   |   |-- logical_expr
|   |   |   |   |-- comparison
|   |   |   |   |   |-- not_expr
|   |   |   |   |   |   |-- expr
|   |   |   |   |   |   |   |-- term
|   |   |   |   |   |   |   |   |-- factor
|   |   |   |   |   |   |   |   |   |-- IDENTIFIER("score")
...
```

### 3. Summary

```
Parse complete: NO syntax errors found.
```

If syntax errors are found, they are reported to `stderr` with the line number and a description, and the summary shows the total error count.

---

## How the Scanner Works

The scanner implements the **DFA (Deterministic Finite Automaton)** derived from the MiniPy lexical specification using the subset construction algorithm.

The main components are:

- **`next_token()`** — the master dispatcher. Looks at the current character and routes to the correct DFA function.

- **`scan_number()`** — implements the INTEGER and FLOAT DFAs. Uses maximal munch: reads digits, then peeks ahead to check for a decimal point to decide INTEGER vs FLOAT.

- **`scan_identifier_or_keyword()`** — reads a full word then checks it against the keyword list. Keywords take priority over identifiers (the priority rule).

- **`scan_string()`** — reads from opening `"` to closing `"`, handling unterminated strings as ERROR tokens.

- **`scan_operator()`** — handles single-character operators and uses one-character lookahead to resolve two-character operators (`==`, `!=`, `<=`, `>=`) via maximal munch.

### Scanning Rules Applied

| Rule                             | Where it applies                                     |
| -------------------------------- | ---------------------------------------------------- |
| **Maximal Munch**                | `<=` matched over `<`, `==` over `=`, `1.5` over `1` |
| **Priority by definition order** | Keywords checked before IDENTIFIER fallback          |
| **Left-to-right scan**           | `pos` index always moves forward, never back         |
| **Catch-all ERROR**              | Any unrecognised character becomes TOKEN_ERROR       |

---

## How the Parser Works

The parser is a **hand-written recursive-descent parser** that implements a predictive LL(1) parsing strategy using a one-token lookahead. It consumes the token stream produced by the scanner and builds a **parse tree** representing the full grammatical structure of the program.

### Grammar

The parser implements the following LL(1) grammar for MiniPy:

```
program -> stmt_list
stmt_list -> stmt stmt_list | ε
stmt -> if_stmt | while_stmt | for_stmt | print_stmt | assign_stmt
assign_stmt -> IDENTIFIER = logical_expr
print_stmt -> PRINT ( logical_expr )
if_stmt -> IF logical_expr : block else_clause
else_clause -> ELSE : block | ε
while_stmt -> WHILE logical_expr : block
for_stmt -> FOR IDENTIFIER IN RANGE ( expr ) : block
block -> stmt stmt_list
logical_expr -> comparison logical_prime
logical_prime -> AND comparison logical_prime | OR comparison logical_prime | ε
comparison -> not_expr comparison_prime
comparison_prime -> EQUALS not_expr | NOT_EQUAL not_expr | LESS_THAN not_expr | GREATER_THAN not_expr | LESS_EQUAL not_expr | GREATER_EQUAL not_expr | ε
not_expr -> NOT not_expr | expr
expr -> term expr_prime
expr_prime -> + term expr_prime | - term expr_prime | ε
term -> factor term_prime
term_prime -> * factor term_prime | / factor term_prime | ε
factor -> INTEGER | FLOAT | STRING | IDENTIFIER | TRUE | FALSE | ( logical_expr ) | - factor
```

The `'` (prime) productions handle left recursion elimination, keeping the grammar suitable for top-down parsing.

### Key Components

- **`parse_file(path)`** — the public entry point. Initialises the scanner, prints the token log, then calls `parse_program()` to build and return the parse tree root.

- **`parse_program()` / `parse_stmt_list()` / `parse_stmt()`** — top-level recursive functions that drive the parse. `parse_stmt_list()` loops while the lookahead is in FIRST(stmt) = `{ IF, WHILE, FOR, PRINT, IDENTIFIER }`.

- **`match(expected)`** — verifies the current token matches the expected type, advances the lookahead, and returns a terminal (leaf) node. On mismatch it records a syntax error and returns an ERROR leaf so the tree remains printable.

- **`peek(type)`** — checks the current lookahead token type without consuming it. Used throughout to make LL(1) branching decisions.

- **`synchronise()`** — panic-mode error recovery. When an unexpected token is encountered, tokens are skipped until a known statement-starting token is found, allowing the parser to continue and report multiple errors in a single run.

### Parse Tree Structure

Each node in the tree is a `ParseNode` with:

| Field         | Description                                                                        |
| ------------- | ---------------------------------------------------------------------------------- |
| `label`       | Non-terminal name (e.g. `"if_stmt"`) or token descriptor (e.g. `KEYWORD_IF("if")`) |
| `is_terminal` | `1` for leaf nodes (tokens), `0` for inner nodes                                   |
| `children[]`  | Up to 8 child pointers                                                             |
| `child_count` | Number of active children                                                          |

The tree is printed with `print_tree()` using indented `|--` connectors, and freed recursively with `free_tree()`.

### Error Handling

The parser uses two layers of error handling:

1. **`match()` errors** — when a specific token is expected but not found, an `ERROR(...)` leaf is inserted into the tree and parsing continues from the same position, so the caller can attempt to recover.
2. **Panic-mode recovery via `synchronise()`** — used at the statement level when a completely unexpected token is encountered. Tokens are discarded until a safe resynchronisation point is reached.

All errors are counted in `parse_error_count` and reported to `stderr` with the offending line number.

---

## Writing Your Own MiniPy Program

Create a plain text file with a `.minipy` extension and write code using these rules:

- Variables: `name = value`
- Arithmetic: `+`, `-`, `*`, `/`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `and`, `or`, `not`
- Control flow: `if`, `else`, `while`, `for ... in range(...)`
- Output: `print("message")` or `print(variable)`
- Comments: anything after `#` on a line is ignored
- Strings must be on a single line enclosed in double quotes `"..."`
- Blocks are delimited by a colon `:` followed by the body statements (indentation is not enforced by the parser, but is recommended for readability)
