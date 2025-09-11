#include <iostream>

#include "bitboards.hpp"
#include "movegen.hpp"
#include "perft.hpp"

int main() {
	initBitboards();
	Position pos("2B1bN2/1pQ1qP2/7k/2p5/1Pp1P2p/5p2/4p3/K7 w - - 0 1");
	MoveGenerator gen(&pos);

	std::cout << pos.toFen() << '\n';
	initPerft(pos);

	auto startTime = std::chrono::high_resolution_clock::now();
	size_t nodes = perft(6, true);
	auto endTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsedTime = endTime - startTime;

	std::cout << '\n' << "Nodes searched: " << nodes << '\n';
	std::cout << "Time: " << elapsedTime.count() << " seconds\n";

	return 0;
}
