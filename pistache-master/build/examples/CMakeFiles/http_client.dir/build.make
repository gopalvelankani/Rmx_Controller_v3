# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/user/Music/rmx_mxl_api/pistache-master

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/user/Music/rmx_mxl_api/pistache-master/build

# Include any dependencies generated for this target.
include examples/CMakeFiles/http_client.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/http_client.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/http_client.dir/flags.make

examples/CMakeFiles/http_client.dir/http_client.cc.o: examples/CMakeFiles/http_client.dir/flags.make
examples/CMakeFiles/http_client.dir/http_client.cc.o: ../examples/http_client.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Music/rmx_mxl_api/pistache-master/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object examples/CMakeFiles/http_client.dir/http_client.cc.o"
	cd /home/user/Music/rmx_mxl_api/pistache-master/build/examples && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/http_client.dir/http_client.cc.o -c /home/user/Music/rmx_mxl_api/pistache-master/examples/http_client.cc

examples/CMakeFiles/http_client.dir/http_client.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/http_client.dir/http_client.cc.i"
	cd /home/user/Music/rmx_mxl_api/pistache-master/build/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Music/rmx_mxl_api/pistache-master/examples/http_client.cc > CMakeFiles/http_client.dir/http_client.cc.i

examples/CMakeFiles/http_client.dir/http_client.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/http_client.dir/http_client.cc.s"
	cd /home/user/Music/rmx_mxl_api/pistache-master/build/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Music/rmx_mxl_api/pistache-master/examples/http_client.cc -o CMakeFiles/http_client.dir/http_client.cc.s

examples/CMakeFiles/http_client.dir/http_client.cc.o.requires:

.PHONY : examples/CMakeFiles/http_client.dir/http_client.cc.o.requires

examples/CMakeFiles/http_client.dir/http_client.cc.o.provides: examples/CMakeFiles/http_client.dir/http_client.cc.o.requires
	$(MAKE) -f examples/CMakeFiles/http_client.dir/build.make examples/CMakeFiles/http_client.dir/http_client.cc.o.provides.build
.PHONY : examples/CMakeFiles/http_client.dir/http_client.cc.o.provides

examples/CMakeFiles/http_client.dir/http_client.cc.o.provides.build: examples/CMakeFiles/http_client.dir/http_client.cc.o


# Object files for target http_client
http_client_OBJECTS = \
"CMakeFiles/http_client.dir/http_client.cc.o"

# External object files for target http_client
http_client_EXTERNAL_OBJECTS =

examples/http_client: examples/CMakeFiles/http_client.dir/http_client.cc.o
examples/http_client: examples/CMakeFiles/http_client.dir/build.make
examples/http_client: src/libnet_static.a
examples/http_client: examples/CMakeFiles/http_client.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/user/Music/rmx_mxl_api/pistache-master/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable http_client"
	cd /home/user/Music/rmx_mxl_api/pistache-master/build/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/http_client.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/http_client.dir/build: examples/http_client

.PHONY : examples/CMakeFiles/http_client.dir/build

examples/CMakeFiles/http_client.dir/requires: examples/CMakeFiles/http_client.dir/http_client.cc.o.requires

.PHONY : examples/CMakeFiles/http_client.dir/requires

examples/CMakeFiles/http_client.dir/clean:
	cd /home/user/Music/rmx_mxl_api/pistache-master/build/examples && $(CMAKE_COMMAND) -P CMakeFiles/http_client.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/http_client.dir/clean

examples/CMakeFiles/http_client.dir/depend:
	cd /home/user/Music/rmx_mxl_api/pistache-master/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/user/Music/rmx_mxl_api/pistache-master /home/user/Music/rmx_mxl_api/pistache-master/examples /home/user/Music/rmx_mxl_api/pistache-master/build /home/user/Music/rmx_mxl_api/pistache-master/build/examples /home/user/Music/rmx_mxl_api/pistache-master/build/examples/CMakeFiles/http_client.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/http_client.dir/depend

