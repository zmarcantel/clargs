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
			-fno-optimize-sibling-calls \
			-fno-rtti


TESTARGS=   --help \
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


CLEANTARGETS=$(OUT) $(OUT).clang $(OUT).gcc

default: $(OUT)

$(OUT):
	@clang++ $(CXXFLAGS) -o $(OUT) tests.cpp

noexcept:
	@clang++ $(CXXFLAGS) -fno-exceptions -o $(OUT).clang tests.cpp
	@g++ $(CXXFLAGS) -fno-exceptions -o $(OUT).gcc tests.cpp

run: $(OUT)
	@testbed $(TESTARGS)

test: clean $(OUT) noexcept
	@echo "\n\n================= DEFAULT        =================\n\n"
	@$(OUT) $(TESTARGS)
	@echo "\n\n================= GCC NOEXCEPT   =================\n\n"
	$(OUT).gcc $(TESTARGS)
	@echo "\n\n================= CLANG NOEXCEPT =================\n\n"
	$(OUT).clang $(TESTARGS)

clean:
	rm -f $(CLEANTARGETS)

.PHONY: run clean
