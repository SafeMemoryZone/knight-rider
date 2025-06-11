#include "movegen.hpp"

#include <cassert>

#include "bitboards.hpp"

static inline Bitboard getRookAttacks(int sq, Bitboard occ) {
	Bitboard blockers = occ & ROOK_BLOCKER_MASK[sq];
	return ROOK_ATTACK_MASK[sq][(blockers * ROOK_MAGIC[sq]) >> (64 - ROOK_RELEVANT_BITS[sq])];
}

static inline Bitboard getBishopAttacks(int sq, Bitboard occ) {
	Bitboard blockers = occ & BISHOP_BLOCKER_MASK[sq];
	return BISHOP_ATTACK_MASK[sq][(blockers * BISHOP_MAGIC[sq]) >> (64 - BISHOP_RELEVANT_BITS[sq])];
}

MoveGenerator::MoveGenerator(Position &position) : position(position) {}

MoveList MoveGenerator::generateLegalMoves(void) const {
	MoveList moveList;

	int kingSq = std::countl_zero(position.pieces[position.usColor * 6 + PT_KING]);
	assert(kingSq < 64);

	Bitboard friendlyOcc = 0ULL;
	for (int pt = 0; pt < 6; pt++) {
		friendlyOcc |= position.pieces[position.usColor * 6 + pt];
	}

	Bitboard attackMask = computeAttackMask();
	Bitboard checkerMask = computeCheckerMask(kingSq);
	bool isInCheck = std::popcount(checkerMask) >= 1;
	bool isInDoubleCheck = std::popcount(checkerMask) == 2;
	// if in normal check and the checking piece is a slider piece, generate the block mask
	Bitboard checkBlockMask =
	    isInCheck && !isInDoubleCheck &&
	            (checkerMask & ~position.pieces[position.oppColor * 6 + PT_PAWN] &
	             ~position.pieces[position.oppColor * 6 + PT_KNIGHT])
	        ? BETWEEN_MASK[kingSq][std::countr_zero(checkerMask)]
	        : 0ULL;
	Bitboard checkEvasionMask = checkerMask | checkBlockMask;
	Bitboard pinMask = computePinMask(kingSq, friendlyOcc);

	return moveList;
}

Bitboard MoveGenerator::computeAttackMask(void) const {
	Bitboard attackMask = 0ULL;

	// pawns
	if (!position.usColor) {
		// we are white
		attackMask |= ((position.pieces[PT_PAWN + 6] & ~FILE_A) >> 9) |
		              ((position.pieces[PT_PAWN + 6] & ~FILE_H) >> 7);
	}
	else {
		// we are black
		attackMask |= ((position.pieces[PT_PAWN] & ~FILE_H) << 9) |
		              ((position.pieces[PT_PAWN] & ~FILE_A) << 7);
	}

	Bitboard occWithoutKing = position.occ & ~position.pieces[position.usColor * 6 + PT_KING];

	// rooks & queens
	Bitboard oppRooksQueens = position.pieces[position.oppColor * 6 + PT_ROOK] |
	                          position.pieces[position.oppColor * 6 + PT_QUEEN];
	while (oppRooksQueens) {
		attackMask |= getRookAttacks(std::countr_zero(oppRooksQueens), occWithoutKing);
		oppRooksQueens &= oppRooksQueens - 1;
	}

	// bishops & queens
	Bitboard oppBishopsQueens = position.pieces[position.oppColor * 6 + PT_BISHOP] |
	                            position.pieces[position.oppColor * 6 + PT_QUEEN];
	while (oppBishopsQueens) {
		attackMask |= getBishopAttacks(std::countr_zero(oppBishopsQueens), occWithoutKing);
		oppBishopsQueens &= oppBishopsQueens - 1;
	}

	// knights
	Bitboard oppKnights = position.pieces[position.oppColor * 6 + PT_KNIGHT];
	while (oppKnights) {
		attackMask |= KNIGHT_MOVE_MASK[std::countr_zero(oppKnights)];
		oppKnights &= oppKnights - 1;
	}

	// king
	int enemyKingSq = std::countr_zero(position.pieces[position.oppColor * 6 + PT_KING]);
	assert(enemyKingSq < 64);
	attackMask |= KING_MOVE_MASK[enemyKingSq];

	return attackMask;
}

Bitboard MoveGenerator::computeCheckerMask(int kingSq) const {
	Bitboard checkerMask = 0ULL;

	// pawns
	if (!position.usColor) {
		// we are white
		// use white pawns beacause we are looking from the perspective of the king
		checkerMask |=
		    (WHITE_PAWN_CAPTURE_LEFT_MASK[kingSq] | WHITE_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    position.pieces[PT_PAWN + 6];
	}
	else {
		// we are black
		// use black pawns beacause we are looking from the perspective of the king
		checkerMask |=
		    (BLACK_PAWN_CAPTURE_LEFT_MASK[kingSq] | BLACK_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    position.pieces[PT_PAWN];
	}

	// rooks & queens
	Bitboard oppRooksQueens = position.pieces[position.oppColor * 6 + PT_ROOK] |
	                          position.pieces[position.oppColor * 6 + PT_QUEEN];
	checkerMask |= getRookAttacks(kingSq, position.occ) & oppRooksQueens;

	// bishops & queens
	Bitboard oppBishopsQueens = position.pieces[position.oppColor * 6 + PT_BISHOP] |
	                            position.pieces[position.oppColor * 6 + PT_QUEEN];
	checkerMask |= getBishopAttacks(kingSq, position.occ) & oppBishopsQueens;

	// knights
	checkerMask |= KNIGHT_MOVE_MASK[kingSq] & position.pieces[position.oppColor * 6 + PT_KNIGHT];

	return checkerMask;
}

Bitboard MoveGenerator::computePinMask(int kingSq, Bitboard friendlyOcc) const {
	Bitboard oppRooksQueens = position.pieces[position.oppColor * 6 + PT_ROOK] |
	                          position.pieces[position.oppColor * 6 + PT_QUEEN];
	Bitboard oppBishopsQueens = position.pieces[position.oppColor * 6 + PT_BISHOP] |
	                            position.pieces[position.oppColor * 6 + PT_QUEEN];

	Bitboard potentialPinners =
	    (ROOK_XRAY_MASK[kingSq] & oppRooksQueens) | (BISHOP_XRAY_MASK[kingSq] & oppBishopsQueens);

	Bitboard pinMask = 0ULL;

	while (potentialPinners) {
		int pinnerSq = std::countr_zero(potentialPinners);
		Bitboard between = BETWEEN_MASK[pinnerSq][kingSq] & position.occ;

		// if there is exactly one piece in between and it is friendly
		if (std::has_single_bit(between) && (between & friendlyOcc)) {
			pinMask |= between;
		}

		potentialPinners &= potentialPinners - 1;
	}

	return pinMask;
}
