// Inside your DllMain, add this:
CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
    // 1. Setup Luau State
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    RegisterAPI(L);

    // 2. Create Named Pipe Server
    HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\WesodaPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024, 1024, 0, NULL);
    
    std::cout << "[+] Internal Pipe Server created. Waiting for external EXE...\n";

    // 3. Listen for the External EXE
    ConnectNamedPipe(hPipe, NULL);
    std::cout << "[+] External EXE connected!\n";

    // 4. Read the script
    char buffer[4096];
    DWORD bytesRead;
    ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
    buffer[bytesRead] = '\0';

    // 5. Execute the script
    ExecuteScript(L, std::string(buffer));

    // 6. Send back a success message
    DWORD bytesWritten;
    WriteFile(hPipe, "Script executed successfully", 27, &bytesWritten, NULL);

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}, nullptr, 0, nullptr);
