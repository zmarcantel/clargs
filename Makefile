OUT=testbed
CXXFLAGS=   -Wall \
			-Wextra \
			-Werror \
			-pedantic \
			-Wno-unused-parameter \
			-g \
			-fverbose-asm \
			-fPIC \
			-Wabi \
			-std=c++11 \
			-fno-inline \
			-fno-omit-frame-pointer \
			-fno-optimize-sibling-calls

default: $(OUT)

$(OUT):
	@clang++ $(CXXFLAGS) -o $(OUT) tests.cpp

run: $(OUT)
	@testbed -h \
			-m 100 \
			--word-size 128 --word-aligned \
			-vvvvvvv \
			-o amer.ica \
			-o fuck.yeah \
			-w \
			-s 1 -s 2 -s 3 -s 4 -s 5 -s 6 -s 7 -s 8 -s 9 \
			--vendor-id abcd:123:xyz \
			-W all -W abi -W inline \
			parse one two three

.PHONY: run
