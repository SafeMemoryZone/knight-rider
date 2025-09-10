#include <iostream>

#include "bitboards.hpp"
#include "movegen.hpp"
#include "perft.hpp"

int main() {
	initBitboards();
	Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	MoveGenerator gen(&pos);

	std::cout << pos.toFen() << '\n';
	initPerft(pos);

	auto startTime = std::chrono::high_resolution_clock::now();
	size_t nodes = perft(7, true);
	auto endTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsedTime = endTime - startTime;

	std::cout << '\n' << "Nodes searched: " << nodes << '\n';
	std::cout << "Time: " << elapsedTime.count() << " seconds\n";

	return 0;
}
