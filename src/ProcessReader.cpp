#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ProcessReader.h"
namespace nvvr::probe {
bool CopyCurrentProcess(void*, Address address, void* output, std::size_t size) {
    SIZE_T copied = 0;
    const BOOL ok = ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied);
    return ok != FALSE && copied == size;
}
}
