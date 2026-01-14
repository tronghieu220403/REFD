#include "mutex.h"

void Mutex::Create()
{
    KeInitializeMutex(&mutex_, 0);
    return;
}

void Mutex::Lock()
{
    KeWaitForSingleObject(
        &mutex_,
        Executive,
        KernelMode,
        FALSE,
        NULL
    );
}

void Mutex::Unlock()
{
    KeReleaseMutex(&mutex_, FALSE);
}

bool Mutex::Trylock()
{
    LARGE_INTEGER zero = {};
    NTSTATUS st = KeWaitForSingleObject(
        &mutex_,
        Executive,
        KernelMode,
        FALSE,
        &zero
    );
    return (st == STATUS_SUCCESS);
}


