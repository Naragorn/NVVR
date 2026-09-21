#pragma once
namespace nvvr::hook {
using Logger=bool(*)(const char*);
void InstallHooks(Logger log);
void Present();
void Report(Logger log);
}
