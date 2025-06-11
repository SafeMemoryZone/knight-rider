#ifndef POSITION_HPP
#define POSITION_HPP
#include <cstdint>
#include <string>

typedef uint64_t Bitboard;

enum { PT_PAWN = 0, PT_KNIGHT, PT_BISHOP, PT_ROOK, PT_QUEEN, PT_KING, PT_NULL };

struct Position {
	Position();
	explicit Position(const std::string &fen);

	std::string toFen(void) const;

	Bitboard occ;
	Bitboard pieces[12];
	Bitboard epSq;
	int usColor;  // 0 for white, 1 for black
	int castlingRights;
	int rule50;
	int fullMoveCount;
};

#endif  // POSITION_HPP
