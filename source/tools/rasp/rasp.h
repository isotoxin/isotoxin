#pragma once

extern HANDLE hConsoleOutput;
extern CONSOLE_SCREEN_BUFFER_INFO csbi;
extern void(*PrintCustomHandler)(int color, const ts::asptr&);

void Print(WORD color, const char *format, ...);
void Print(const char *format, ...);


struct UnusedPrinter
{
    const char *what;
    bool wasUnused;

    void operator()(const char *str)
    {
        if (!wasUnused) { wasUnused = true; Print(FOREGROUND_RED, "Unused %s:\n", what); }
        Print(" %s\n", str);
    }

    ~UnusedPrinter()
    {
        if (wasUnused) Print("-------------\n");
        else Print(FOREGROUND_GREEN, "No unused %s!\n", what);
    }
};
