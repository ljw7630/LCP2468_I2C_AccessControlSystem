#ifndef CONTROLLER_H
#define CONTROLLER_H

void vStartController( unsigned portBASE_TYPE uxPriority );

typedef unsigned long ulong;

ulong outdoorBtnPressed(ulong, ulong);
ulong outdoorFiveSecondsPassed(ulong, ulong);																  
ulong outdoorPasswordApproved(ulong, ulong);
ulong outdoorOpen(ulong, ulong);
ulong outdoorClose(ulong, ulong);

ulong indoorBtnPressed(ulong, ulong);
ulong indoorFiveSecondsPassed(ulong, ulong);
ulong indoorOpen(ulong, ulong);
ulong indoorClose(ulong, ulong);

ulong waitingState(ulong, ulong);
ulong emptyState(ulong, ulong);

void initializeStateMachine(void);

inline void sendToGlobalQueue(ulong state);

#endif
