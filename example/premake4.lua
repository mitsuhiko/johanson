solution "johanson-example"
	configurations { "release" }
	location ( "solutions" )
	platforms { "native" }

project "example"
	targetname "example"
	language "C"
	kind "ConsoleApp"
	flags { "ExtraWarnings" }
	includedirs {
		"../include",
	}

	files {
		"*.c",
	}

	links { "johanson" }
	libdirs { "../build/release/native" }

	-- IDE specific configuration
	configuration "vs*"
		defines { "_CRT_SECURE_NO_WARNINGS" }
