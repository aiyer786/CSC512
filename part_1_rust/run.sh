#!/usr/bin/bash

CLANG_COMMAND="clang"
if ! command -v CLANG_COMMAND; then
	CLANG_COMMAND="clang-15"
fi

cargo build || exit

test_projects=(
	"test_1.c"
	"test_2.c"
	"test_3.c"
)

function my_exit() {
	echo "ERROR: $1"
	exit 1
}

for i in "${test_projects[@]}"; do
	printf "\n****************************\n"
	printf "\nRunning $i\n"
	$CLANG_COMMAND -O0 -g -fstandalone-debug -fpass-plugin=target/debug/libpart_1_rust.so ../test_files/"$i" || my_exit "Failed to compile $i"
	printf "\nRunning compiled program\n"
	./a.out | tee /dev/tty | grep "}: br_" | sort --unique >dictionary.txt || my_exit "$i exited with $?"
	printf "\ndictionary.txt\n"
	cat dictionary.txt
	valgrind --tool=callgrind --callgrind-out-file=callgrind_output ./a.out &>/dev/null || my_exit "Failed to execute valgrind on $i: Status $?"
	INSTRUCTIONS_EXECUTED="$(grep totals callgrind_output | awk -F: '{ print $2 }')"
	printf "\nInsructions executed:$INSTRUCTIONS_EXECUTED\n"
	rm callgrind_output
done
