#include "perft.hpp"

#include <iostream>

#include "movegen.hpp"
#include "movelist.hpp"

static Position position;
static MoveGenerator moveGenerator = MoveGenerator(&position);

void initPerft(const Position &pos) { position = pos; }

template <bool PrintPerftLine>
static size_t perftT(int depth) {
	size_t nodes = 0;

	MoveList legalMoves = moveGenerator.generateLegalMoves();

	if (depth == 1) {
		return static_cast<size_t>(legalMoves.size());
	}

	for (Move move : legalMoves) {
		position.makeMove(move);
		size_t count = perftT<false>(depth - 1);

		if constexpr (PrintPerftLine) {
			std::cout << move.toLan() << ": " << count << '\n';
		}

		nodes += count;
		position.undoMove();
	}

	return nodes;
}

// wrapper for perfT
size_t perft(int depth, bool printPerftLine) {
	if (printPerftLine) {
		return perftT<true>(depth);
	}
	return perftT<false>(depth);
}
