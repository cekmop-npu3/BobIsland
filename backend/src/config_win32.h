#ifndef CONFIG_WIN32_H
#define CONFIG_WIN32_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
int make_dcb(unsigned stop_bits, DCB *dcb);
void make_timeouts(COMMTIMEOUTS *timeouts);
#ifdef __cplusplus
}
#endif
#endif
