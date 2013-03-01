#ifndef SENSORS_H
#define SENSORS_H

void vStartSensors( unsigned portBASE_TYPE uxPriority );
unsigned char setLightOn(int index);
unsigned char setLightOff(int index);
void putLights(unsigned char lights);

#endif
