# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
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
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vipul/ossim-svn/src/ossim_package_support/cmake

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vipul/ossim-svn/src/ossim_package_support/cmake/build

# Include any dependencies generated for this target.
include ossim/src/test/CMakeFiles/ossim-foo.dir/depend.make

# Include the progress variables for this target.
include ossim/src/test/CMakeFiles/ossim-foo.dir/progress.make

# Include the compile flags for this target's objects.
include ossim/src/test/CMakeFiles/ossim-foo.dir/flags.make

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o: ossim/src/test/CMakeFiles/ossim-foo.dir/flags.make
ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o: /home/vipul/ossim-svn/src/ossim/src/test/ossim-foo.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o"
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o -c /home/vipul/ossim-svn/src/ossim/src/test/ossim-foo.cpp

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ossim-foo.dir/ossim-foo.cpp.i"
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/vipul/ossim-svn/src/ossim/src/test/ossim-foo.cpp > CMakeFiles/ossim-foo.dir/ossim-foo.cpp.i

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ossim-foo.dir/ossim-foo.cpp.s"
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/vipul/ossim-svn/src/ossim/src/test/ossim-foo.cpp -o CMakeFiles/ossim-foo.dir/ossim-foo.cpp.s

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.requires:
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.requires

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.provides: ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.requires
	$(MAKE) -f ossim/src/test/CMakeFiles/ossim-foo.dir/build.make ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.provides.build
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.provides

ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.provides.build: ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o

# Object files for target ossim-foo
ossim__foo_OBJECTS = \
"CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o"

# External object files for target ossim-foo
ossim__foo_EXTERNAL_OBJECTS =

bin/ossim-foo: ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o
bin/ossim-foo: lib/libossim.so.1.8.12
bin/ossim-foo: /usr/lib/libOpenThreads.so
bin/ossim-foo: /usr/lib/libjpeg.so
bin/ossim-foo: /usr/lib/libtiff.so
bin/ossim-foo: /usr/lib/libgeotiff.so
bin/ossim-foo: /usr/lib/libOpenThreads.so
bin/ossim-foo: /usr/lib/libfreetype.so
bin/ossim-foo: /usr/lib64/openmpi/lib/libmpi.so
bin/ossim-foo: /usr/lib/libz.so
bin/ossim-foo: /usr/lib/libdl.so
bin/ossim-foo: ossim/src/test/CMakeFiles/ossim-foo.dir/build.make
bin/ossim-foo: ossim/src/test/CMakeFiles/ossim-foo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable ../../../bin/ossim-foo"
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ossim-foo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
ossim/src/test/CMakeFiles/ossim-foo.dir/build: bin/ossim-foo
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/build

ossim/src/test/CMakeFiles/ossim-foo.dir/requires: ossim/src/test/CMakeFiles/ossim-foo.dir/ossim-foo.cpp.o.requires
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/requires

ossim/src/test/CMakeFiles/ossim-foo.dir/clean:
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test && $(CMAKE_COMMAND) -P CMakeFiles/ossim-foo.dir/cmake_clean.cmake
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/clean

ossim/src/test/CMakeFiles/ossim-foo.dir/depend:
	cd /home/vipul/ossim-svn/src/ossim_package_support/cmake/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vipul/ossim-svn/src/ossim_package_support/cmake /home/vipul/ossim-svn/src/ossim/src/test /home/vipul/ossim-svn/src/ossim_package_support/cmake/build /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test /home/vipul/ossim-svn/src/ossim_package_support/cmake/build/ossim/src/test/CMakeFiles/ossim-foo.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : ossim/src/test/CMakeFiles/ossim-foo.dir/depend

