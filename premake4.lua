solution "Decoda"
    configurations { "Debug", "Release" }
    location "build"
	--debugdir "working"
	flags { "No64BitChecks" }
    
	defines { "_CRT_SECURE_NO_WARNINGS", "WIN32" }
    
    vpaths { 
        ["Header Files"] = { "**.h" },
        ["Source Files"] = { "**.cpp" },
		["Resource Files"] = { "**.rc" },
    }

project "Frontend"
    kind "WindowedApp"
	targetname "Decoda"
	flags { "WinMain" }
    location "build"
    language "C++"
    files {
		"src/Frontend/*.h",
		"src/Frontend/*.cpp",
		"src/Frontend/*.rc",
	}		
    includedirs {
		"src/Shared",
		"libs/wxWidgets/include",
		"libs/wxScintilla/include",
		"libs/Update/include",
	}
	libdirs {
		"libs/wxWidgets/lib/vc_lib",
		"libs/wxScintilla/lib",
		"libs/Update/lib",
	}
    links {
		"comctl32",
		"rpcrt4",
		"imagehlp",
		"Update",
		"Shared",		
	}

    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols" }
        targetdir "bin/debug"
		includedirs { "libs/wxWidgets/lib/vc_lib/mswd" }
		links {
			"wxbase30ud",
			"wxmsw30ud_core",
      "wxmsw30ud_stc",
			"wxmsw30ud_aui",
			"wxscintillad",
			"wxbase30ud_xml",
			"wxexpatd",
			"wxmsw30ud_adv",
			"wxmsw30ud_qa",
			"wxzlibd",
			"wxmsw30ud_richtext",
			"wxmsw30ud_html",
			"wxpngd",
		}

    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize" }
        targetdir "bin/release"
		includedirs { "libs/wxWidgets/lib/vc_lib/msw" }
		links {
			"wxbase28",
			"wxmsw30u_core",
      "wxmsw30u_stc",
			"wxmsw30u_aui",
			"wxscintilla",
			"wxbase30u_xml",
			"wxexpat",
			"wxmsw30u_adv",
			"wxmsw30u_qa",
			"wxzlib",
			"wxmsw30u_richtext",
			"wxmsw30u_html",
			"wxpng",
		}		
		
project "LuaInject"
    kind "SharedLib"
    location "build"
    language "C++"
	defines { "TIXML_USE_STL" }
    files {
		"src/LuaInject/*.h",
		"src/LuaInject/*.cpp",
	}		
    includedirs {
		"src/Shared",
		"libs/LuaPlus/include",
		"libs/tinyxml/include",
		"libs/dbghlp/include",
	}
	libdirs {
		"libs/tinyxml/lib",
		"libs/dbghlp/lib",
	}
    links {
		"Shared",
		"psapi"
	}

    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols" }
        targetdir "bin/debug"
		links { "tinyxmld_STL" }

    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize" }
        targetdir "bin/release"				
		links { "tinyxml_STL" }
		
project "Shared"
    kind "StaticLib"
    location "build"
    language "C++"
    files {
		"src/Shared/*.h",
		"src/Shared/*.cpp",
	}		
    includedirs {
	}
	libdirs {
	}
    links {
	}

    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols" }
        targetdir "bin/debug"

    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize" }
        targetdir "bin/release"		
		