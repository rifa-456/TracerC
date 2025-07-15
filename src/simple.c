#include <stdio.h>
#include <unistd.h>

int main()
{
    while (1)
    {

        printf("Simple test running, PID: %d\n", getpid());
        sleep(1);
        printf("Test complete\n");
    }
    return 0;
}