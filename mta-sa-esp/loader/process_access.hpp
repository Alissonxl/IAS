#pragma once
#include "win.hpp"

namespace loader {
constexpr DWORD process_access=PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|
                               PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ;
}
