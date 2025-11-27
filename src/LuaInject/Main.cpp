/*

Decoda
Copyright (C) 2007-2013 Unknown Worlds Entertainment, Inc. 

This file is part of Decoda.

Decoda is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Decoda is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Decoda.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <windows.h>

#include "LuaDll.h"
#include "DebugBackend.h"

HINSTANCE g_hInstance = NULL;
//static bool g_initialized = false;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved)
{

    g_hInstance = hInstance;

    if (reason == DLL_PROCESS_ATTACH)
    {

        // This line can be uncommented to give yourself an opportunity to attach
        // the MSVC debugger to the process being debugged in Decoda to allow
        // LuaInject to be debugged.
        MessageBox(NULL, "Waiting to attach the debugger", NULL, MB_OK);

        // Check for the delay environment variable
        //if (GetEnvironmentVariableA("DELAY_LUAINJECT_INIT", NULL, 0) == 0)
        {
            // No delay requested, initialize immediately
            if (!DebugBackend::Get().Initialize(hInstance))
            {
                return FALSE;
            }
            //g_initialized = true;
        }

    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        DebugBackend::Destroy();
    }

    return TRUE;

}

// Add a helper function to ensure initialization
//void EnsureInitialized()
//{
//    if (!g_initialized)
//    {
//        if (!DebugBackend::Get().Initialize(g_hInstance))
//        {
//            // Handle initialization failure (e.g., log or abort)
//            return;
//        }
//        g_initialized = true;
//    }
//}