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
include src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/depend.make

# Include the progress variables for this target.
include src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/progress.make

# Include the compile flags for this target's objects.
include src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/flags.make

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.o: src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/flags.make
src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.o: ../src/core/tools/hpc/hpc_task_queue.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/hpc_task_queue.cpp

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/hpc_task_queue.cpp > CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.i

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/hpc_task_queue.cpp -o CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.s

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.o: src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/flags.make
src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.o: ../src/core/tools/hpc/providers.hpc.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/providers.hpc.cpp

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/providers.hpc.cpp > CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.i

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc/providers.hpc.cpp -o CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.s

dsn.tools.hpc: src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/hpc_task_queue.cpp.o
dsn.tools.hpc: src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/providers.hpc.cpp.o
dsn.tools.hpc: src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/build.make

.PHONY : dsn.tools.hpc

# Rule to build all files generated by this target.
src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/build: dsn.tools.hpc

.PHONY : src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/build

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/clean:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc && $(CMAKE_COMMAND) -P CMakeFiles/dsn.tools.hpc.dir/cmake_clean.cmake
.PHONY : src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/clean

src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/depend:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mi/workspace/pegasus/rdsn /home/mi/workspace/pegasus/rdsn/src/core/tools/hpc /home/mi/workspace/pegasus/rdsn/cmake-build-debug /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/core/tools/hpc/CMakeFiles/dsn.tools.hpc.dir/depend

