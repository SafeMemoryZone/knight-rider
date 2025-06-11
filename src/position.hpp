#ifndef POSITION_HPP
#define POSITION_HPP
#include <cstdint>
#include <string>

typedef uint64_t Bitboard;

struct Position {
	Position();
	explicit Position(const std::string &fen);

	std::string toFen(void) const;

	Bitboard occ;
	Bitboard pieces[12];
	Bitboard epSq;
	int usColor;  // 0 for white, 1 for black
	int oppColor;
	int castlingRights;
	int rule50;
	int fullMoveCount;
};

#endif  // POSITION_HPP
