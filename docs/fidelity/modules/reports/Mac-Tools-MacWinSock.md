# Mac/Tools/MacWinSock reconciliation

Updated 2026-11-05.

4/4 reconciled: 3 equal, 1 intentional. The selected Apple provider is src/unix/linux_net.c, present in both native targets; there is no MacWinSock.c source body to audit instead.

Mac closesocket 0x282b0 and ioctlsocket 0x282ba tail-call close and ioctl. Native forwards the same descriptors, pointers and return values. FIONBIO equals the legacy 0x8004667e request on Apple; the conditional request translation changes only non-Apple platforms. WSAGetLastError 0x282f6 adds 10000 for errno values 1 through 88. The four explicit native error cases produce those same values with Darwin errno constants; the remaining range logic matches.

Mac WSAStartup 0x282c4 clears a 400-byte i386 WSADATA then writes its version word. Native clears sizeof(WSADATA), including LP64 vendor-pointer storage/padding, and stores the same low requested version before returning zero. That deliberate typed storage adaptation is not a claim that the native object remains 400 bytes.

No source changed. The arm64 Release dedicated target compiled and linked the native socket provider. No socket operation, new test, full CI or replay was run.
