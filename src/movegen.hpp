#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include <cstdint>

#include "position.hpp"

constexpr int PT_PAWN = 0;
constexpr int PT_KNIGHT = 1;
constexpr int PT_BISHOP = 2;
constexpr int PT_ROOK = 3;
constexpr int PT_QUEEN = 4;
constexpr int PT_KING = 5;
constexpr int PT_NULL = 6;

class Move {
   public:
	Move() = default;
	Move(Bitboard from, Bitboard to, int movingPt, int promoPt, bool isCastling, bool isEp);

	inline Bitboard getFrom() const;
	inline Bitboard getTo() const;
	inline int getMovingPt() const;
	inline int getPromoPt() const;
	inline bool getIsCastling() const;
	inline bool getIsEp() const;

   private:
	uint32_t move;
};

inline Bitboard Move::getFrom() const { return 1ULL << (move & 0x3F); }

inline Bitboard Move::getTo() const { return 1ULL << ((move >> 6) & 0x3F); }

inline int Move::getMovingPt() const { return (move >> 12) & 7; }

inline int Move::getPromoPt() const { return (move >> 15) & 7; }

inline bool Move::getIsCastling() const { return (move >> 18) & 1; }

inline bool Move::getIsEp() const { return (move >> 19) & 1; }

class MoveList {
   public:
	MoveList() : movesCount(0) {}
	inline void add(Move move);

	Move* begin() { return moves; }
	Move* end() { return moves + movesCount; }
	const Move* begin() const { return moves; }
	const Move* end() const { return moves + movesCount; }
	const Move* cbegin() const { return moves; }
	const Move* cend() const { return moves + movesCount; }

   private:
	static constexpr int MAX_MOVES = 256;

	Move moves[MAX_MOVES];
	int movesCount;
};

inline void MoveList::add(Move move) { moves[movesCount++] = move; }

class MoveGenerator {
   public:
	MoveGenerator(Position& position);

	MoveList generateLegalMoves(void) const;

   private:
	Bitboard computeAttackMask(Bitboard occ) const;
    Bitboard computeCheckerMask(int kingSq, Bitboard occ) const;
    Bitboard computePinMask(int kingSq, Bitboard occ) const;
    bool isEpLegal(int kingSq, Bitboard occ, Bitboard capturingPawn) const;

	Position& position;
};

#endif  // MOVEGEN_HPP
