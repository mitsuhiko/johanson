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
	for _, cfg in ipairs(configurations()) do
		for _, plat in ipairs(platforms()) do
			configuration { cfg, plat }
				targetdir("build/" .. cfg:lower() .. "/" .. plat:lower())
				objdir("build/objects/" .. cfg:lower() .. "/" .. plat:lower())
			end
		end

project "tests"
	targetname "tests"
	language "C"
	kind "ConsoleApp"
	flags { "ExtraWarnings" }
	includedirs {
		"include"
	}

	files {
		"tests/runtests.c"
	}

	links { "johanson" }

	-- build path settings for all possible configurations
	for _, cfg in ipairs(configurations()) do
		for _, plat in ipairs(platforms()) do
			configuration { cfg, plat }
				targetdir("build/tests/" .. cfg:lower() .. "/" .. plat:lower())
				objdir("build/tests/objects/" .. cfg:lower() .. "/" .. plat:lower())
			end
		end

newoption {
	trigger = "to",
	value = "PATH",
	description = "Set the output location for the generated files"
}
