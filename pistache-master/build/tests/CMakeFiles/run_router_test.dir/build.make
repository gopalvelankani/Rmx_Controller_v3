# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.2

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
CMAKE_SOURCE_DIR = /home/mahesh/Music/rmx_mxl_api/pistache-master

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/mahesh/Music/rmx_mxl_api/pistache-master/build

# Include any dependencies generated for this target.
include tests/CMakeFiles/run_router_test.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/run_router_test.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/run_router_test.dir/flags.make

tests/CMakeFiles/run_router_test.dir/router_test.cc.o: tests/CMakeFiles/run_router_test.dir/flags.make
tests/CMakeFiles/run_router_test.dir/router_test.cc.o: ../tests/router_test.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/mahesh/Music/rmx_mxl_api/pistache-master/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object tests/CMakeFiles/run_router_test.dir/router_test.cc.o"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/run_router_test.dir/router_test.cc.o -c /home/mahesh/Music/rmx_mxl_api/pistache-master/tests/router_test.cc

tests/CMakeFiles/run_router_test.dir/router_test.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/run_router_test.dir/router_test.cc.i"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/mahesh/Music/rmx_mxl_api/pistache-master/tests/router_test.cc > CMakeFiles/run_router_test.dir/router_test.cc.i

tests/CMakeFiles/run_router_test.dir/router_test.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/run_router_test.dir/router_test.cc.s"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/mahesh/Music/rmx_mxl_api/pistache-master/tests/router_test.cc -o CMakeFiles/run_router_test.dir/router_test.cc.s

tests/CMakeFiles/run_router_test.dir/router_test.cc.o.requires:
.PHONY : tests/CMakeFiles/run_router_test.dir/router_test.cc.o.requires

tests/CMakeFiles/run_router_test.dir/router_test.cc.o.provides: tests/CMakeFiles/run_router_test.dir/router_test.cc.o.requires
	$(MAKE) -f tests/CMakeFiles/run_router_test.dir/build.make tests/CMakeFiles/run_router_test.dir/router_test.cc.o.provides.build
.PHONY : tests/CMakeFiles/run_router_test.dir/router_test.cc.o.provides

tests/CMakeFiles/run_router_test.dir/router_test.cc.o.provides.build: tests/CMakeFiles/run_router_test.dir/router_test.cc.o

# Object files for target run_router_test
run_router_test_OBJECTS = \
"CMakeFiles/run_router_test.dir/router_test.cc.o"

# External object files for target run_router_test
run_router_test_EXTERNAL_OBJECTS =

tests/run_router_test: tests/CMakeFiles/run_router_test.dir/router_test.cc.o
tests/run_router_test: tests/CMakeFiles/run_router_test.dir/build.make
tests/run_router_test: googletest-release-1.7.0/libgtest.a
tests/run_router_test: googletest-release-1.7.0/libgtest_main.a
tests/run_router_test: src/libnet_static.a
tests/run_router_test: googletest-release-1.7.0/libgtest.a
tests/run_router_test: tests/CMakeFiles/run_router_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable run_router_test"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/run_router_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/run_router_test.dir/build: tests/run_router_test
.PHONY : tests/CMakeFiles/run_router_test.dir/build

tests/CMakeFiles/run_router_test.dir/requires: tests/CMakeFiles/run_router_test.dir/router_test.cc.o.requires
.PHONY : tests/CMakeFiles/run_router_test.dir/requires

tests/CMakeFiles/run_router_test.dir/clean:
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests && $(CMAKE_COMMAND) -P CMakeFiles/run_router_test.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/run_router_test.dir/clean

tests/CMakeFiles/run_router_test.dir/depend:
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mahesh/Music/rmx_mxl_api/pistache-master /home/mahesh/Music/rmx_mxl_api/pistache-master/tests /home/mahesh/Music/rmx_mxl_api/pistache-master/build /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests /home/mahesh/Music/rmx_mxl_api/pistache-master/build/tests/CMakeFiles/run_router_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/run_router_test.dir/depend

