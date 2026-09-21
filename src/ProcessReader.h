#pragma once
#include "EngineProbe.h"
namespace nvvr::probe {
bool CopyCurrentProcess(void*, Address address, void* output, std::size_t size);
}
