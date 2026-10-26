#include "lib.h"

int main(void)
{
    capp_begin("commandline.capp", "commandline.capp",
               "ZaytoonOS commandline demo — packaged .capp");
    capp_println("commandline.capp  v1.0");
    capp_println("─────────────────────────────────────────");
    capp_println("This is a packaged .capp application.");
    capp_println("It was compiled from C source and embedded");
    capp_println("into the kernel image as a byte array.");
    capp_println("");
    capp_print("Arguments passed: ");
    capp_print_args();
    capp_print("Uptime when launched: ");
    capp_print_ticks();
    capp_println("─────────────────────────────────────────");
    capp_println("The interactive C shell is shell.capp.");
    capp_println("Run it directly from the ZaytoonOS prompt.");
    capp_exit(0);
    capp_end();
    return 0;
}
