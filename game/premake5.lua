project "game"
	kind "WindowedApp"
	language "C++"
	cppdialect "C++17"

	targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"**.h",
		"**.cpp"
	}

	filter "system:windows"
		files { "game.rc" }

	includedirs
	{
		"%{wks.location}/engine/source",
		"%{wks.location}/engine/third_party"
	}
	
	links { "engine" }

	filter "configurations:Debug"
		runtime "Debug"
		optimize "Off"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "on"
