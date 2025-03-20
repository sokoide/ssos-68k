#include <stdio.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

extern void ssosmain();
extern void interrupts();

int main() {
    int ssp = _iocs_b_super(0); // enter supervisor mode
                                //
    interrupts();
    ssosmain();

    _iocs_b_super(ssp); // leave supervisor mode
    return 0;
}
