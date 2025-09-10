#ifndef POSITION_HPP
#define POSITION_HPP
#include <cstdint>
#include <string>

typedef uint64_t Bitboard;

class Move;  // forward decleration to avoid recursive dependency

constexpr int WHITE = 0;
constexpr int BLACK = 1;

constexpr int WHITE_KING_SIDE_CASTLE = 1;
constexpr int WHITE_QUEEN_SIDE_CASTLE = 2;
constexpr int BLACK_KING_SIDE_CASTLE = 4;
constexpr int BLACK_QUEEN_SIDE_CASTLE = 8;

struct Position {
	Position();
	explicit Position(const std::string &fen);

	bool operator==(const Position &other) const noexcept;

	std::string toFen(void) const;
	void makeMove(Move move);
	void undoMove(void);

	Bitboard occForColor[2];
	Bitboard pieces[12];
	Bitboard epSquare;
	int rule50;
	uint8_t castlingRights;  // 1 - white king side, 2 - white queen side, 4 - black king side, 8 -
	                         // black queen side
	uint8_t usColor;         // 0 for white, 1 for black
	uint8_t oppColor;
};

static constexpr int MAX_PLY = 256;
struct UndoInfo {
	Bitboard epSquare;
	uint32_t move;
	int halfmoveClock;
	uint8_t castlingRights;
	uint8_t capturedType;
};

#endif  // POSITION_HPP
