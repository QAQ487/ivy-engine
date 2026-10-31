/*==============================================================================

   System timer [system_timer.h]
														 Author : Terry Yeung
														 Date   : 2018/06/17
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SYSTEM_TIMER_H_
#define SYSTEM_TIMER_H_

/*------------------------------------------------------------------------------
   Function prototypes
------------------------------------------------------------------------------*/

// Initialize the system timer
void SystemTimer_Initialize(void);

// Reset the system timer
void SystemTimer_Reset(void);

// Start the system timer
void SystemTimer_Start(void);

// Stop the system timer
void SystemTimer_Stop(void);

// Advance the system timer by 0.1 seconds
void SystemTimer_Advance(void);

// Get measured time
double SystemTimer_GetTime(void);

// Get the current (absolute) time
double SystemTimer_GetAbsoluteTime(void);

// Get elapsed time
float SystemTimer_GetElapsedTime(void);

// Is the system timer stopped?
bool SystemTimer_IsStoped(void);

// Pin the current thread to a single processor
void LimitThreadAffinityToCurrentProc(void);

#endif // SYSTEM_TIMER_H_
