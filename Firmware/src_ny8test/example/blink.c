// Blink an LED on PB2 (SOT23-6 pin 4, SOP8 pin 5).
#include <ny8.h>

static void delay(unsigned char n)
{
    while (n--) {
        unsigned char i = 250;
        while (i--) __asm__ volatile ("nop");
    }
}

int main(void)
{
    IOSTB = 0x3B;               // PB2 output, others input
    for (;;) {
        PORTB ^= 0x04;
        delay(200);
    }
}
