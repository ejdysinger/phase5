#include <stdio.h>
//#include <sys/varargs.h>
#include <stdarg.h>
#include <usloss.h>

int interrupts_off() {
    unsigned int result;
    int onOff;    // == 1 if interrupts were on, else 0

    result = USLOSS_PsrGet();
    onOff = result & USLOSS_PSR_CURRENT_INT;

    // turn off interrupts if they were on
    if ( onOff == 1 )
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );

    return onOff;
} /* interrupts_off */

void interrupts_on() {

    USLOSS_PsrSet((USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT));

} /* interrupts_on */

void Tconsole(char *fmt, ...)
{
    va_list ap;
    int enabled;
    
    USLOSS_Console("TEST452: ");
    enabled = interrupts_off();
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fflush(stdout);
    va_end(ap);
    if (enabled) {
        interrupts_on();
    }
} /* Tconsole */
