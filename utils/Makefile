# Makefile для CodeSearch (Windows / macOS / Linux)

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    CXX      = clang++
    BIN      = codesearch
    TEST_BIN = tests
else ifeq ($(UNAME_S),Linux)
    CXX      = g++
    BIN      = codesearch
    TEST_BIN = tests
else
    CXX      = g++
    BIN      = codesearch.exe
    TEST_BIN = tests.exe
endif

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

SOURCES      = main.cpp RegexBuilder.cpp FileSearcher.cpp Prompt.cpp
TEST_SOURCES = tests.cpp RegexBuilder.cpp FileSearcher.cpp

.PHONY: all tests clean run test-run

all: $(BIN)

$(BIN): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SOURCES)

tests: $(TEST_BIN)

$(TEST_BIN): $(TEST_SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TEST_BIN) $(TEST_SOURCES)

run: $(BIN)
	./$(BIN)

test-run: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f codesearch codesearch.exe tests tests.exe