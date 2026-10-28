#include "lib.h"

int main(void)
{
    capp_begin("zayflight.capp", "zayflight.capp",
               "Interactive text flight simulator for ZaytoonOS");
    capp_println("ZayFlight - ZaytoonOS Flight Simulator");
    capp_println("Take off, climb, turn, and make a safe landing.");
    capp_println("if you die i will point a gun at tut word");
    capp_flight_sim();
    capp_exit(0);
    capp_end();
    return 0;
}
