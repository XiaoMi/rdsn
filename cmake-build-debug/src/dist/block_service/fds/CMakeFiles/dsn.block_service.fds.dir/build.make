# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

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
CMAKE_COMMAND = /snap/clion/99/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /snap/clion/99/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/mi/workspace/pegasus/rdsn

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/mi/workspace/pegasus/rdsn/cmake-build-debug

# Include any dependencies generated for this target.
include src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/depend.make

# Include the progress variables for this target.
include src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/progress.make

# Include the compile flags for this target's objects.
include src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/flags.make

src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o: src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/flags.make
src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o: ../src/dist/block_service/fds/fds_service.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/block_service/fds/fds_service.cpp

src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/block_service/fds/fds_service.cpp > CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.i

src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/block_service/fds/fds_service.cpp -o CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.s

# Object files for target dsn.block_service.fds
dsn_block_service_fds_OBJECTS = \
"CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o"

# External object files for target dsn.block_service.fds
dsn_block_service_fds_EXTERNAL_OBJECTS =

src/dist/block_service/fds/libdsn.block_service.fds.a: src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/fds_service.cpp.o
src/dist/block_service/fds/libdsn.block_service.fds.a: src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/build.make
src/dist/block_service/fds/libdsn.block_service.fds.a: src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libdsn.block_service.fds.a"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && $(CMAKE_COMMAND) -P CMakeFiles/dsn.block_service.fds.dir/cmake_clean_target.cmake
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dsn.block_service.fds.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/build: src/dist/block_service/fds/libdsn.block_service.fds.a

.PHONY : src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/build

src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/clean:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds && $(CMAKE_COMMAND) -P CMakeFiles/dsn.block_service.fds.dir/cmake_clean.cmake
.PHONY : src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/clean

src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/depend:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mi/workspace/pegasus/rdsn /home/mi/workspace/pegasus/rdsn/src/dist/block_service/fds /home/mi/workspace/pegasus/rdsn/cmake-build-debug /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/dist/block_service/fds/CMakeFiles/dsn.block_service.fds.dir/depend

