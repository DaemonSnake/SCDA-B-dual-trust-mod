// Single player build: the network stack reports no cable and every socket call fails
#include "xdk/hle.h"

HLE_STDCALL(00465FE2, XNetStartup, 1) { return 0; }
HLE_STDCALL(00465FF9, WSAStartup, 2) {
    if (a[1]) std::memset(kernel::HostPtr(a[1]), 0, 400);
    return 0;
}
HLE_STDCALL(00050488, XNetGetEthernetLinkStatus, 0) { return 0; }
HLE_STDCALL(00474F5C, socket, 3) { return 0xFFFFFFFF; }
HLE_STDCALL(00473EA0, bind, 3) { return 0xFFFFFFFF; }
HLE_STDCALL(00473F24, connect, 3) { return 0xFFFFFFFF; }
HLE_STDCALL(00473350, ioctlsocket, 3) { return 0xFFFFFFFF; }
HLE_STDCALL(00478F54, XOnlineStartup, 1) { return 0x80004005; }
