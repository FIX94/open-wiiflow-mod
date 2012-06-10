
//============================================================================
// Name        : usbthread.h
// Copyright   : 2012 FIX94
//============================================================================

#ifndef _USBTHREAD_H_
#define _USBTHREAD_H_

#ifdef __cplusplus
extern "C"
{
#endif

void CreateUSBKeepAliveThread();
void KillUSBKeepAliveThread();

#ifdef __cplusplus
}
#endif

#endif /* _USBTHREAD_H_ */
