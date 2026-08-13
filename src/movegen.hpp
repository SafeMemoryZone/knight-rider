#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "movelist.hpp"
#include "position.hpp"

class MoveGenerator {
   public:
	MoveGenerator(void) = default;
	explicit MoveGenerator(Position* positionPtr);

	MoveList generateLegalMoves(bool onlyCaptures = false);

   private:
	// color-specific templates
	template <int UsColor, bool OnlyCaptures>
	MoveList generateLegalMovesT(void);
	template <int UsColor>
	Bitboard computeAttackMaskT(void) const;
	template <int UsColor>
	Bitboard computeCheckerMaskT(void) const;
	template <int UsColor>
	Bitboard computePinMaskT(void) const;
	template <int UsColor>
	bool isEpLegalT(Bitboard capturingPawn) const;

	Position* position = nullptr;
	Bitboard* P = nullptr;  // alias for position->pieces

    // re-computed by generateLegalMovesT() everytime
    Bitboard usOcc = 0ULL;
    Bitboard oppOcc = 0ULL;
    Bitboard occ = 0ULL;
    Bitboard oppRooksQueens = 0ULL;
    Bitboard oppBishopsQueens = 0ULL;
    int kingSq = 0ULL;
};

#endif  // MOVEGEN_HPP
