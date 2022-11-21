
project "cyb-engine"
	kind "StaticLib"
   	language "C++"
   	cppdialect "C++17"
   	staticruntime "off"

   	targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

	VULKAN_SDK = os.getenv("VULKAN_SDK")
	
   	files
	{
		"source/**.h",
		"source/**.cpp",
		"third_party/*.h",
		"third_party/*.cpp",
		"third_party/*.c",
		"third_party/fmt/*.h",
   	}

	includedirs
	{
		"source",
		"%{VULKAN_SDK}/Include",
		"%{wks.location}/cyb-engine/third_party",
		"%{wks.location}/cyb-engine/third_party/imgui",
	}

	links
	{
		"imgui",
		"comctl32.lib",
	}

	filter "system:windows"
		systemversion "latest"
		defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR" }
   
   	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"
		optimize "off"
		
		--[[ The debug version of shaderc is really slow, only use it over the
			 release version if debugging the shader compiler. ]]
		links { "%{VULKAN_SDK}/Lib/shaderc_shared.lib" }
		--links {"%{VULKAN_SDK}/Lib/shaderc_sharedd.lib" }

   	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "off"
		links
		{
			"%{VULKAN_SDK}/Lib/shaderc_shared.lib",
		}


