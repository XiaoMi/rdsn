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
include src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/depend.make

# Include the progress variables for this target.
include src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/progress.make

# Include the compile flags for this target's objects.
include src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/flags.make

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/flags.make
src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o: ../src/dist/cli/shell/cli.main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli.main.cpp

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli.main.cpp > CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.i

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli.main.cpp -o CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.s

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/flags.make
src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o: ../src/dist/cli/shell/cli_app.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli_app.cpp

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli_app.cpp > CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.i

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell/cli_app.cpp -o CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.s

# Object files for target dsn_cli_shell
dsn_cli_shell_OBJECTS = \
"CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o" \
"CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o"

# External object files for target dsn_cli_shell
dsn_cli_shell_EXTERNAL_OBJECTS =

src/dist/cli/shell/dsn_cli_shell: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli.main.cpp.o
src/dist/cli/shell/dsn_cli_shell: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/cli_app.cpp.o
src/dist/cli/shell/dsn_cli_shell: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/build.make
src/dist/cli/shell/dsn_cli_shell: src/dist/cli/libdsn_cli.a
src/dist/cli/shell/dsn_cli_shell: src/core/libdsn_runtime.a
src/dist/cli/shell/dsn_cli_shell: ../thirdparty/output/lib/libthrift.a
src/dist/cli/shell/dsn_cli_shell: ../thirdparty/output/lib/libfmt.a
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libboost_system.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libboost_filesystem.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/librt.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libaio.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libdl.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libcrypto.so
src/dist/cli/shell/dsn_cli_shell: /usr/lib/x86_64-linux-gnu/libboost_system.so
src/dist/cli/shell/dsn_cli_shell: src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable dsn_cli_shell"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dsn_cli_shell.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/build: src/dist/cli/shell/dsn_cli_shell

.PHONY : src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/build

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/clean:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell && $(CMAKE_COMMAND) -P CMakeFiles/dsn_cli_shell.dir/cmake_clean.cmake
.PHONY : src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/clean

src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/depend:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mi/workspace/pegasus/rdsn /home/mi/workspace/pegasus/rdsn/src/dist/cli/shell /home/mi/workspace/pegasus/rdsn/cmake-build-debug /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/dist/cli/shell/CMakeFiles/dsn_cli_shell.dir/depend

