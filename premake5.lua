-- premake5.lua
workspace "cyb-engine"
   architecture "x64"
   startproject "game"

   configurations { "Debug", "Release" }
   flags { "MultiProcessorCompile" }

   outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
   targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

   --[[ Build without the editor. This is not preperly tested and might not
        work as intended yet. ]]
   -- defines { "NO_EDITOR" }

   filter "configurations:Debug"
      defines { "CYB_DEBUG_BUILD" }

   filter "configurations:Release"
      defines { "CYB_RELEASE_BUILD" }

   project "shaders"
	   kind "None"
      files "shaders/*"
   
   group "dependencies"
	   include "cyb-engine/third_party/imgui"
   group ""
   
   include "cyb-engine"
   include "game"

