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
include src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/depend.make

# Include the progress variables for this target.
include src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/progress.make

# Include the compile flags for this target's objects.
include src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o: ../src/dist/replication/zookeeper/distributed_lock_service_zookeeper.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/distributed_lock_service_zookeeper.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/distributed_lock_service_zookeeper.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/distributed_lock_service_zookeeper.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.s

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o: ../src/dist/replication/zookeeper/lock_struct.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/lock_struct.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/lock_struct.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/lock_struct.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.s

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o: ../src/dist/replication/zookeeper/meta_state_service_zookeeper.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/meta_state_service_zookeeper.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/meta_state_service_zookeeper.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/meta_state_service_zookeeper.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.s

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o: ../src/dist/replication/zookeeper/zookeeper_error.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_error.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_error.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_error.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.s

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o: ../src/dist/replication/zookeeper/zookeeper_session.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.s

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/flags.make
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o: ../src/dist/replication/zookeeper/zookeeper_session_mgr.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && ccache /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o -c /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session_mgr.cpp

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.i"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session_mgr.cpp > CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.i

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.s"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper/zookeeper_session_mgr.cpp -o CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.s

# Object files for target dsn.replication.zookeeper_provider
dsn_replication_zookeeper_provider_OBJECTS = \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o" \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o" \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o" \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o" \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o" \
"CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o"

# External object files for target dsn.replication.zookeeper_provider
dsn_replication_zookeeper_provider_EXTERNAL_OBJECTS =

src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/distributed_lock_service_zookeeper.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/lock_struct.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/meta_state_service_zookeeper.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_error.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/zookeeper_session_mgr.cpp.o
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/build.make
src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a: src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/mi/workspace/pegasus/rdsn/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Linking CXX static library libdsn.replication.zookeeper_provider.a"
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && $(CMAKE_COMMAND) -P CMakeFiles/dsn.replication.zookeeper_provider.dir/cmake_clean_target.cmake
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dsn.replication.zookeeper_provider.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/build: src/dist/replication/zookeeper/libdsn.replication.zookeeper_provider.a

.PHONY : src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/build

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/clean:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper && $(CMAKE_COMMAND) -P CMakeFiles/dsn.replication.zookeeper_provider.dir/cmake_clean.cmake
.PHONY : src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/clean

src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/depend:
	cd /home/mi/workspace/pegasus/rdsn/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mi/workspace/pegasus/rdsn /home/mi/workspace/pegasus/rdsn/src/dist/replication/zookeeper /home/mi/workspace/pegasus/rdsn/cmake-build-debug /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper /home/mi/workspace/pegasus/rdsn/cmake-build-debug/src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/dist/replication/zookeeper/CMakeFiles/dsn.replication.zookeeper_provider.dir/depend

