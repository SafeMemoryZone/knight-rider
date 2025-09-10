#include "perft.hpp"

#include <iostream>

#include "movegen.hpp"

static MoveGenerator moveGenerator;
static Position position;

void initPerft(Position pos) {
	position = pos;
	moveGenerator = MoveGenerator(&position);
}

size_t perft(int depth, bool printCountAfterMoves) {
	size_t nodes = 0;

	MoveList legalMoves = moveGenerator.generateLegalMoves();

	if (depth == 1) {
		return static_cast<size_t>(legalMoves.getMovesCount());
	}

	for (Move move : legalMoves) {
		position.makeMove(move);
		size_t count = perft(depth - 1, false);

		if (printCountAfterMoves) [[unlikely]] {
			std::cout << move.toLan() << ": " << count << '\n';
		}

		nodes += count;
		position.undoMove();
	}

	return nodes;
}
