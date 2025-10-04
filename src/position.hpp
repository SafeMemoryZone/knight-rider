#ifndef POSITION_HPP
#define POSITION_HPP

#include <cstdint>
#include <string>

#include "misc.hpp"
#include "move.hpp"

// undo info for a single move
struct UndoInfo {
	Bitboard epSquare;
	Move move;
	int halfmoveClock;
	uint8_t castlingRights;
	uint8_t capturedType;
};

// chess position representation
struct Position {
	static Position fromFen(const std::string &fen, bool &success) noexcept;

	Position(void);

	bool operator==(const Position &other) const noexcept;

	std::string toFen(void) const;
	void makeMove(Move move);
	void undoMove(void);
	void resetPly(void);

	// board state
	Bitboard occForColor[2];
	Bitboard pieces[12];
	Bitboard epSquare;
	int rule50;
	uint8_t castlingRights;
	uint8_t usColor;
	uint8_t oppColor;

	uint64_t hash;

	int ply = 0;

   private:
	UndoInfo undoStack[MAX_PLY];

	template <int UsColor>
	void makeMoveT(Move move);
	template <int UsColor>
	void undoMoveT(void);

	uint64_t computeHash(void);
};

#endif  // POSITION_HPP
