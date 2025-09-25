#include <iostream>

#include "bitboards.hpp"
#include "movegen.hpp"
#include "perft.hpp"

int main() {
	initBitboards();
	Position pos("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
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
