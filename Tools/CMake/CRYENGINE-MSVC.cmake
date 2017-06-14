
set(MSVC_COMMON_FLAGS 
	/nologo		# Don't show version info
	/W3 		# Enable warning level 3
	/fp:fast 	# Use fast floating point precision model
	/Zc:wchar_t	# Parse wchar_t as internal type
	/GF 		# Eliminate Duplicate Strings
	/Gy		# Enable function level linking
	/GR- 		# Disable RTTI
	/utf-8		# Set source and execution charset to utf-8
	/Wv:18		# Disable warnings until SDK depedencies switch to UTF-8/ASCII.
	/MP
	/bigobj

	/WX  #Treat warnings as errors
)
string(REPLACE ";" " " MSVC_COMMON_FLAGS "${MSVC_COMMON_FLAGS}")
 
# Override cxx flags
set(CMAKE_CXX_FLAGS "${MSVC_COMMON_FLAGS}" CACHE STRING "C++ Common Flags" FORCE)

set(CMAKE_C_FLAGS_DEBUG "/MDd /Zi /Zo /Od /Ob0 /Oy- /RTC1 /GS /DDEBUG /D_DEBUG" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Zo /Od /Ob0 /Oy- /RTC1 /GS /DDEBUG /D_DEBUG" CACHE STRING "C++ Flags" FORCE)

# Create PDBs (/Zi)
# Create better debug info (/Zo)
# Enable full optimization (/Ox) Same as /Ob2 /Oi /Ot /Oy
# Don't omit frame pointer (/Oy-)
# Disable buffer security checks (/GS-)
set(CMAKE_C_FLAGS_PROFILE "/Zi /Zo /MD /Ox /Oy- /GS- /DNDEBUG /D_PROFILE" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_PROFILE "/Zi /Zo /MD /Ox /Oy- /GS- /DNDEBUG /D_PROFILE" CACHE STRING "C++ Flags" FORCE)

set(CMAKE_C_FLAGS_RELEASE "/MD /Ox /GS- /DNDEBUG /D_RELEASE /DPURE_CLIENT" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "/MD /Ox /GS- /DNDEBUG /D_RELEASE /DPURE_CLIENT" CACHE STRING "C++ Flags" FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_PROFILE "/debug" CACHE STRING "Profile link flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE    "/debug" CACHE STRING "Profile link flags" FORCE)

function (wrap_whole_archive target source)
	# no-op for MSVC (for now)
	set(${target} "${${source}}" PARENT_SCOPE)
endfunction()

