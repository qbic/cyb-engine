
project "engine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"

	targetdir("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

	VULKAN_SDK = os.getenv("VULKAN_SDK")
	
	files {
		-- cyb-engine source files
		"source/**.h",
		"source/**.cpp",

		-- built-in shaders
		"shaders/*",

		-- staticly linked third party libraries
		"third_party/*.h",
		"third_party/*.hpp",
		"third_party/*.cpp",
		"third_party/*.c",
		"third_party/fmt/*.h",
	}

	includedirs {
		"source",
		"%{VULKAN_SDK}/Include",
		"%{wks.location}/engine/third_party",
		"%{wks.location}/engine/third_party/imgui",
		"%{wks.location}/engine/third_party/freetype/include",
	}

	links {
		"imgui",
		"freetype",
		"comctl32.lib",		-- High DPI awareness interface
	}

	filter "system:windows"
		defines { "VK_USE_PLATFORM_WIN32_KHR" }
   
	-- The debug version of shaderc compiles super slow, only use it over the release version
	-- if debugging the shader compiler is really necessary
	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"
		optimize "Off"		
		links { "%{VULKAN_SDK}/Lib/shaderc_shared.lib" }
		--links {"%{VULKAN_SDK}/Lib/shaderc_sharedd.lib" }

	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "On"
   		buildoptions { "/GT" }
		links { "%{VULKAN_SDK}/Lib/shaderc_shared.lib" }