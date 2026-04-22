#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "movelist.hpp"
#include "position.hpp"

class MoveGenerator {
   public:
	MoveGenerator(void) = default;
	explicit MoveGenerator(Position* positionPtr);

	MoveList generateLegalMoves(bool onlyCaptures = false) const;

   private:
	// color-specific templates
	template <int UsColor, bool OnlyCaptures>
	MoveList generateLegalMovesT(void) const;
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
};

#endif  // MOVEGEN_HPP
