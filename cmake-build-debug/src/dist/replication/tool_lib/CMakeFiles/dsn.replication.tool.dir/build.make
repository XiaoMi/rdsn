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
CMAKE_COMMAND = /home/smilencer/Downloads/clion-2019.2.5/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /home/smilencer/Downloads/clion-2019.2.5/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/smilencer/code/pegasus/rdsn

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/smilencer/code/pegasus/rdsn/cmake-build-debug

# Include any dependencies generated for this target.
include src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/depend.make

# Include the progress variables for this target.
include src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/progress.make

# Include the compile flags for this target's objects.
include src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/flags.make

src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o: src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/flags.make
src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o: ../src/dist/replication/tool_lib/mutation_log_tool.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/smilencer/code/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o"
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o -c /home/smilencer/code/pegasus/rdsn/src/dist/replication/tool_lib/mutation_log_tool.cpp

src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.i"
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/smilencer/code/pegasus/rdsn/src/dist/replication/tool_lib/mutation_log_tool.cpp > CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.i

src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.s"
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/smilencer/code/pegasus/rdsn/src/dist/replication/tool_lib/mutation_log_tool.cpp -o CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.s

# Object files for target dsn.replication.tool
dsn_replication_tool_OBJECTS = \
"CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o"

# External object files for target dsn.replication.tool
dsn_replication_tool_EXTERNAL_OBJECTS =

src/dist/replication/tool_lib/libdsn.replication.tool.a: src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/mutation_log_tool.cpp.o
src/dist/replication/tool_lib/libdsn.replication.tool.a: src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/build.make
src/dist/replication/tool_lib/libdsn.replication.tool.a: src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/smilencer/code/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libdsn.replication.tool.a"
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && $(CMAKE_COMMAND) -P CMakeFiles/dsn.replication.tool.dir/cmake_clean_target.cmake
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dsn.replication.tool.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/build: src/dist/replication/tool_lib/libdsn.replication.tool.a

.PHONY : src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/build

src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/clean:
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib && $(CMAKE_COMMAND) -P CMakeFiles/dsn.replication.tool.dir/cmake_clean.cmake
.PHONY : src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/clean

src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/depend:
	cd /home/smilencer/code/pegasus/rdsn/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/smilencer/code/pegasus/rdsn /home/smilencer/code/pegasus/rdsn/src/dist/replication/tool_lib /home/smilencer/code/pegasus/rdsn/cmake-build-debug /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib /home/smilencer/code/pegasus/rdsn/cmake-build-debug/src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/dist/replication/tool_lib/CMakeFiles/dsn.replication.tool.dir/depend

