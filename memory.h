#ifndef MEMORY_H
#define MEMORY_H

unsigned long readFromMemory (int address);

void          writeToMemory  (int address, unsigned long value);

void          initiateMemory ();

#endif // MEMORY_H