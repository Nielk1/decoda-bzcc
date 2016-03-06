solution "Decoda"
    configurations { "Debug", "Release" }
    location "build"
	--debugdir "working"
	flags { "No64BitChecks" }
    
	defines { "_CRT_SECURE_NO_WARNINGS", "WIN32", "wxUSE_UNICODE" }
    
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
	}
	libdirs {
		"libs/wxWidgets/lib/vc_lib",
	}
    links {
		"comctl32",
		"rpcrt4",
		"imagehlp",
		"Shared",		
	}

    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols", "Unicode" }
        targetdir "bin/debug"
		includedirs { "libs/wxWidgets/lib/vc_lib/mswud" }
		links {
			"wxbase31ud",
			"wxmsw31ud_core",
			"wxmsw31ud_stc",
			"wxmsw31ud_aui",
			"wxscintillad",
			"wxbase31ud_xml",
			"wxexpatd",
			"wxmsw31ud_adv",
			"wxmsw31ud_qa",
			"wxzlibd",
			"wxmsw31ud_richtext",
			"wxmsw31ud_html",
			"wxpngd",
		}

    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize", "Symbols", "Unicode" }
        targetdir "bin/release"
		includedirs { "libs/wxWidgets/lib/vc_lib/mswu" }
		links {
			"wxbase31u",
			"wxmsw31u_core",
			"wxmsw31u_stc",
			"wxmsw31u_aui",
			"wxscintilla",
			"wxbase31u_xml",
			"wxexpat",
			"wxmsw31u_adv",
			"wxmsw31u_qa",
			"wxzlib",
			"wxmsw31u_richtext",
			"wxmsw31u_html",
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
        flags { "Optimize", "Symbols" }
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
		