all: ice-sliding

ice-sliding: ice-sliding-puzzle.cpp
	g++ -O3 -Wall -Wextra $^ -o $@

