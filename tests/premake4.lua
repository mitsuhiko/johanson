solution "johanson-tests"
	configurations { "debug", "release" }
	location ( "solutions" )
	platforms { "native" }

project "parsing-tests"
	language "C"
	kind "ConsoleApp"
	flags { "ExtraWarnings" }
	includedirs {
		"../include",
	}

	files {
		"run-parsing-tests.c",
	}

	-- IDE specific configuration
	configuration "vs*"
		defines { "_CRT_SECURE_NO_WARNINGS" }

	configuration { "debug", "native" }
		targetname "parsing-tests-debug"
		links { "johanson-d" }
		libdirs { "../build/native" }
	configuration { "release", "native" }
		targetname "parsing-tests-release"
		links { "johanson" }
		libdirs { "../build/native" }
