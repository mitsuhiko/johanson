solution "johanson"
	configurations { "release", "debug" }
	location ( _OPTIONS["to"] )

	if os.is('windows') then
		platforms { "x32" }
	else
		platforms { "native" }
	end

project "johanson"
	targetname "johanson"
	language "C"
	kind "SharedLib"
	flags { "ExtraWarnings" }
	includedirs {
		"include",
	}

	files {
		"src/*.c",
		"src/*.h",
		"include/yajl/*.h",
	}

	if not os.is('windows') then
		buildoptions { "-fvisibility=hidden" }
	end

	-- debug/release configurations
	configuration "debug"
		targetsuffix "-d"
		defines "_DEBUG"
		flags { "Symbols" }

	configuration "release"
		defines "NDEBUG"
		flags { "OptimizeSize" }

	-- IDE specific configuration
	configuration "vs*"
		defines { "_CRT_SECURE_NO_WARNINGS" }

	-- build path settings for all possible configurations
	for _, plat in ipairs(platforms()) do
		configuration { cfg, plat }
			targetdir("build/" .. plat:lower())
			objdir("build/objects/" .. plat:lower())
	end

newoption {
	trigger = "to",
	value = "PATH",
	description = "Set the output location for the generated files"
}
