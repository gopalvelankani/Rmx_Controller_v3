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
include googletest-release-1.7.0/CMakeFiles/gtest.dir/depend.make

# Include the progress variables for this target.
include googletest-release-1.7.0/CMakeFiles/gtest.dir/progress.make

# Include the compile flags for this target's objects.
include googletest-release-1.7.0/CMakeFiles/gtest.dir/flags.make

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o: googletest-release-1.7.0/CMakeFiles/gtest.dir/flags.make
googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o: ../googletest-release-1.7.0/src/gtest-all.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/mahesh/Music/rmx_mxl_api/pistache-master/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/gtest.dir/src/gtest-all.cc.o -c /home/mahesh/Music/rmx_mxl_api/pistache-master/googletest-release-1.7.0/src/gtest-all.cc

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/gtest.dir/src/gtest-all.cc.i"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/mahesh/Music/rmx_mxl_api/pistache-master/googletest-release-1.7.0/src/gtest-all.cc > CMakeFiles/gtest.dir/src/gtest-all.cc.i

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/gtest.dir/src/gtest-all.cc.s"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/mahesh/Music/rmx_mxl_api/pistache-master/googletest-release-1.7.0/src/gtest-all.cc -o CMakeFiles/gtest.dir/src/gtest-all.cc.s

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.requires:
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.requires

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.provides: googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.requires
	$(MAKE) -f googletest-release-1.7.0/CMakeFiles/gtest.dir/build.make googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.provides.build
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.provides

googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.provides.build: googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o

# Object files for target gtest
gtest_OBJECTS = \
"CMakeFiles/gtest.dir/src/gtest-all.cc.o"

# External object files for target gtest
gtest_EXTERNAL_OBJECTS =

googletest-release-1.7.0/libgtest.a: googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o
googletest-release-1.7.0/libgtest.a: googletest-release-1.7.0/CMakeFiles/gtest.dir/build.make
googletest-release-1.7.0/libgtest.a: googletest-release-1.7.0/CMakeFiles/gtest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library libgtest.a"
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && $(CMAKE_COMMAND) -P CMakeFiles/gtest.dir/cmake_clean_target.cmake
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/gtest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
googletest-release-1.7.0/CMakeFiles/gtest.dir/build: googletest-release-1.7.0/libgtest.a
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/build

googletest-release-1.7.0/CMakeFiles/gtest.dir/requires: googletest-release-1.7.0/CMakeFiles/gtest.dir/src/gtest-all.cc.o.requires
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/requires

googletest-release-1.7.0/CMakeFiles/gtest.dir/clean:
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 && $(CMAKE_COMMAND) -P CMakeFiles/gtest.dir/cmake_clean.cmake
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/clean

googletest-release-1.7.0/CMakeFiles/gtest.dir/depend:
	cd /home/mahesh/Music/rmx_mxl_api/pistache-master/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mahesh/Music/rmx_mxl_api/pistache-master /home/mahesh/Music/rmx_mxl_api/pistache-master/googletest-release-1.7.0 /home/mahesh/Music/rmx_mxl_api/pistache-master/build /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0 /home/mahesh/Music/rmx_mxl_api/pistache-master/build/googletest-release-1.7.0/CMakeFiles/gtest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : googletest-release-1.7.0/CMakeFiles/gtest.dir/depend

