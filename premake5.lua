-- premake5.lua
workspace "cyb-engine"
   architecture "x64"
   startproject "game"
   configurations { "Debug", "Release" }

   -- Setup compiler flags
   flags { "MultiProcessorCompile" }
   floatingpoint "fast"
   staticruntime "on"

   outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
   targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
   objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

   -- Build without the editor. This is not preperly tested and might not work as intended
   -- defines { "NO_EDITOR" }

   filter "system:windows"
      systemversion "latest"
      defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX" }

   filter "configurations:Debug"
      defines { "CYB_DEBUG_BUILD" }

   filter "configurations:Release"
      defines { "CYB_RELEASE_BUILD", "NDEBUG" }
   
   group "dependencies"
	   include "engine/third_party/imgui"
      include "engine/third_party/freetype"
   group ""
   
   include "engine"
   include "game"

