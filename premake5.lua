workspace "proto"
    configurations { "Debug", "Release" }
	architecture "x64"
	startproject "proto"
    location "bin"
	
project "proto"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files { "src/**.h", "src/**.cpp" }

    filter "configurations:Debug"
        defines { "PR_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "PR_RELEASE" }
        optimize "On"
