// #define WIN_API

#ifdef WIN_API
#include <windows.h>
#else
#define STD_OUTPUT_HANDLE ((DWORD) - 11)
#define NULL ((void *)0)
typedef void *HANDLE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;

HANDLE GetStdHandle(DWORD nStdHandle);

BOOL WriteConsoleA(
    HANDLE hConsoleOutput,
    const void *lpBuffer,
    DWORD nNumberOfCharsToWrite,
    DWORD *lpNumberOfCharsWritten,
    void *lpReserved);
BOOL SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes);

#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080

int wsprintfA(char *buffer, const char *format, ...);
#endif

HANDLE hConsole;
DWORD bytesWritten;
#define RegularText() SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define SetBgText(bg) SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | (bg))
#define Write(text) WriteConsoleA(hConsole, text, strlen(text), &bytesWritten, NULL);
#define WriteChar(ch) WriteConsoleA(hConsole, ch, strlen(text), &bytesWritten, NULL);

typedef enum TokenType
{
    Unassigned,
    Literal,
    Operator
} TokenType;

typedef struct Token
{
    TokenType type;
    union data
    {
        int literalValue;
        char operator;
    };
} Token;

inline BOOL IsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

inline BOOL IsOperator(char ch)
{
    return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

int Eval(char *code)
{
    Token tokens[255] = {0};
    Token *firstToken = &tokens[0];

    int currentToken = 0;

    while (*code)
    {
        char ch = *code;

        if (IsDigit(ch))
        {
            if (tokens[currentToken].type == Literal)
            {
                tokens[currentToken].literalValue *= 10;
                tokens[currentToken].literalValue += ch - '0';
            }
            else if (tokens[currentToken].type == Unassigned)
            {

                tokens[currentToken].type = Literal;
                tokens[currentToken].literalValue += ch - '0';
            }
            else if (tokens[currentToken].type == Operator)
            {
                currentToken++;

                tokens[currentToken].type = Literal;
                tokens[currentToken].literalValue += ch - '0';
            }
        }
        else if (IsOperator(ch))
        {
            currentToken++;

            tokens[currentToken].type = Operator;
            tokens[currentToken].operator= ch;
        }
        code++;
    }

    if (tokens[1].operator== '+')
        return tokens[0].literalValue + tokens[2].literalValue;
    else if (tokens[1].operator== '-')
        return tokens[0].literalValue - tokens[2].literalValue;
    else
        return 0;
}

void Test(char *code, int expectedResult)
{
    RegularText();
    Write(code);

    int res = Eval(code);

    int width = 20;
    int len = strlen(code);
    while (len < width)
    {
        Write(" ");
        len++;
    }
    Write(" ");

    if (res == expectedResult)
    {

        SetBgText(BACKGROUND_GREEN | FOREGROUND_INTENSITY);
        Write("ok");
    }
    else
    {
        SetBgText(BACKGROUND_RED | FOREGROUND_INTENSITY);
        Write("fail");
        RegularText();
        Write(" (");
        char buf[255] = {0};
        wsprintfA(buf, "%d", res);
        Write(buf);
        Write(")");
    }

    RegularText();
    Write("\n");
}

void mainCRTStartup()
{
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    Write("\n");

    Test("1 + 5", 1 + 5);
    Test("11 + 50", 11 + 50);
    Test("1201 + 5001", 1201 + 5001);
    Test("20-7", 20 - 7);

    ExitProcess(0);
}