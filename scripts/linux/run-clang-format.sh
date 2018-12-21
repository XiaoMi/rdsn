#!/bin/bash

# How to use this hook?
# ln -s hooks/pre-commit .git/hooks/
# In case hook is not executable
# chmod +x .git/hooks/pre-commit

find src include -name "*.h" -or -name "*.cpp" | xargs clang-format-3.9 -i

effected_files=$(git status -s)
echo "Checking for files that need clang-format..."
if [ -z "${effected_files}" ]; then
	echo "All files are well formatted"
	exit 0
else
	echo "Please check if the following files are well formatted:"
	echo "${effected_files}"
	exit 1
fi
