#include "config_win32.h"
#include "stream.h"
#include <string.h>

int make_dcb(unsigned stop_bits, DCB *dcb)
{
    if (!dcb || !valid_config(1, stop_bits)) return 0;
    /* Fully specify settings: do not inherit flow control from a previous user. */
    memset(dcb, 0, sizeof(*dcb));
    dcb->DCBlength = sizeof(*dcb);
    dcb->BaudRate = CBR_9600;
    dcb->ByteSize = 8;
    dcb->Parity = NOPARITY;
    dcb->StopBits = stop_bits == 1 ? ONESTOPBIT : TWOSTOPBITS;
    dcb->fBinary = TRUE;
    dcb->fDtrControl = DTR_CONTROL_ENABLE;
    dcb->fRtsControl = RTS_CONTROL_ENABLE;
    dcb->fTXContinueOnXoff = TRUE;
    dcb->fAbortOnError = TRUE;
    dcb->XonChar = 0x11;
    dcb->XoffChar = 0x13;
    dcb->XonLim = 128;
    dcb->XoffLim = 128;
    return 1;
}

void make_timeouts(COMMTIMEOUTS *timeouts)
{
    memset(timeouts, 0, sizeof(*timeouts));
    /* One-byte reads wait up to 100 ms; completed bytes are delivered immediately. */
    timeouts->ReadTotalTimeoutConstant = 100;
    timeouts->WriteTotalTimeoutConstant = 2000;
}
