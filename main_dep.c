void Crash()
{
    int *p = 0;
    *p = 42; // Intentional crash
}

void Foo()
{
    Crash();
}