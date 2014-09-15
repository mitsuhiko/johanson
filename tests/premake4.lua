solution "johanson-tests"
	configurations { "debug", "release" }
	location ( "solutions" )
	platforms { "native" }

project "tests"
	language "C"
	kind "ConsoleApp"
	flags { "ExtraWarnings" }
	includedirs {
		"../include",
	}

	files {
		"runtests.c",
	}

	-- IDE specific configuration
	configuration "vs*"
		defines { "_CRT_SECURE_NO_WARNINGS" }

	configuration { "debug", "native" }
		targetname "tests-debug"
		links { "johanson-d" }
		libdirs { "../build/debug/native" }
	configuration { "release", "native" }
		targetname "tests-release"
		links { "johanson" }
		libdirs { "../build/release/native" }
