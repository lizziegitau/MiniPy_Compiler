# MiniPy Scanner

## What is this?

This is a **lexical analyser (scanner)** for **MiniPy** — a mini subset of the Python programming language.

The scanner reads a MiniPy source file and breaks it down into tokens — the smallest meaningful units of the language. For each token it prints the line number, token type, and the actual characters matched (the lexeme).

The scanner is built **from scratch in C**, directly implementing a Deterministic Finite Automata (DFAs) derived from MiniPy's lexical specification.

---

## Project Structure

```
MiniPy Scanner/
│
├── scanner.h        # Token type definitions (enum + struct)
├── scanner.c        # Full scanner implementation
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

3. Compile the scanner:

   ```
   gcc scanner.c -o scanner
   ```

   This produces an executable file called `scanner.exe` (Windows) or `scanner` (Mac/Linux).

   If compilation is successful, you will see no output — just your prompt returning. If you see error messages, check that both `scanner.h` and `scanner.c` are in the same folder.

---

## How to Run

### On Windows

```
.\scanner.exe sample1.minipy
```

or

```
scanner.exe sample1.minipy
```

### On Mac / Linux

```
./scanner sample1.minipy
```

Replace `sample1.minipy` with any `.minipy` file you want to scan.

---

## Example Output

Given this MiniPy source code in `sample1.minipy`:

```python
score = 85
if score >= 50:
    print("Student has passed")
```

The scanner produces:

```
Line      Token Type       Lexeme
---------------------------------------------
Line 1    IDENTIFIER       score
Line 1    ASSIGN           =
Line 1    INTEGER          85
Line 2    IF               if
Line 2    IDENTIFIER       score
Line 2    GREATER_EQUAL    >=
Line 2    INTEGER          50
Line 2    COLON            :
Line 3    PRINT            print
Line 3    LPAREN           (
Line 3    STRING           "Student has passed"
Line 3    RPAREN           )
```

---

## How the Scanner Works

The scanner implements the **DFA (Deterministic Finite Automaton)** derived from the MiniPy lexical specification using the subset construction algorithm.

The main components are:

- **`next_token()`** — the master dispatcher. Looks at the current character and routes to the correct DFA function. This corresponds to the master start state `q_start` in the combined DFA.

- **`scan_number()`** — implements the INTEGER and FLOAT DFAs. Uses maximal munch: reads digits, then peeks ahead to check for a decimal point to decide INTEGER vs FLOAT.

- **`scan_identifier_or_keyword()`** — implements the IDENTIFIER and KEYWORD DFAs combined. Reads a full word then checks it against the keyword list. Keywords take priority over identifiers (the priority rule).

- **`scan_string()`** — implements the STRING DFA. Reads from opening `"` to closing `"`, handling unterminated strings as ERROR tokens.

- **`scan_operator()`** — implements the OPERATOR DFA. Handles single-character operators directly and uses one-character lookahead (peek) to resolve two-character operators (`==`, `!=`, `<=`, `>=`) via maximal munch.

### Scanning Rules Applied

| Rule                             | Where it applies                                     |
| -------------------------------- | ---------------------------------------------------- |
| **Maximal Munch**                | `<=` matched over `<`, `==` over `=`, `1.5` over `1` |
| **Priority by definition order** | Keywords checked before IDENTIFIER fallback          |
| **Left-to-right scan**           | `pos` index always moves forward, never back         |
| **Catch-all ERROR**              | Any unrecognised character becomes TOKEN_ERROR       |

---

## Writing Your Own MiniPy Program

Create a plain text file with a `.minipy` extension and write code using these rules:

- Variables: `name = value`
- Arithmetic: `+`, `-`, `*`, `/`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `and`, `or`, `not`
- Control flow: `if`, `else`, `while`, `for ... in range(...)`
- Output: `print("message")`
- Comments: anything after `#` on a line is ignored
- Strings must be on a single line enclosed in double quotes `"..."`
