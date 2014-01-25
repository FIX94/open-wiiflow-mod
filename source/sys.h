
#ifndef _SYS_H_
#define _SYS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Prototypes */
void Sys_Init(void);
bool Sys_Exiting(void);

void Input_Reset(void);
void Open_Inputs(void);
void Close_Inputs(void);

/* All our extern C stuff */
extern void __exception_setreload(int t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
