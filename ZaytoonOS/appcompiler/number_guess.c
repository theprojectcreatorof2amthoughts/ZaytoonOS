#include "lib.h"

int main(void)
{
    capp_begin("number_guess.capp", "number_guess.capp",
               "Interactive number-guessing game for ZaytoonOS");
    capp_println("Number Guess");
    capp_println("I'm thinking of a number from 1 to 50. Type Q to quit.");
    capp_number_guess();
    capp_exit(0);
    capp_end();
    return 0;
}
