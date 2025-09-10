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

static inline void addMovesToList(MoveList &moveList, Bitboard from, Bitboard allMoves, int pt) {
	while (allMoves) {
		Bitboard currMove = allMoves & -allMoves;
		moveList.add(Move(from, currMove, pt, PT_NULL, false, false));
		allMoves &= allMoves - 1;
	}
}

Move::Move(Bitboard from, Bitboard to, int movingPt, int promoPt, bool isCastling, bool isEp)
    : move(0) {
	move |= std::countr_zero(from) & 0x3F;
	move |= (uint32_t)(std::countr_zero(to) & 0x3F) << 6;
	move |= (uint32_t)(movingPt & 7) << 12;
	move |= (uint32_t)(promoPt & 7) << 15;
	move |= (uint32_t)(isCastling & 1) << 18;
	move |= (uint32_t)(isEp & 1) << 19;
}

MoveGenerator::MoveGenerator(Position *position) : position(position) {}

MoveList MoveGenerator::generateLegalMoves(void) const {
	MoveList moveList;

	int kingSq = std::countr_zero(position->pieces[position->usColor * 6 + PT_KING]);
	assert(kingSq < 64);

	Bitboard occ = position->occForColor[WHITE] | position->occForColor[BLACK];

	Bitboard attackMask = computeAttackMask(occ);
	Bitboard checkerMask = computeCheckerMask(kingSq, occ);
	bool isInCheck = std::popcount(checkerMask) >= 1;
	bool isInDoubleCheck = std::popcount(checkerMask) == 2;
	// if in normal check and the checking piece is a slider piece, generate the block mask
	Bitboard checkBlockMask =
	    isInCheck && !isInDoubleCheck &&
	            (checkerMask & ~position->pieces[position->oppColor * 6 + PT_PAWN] &
	             ~position->pieces[position->oppColor * 6 + PT_KNIGHT])
	        ? BETWEEN_MASK[kingSq][std::countr_zero(checkerMask)]
	        : 0ULL;
	Bitboard checkEvasionMask = checkerMask | checkBlockMask;
	Bitboard pinMask = computePinMask(kingSq, occ);

	Bitboard capturableSquares = ~position->occForColor[position->usColor];
	if (!isInDoubleCheck) [[likely]] {
		// pawns
		Bitboard freeSquares = ~occ;
		Bitboard pawns = position->pieces[position->usColor * 6 + PT_PAWN];
		while (pawns) {
			Bitboard currPawn = pawns & -pawns;
			int currPawnSq = std::countr_zero(currPawn);

			if (position->usColor == WHITE) {
				// we are white
				// pushes
				Bitboard singlePush = WHITE_PAWN_SINGLE_PUSH_MASK[currPawnSq] & freeSquares;
				Bitboard doublePush = ((singlePush & RANK_3) << 8) & freeSquares;

				// captures
				Bitboard leftCapture = WHITE_PAWN_CAPTURE_LEFT_MASK[currPawnSq] &
				                       position->occForColor[position->oppColor];
				Bitboard rightCapture = WHITE_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] &
				                        position->occForColor[position->oppColor];

				Bitboard normalMoves = singlePush | doublePush | leftCapture | rightCapture;

				// en-passant
				Bitboard ep =
				    isEpLegal(kingSq, occ, currPawn)
				        ? (WHITE_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & position->epSquare) |
				              (WHITE_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & position->epSquare)
				        : 0ULL;

				if (isInCheck) {
					normalMoves &= checkEvasionMask;

					// if en-passant does not resolve the check, disallow it
					if ((ep >> 8) != checkEvasionMask) {
						ep = 0ULL;
					}
				}

				if (currPawn & pinMask) {
					normalMoves &= LINE_MASK[currPawnSq][kingSq];
					ep &= LINE_MASK[currPawnSq][kingSq];
				}

				// add en-passant move
				if (ep) {
					moveList.add(Move(currPawn, ep, PT_PAWN, PT_NULL, false, true));
				}

				// add each normal move
				while (normalMoves) {
					Bitboard to = normalMoves & -normalMoves;

					// promotion
					if (to & RANK_8) {
						moveList.add(Move(currPawn, to, PT_PAWN, PT_KNIGHT, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_BISHOP, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_ROOK, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_QUEEN, false, false));
					}
					else {
						moveList.add(Move(currPawn, to, PT_PAWN, PT_NULL, false, false));
					}

					normalMoves &= normalMoves - 1;
				}
			}
			else {
				// we are black
				// pushes
				Bitboard singlePush = BLACK_PAWN_SINGLE_PUSH_MASK[currPawnSq] & freeSquares;
				Bitboard doublePush = ((singlePush & RANK_6) >> 8) & freeSquares;

				// captures
				Bitboard leftCapture = BLACK_PAWN_CAPTURE_LEFT_MASK[currPawnSq] &
				                       position->occForColor[position->oppColor];
				Bitboard rightCapture = BLACK_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] &
				                        position->occForColor[position->oppColor];

				Bitboard normalMoves = singlePush | doublePush | leftCapture | rightCapture;

				// en-passant
				Bitboard ep =
				    isEpLegal(kingSq, occ, currPawn)
				        ? (BLACK_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & position->epSquare) |
				              (BLACK_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & position->epSquare)
				        : 0ULL;

				if (isInCheck) {
					normalMoves &= checkEvasionMask;

					// if en-passant does not resolve the check, disallow it
					if ((ep << 8) != checkEvasionMask) {
						ep = 0ULL;
					}
				}

				if (currPawn & pinMask) {
					normalMoves &= LINE_MASK[currPawnSq][kingSq];
					ep &= LINE_MASK[currPawnSq][kingSq];
				}

				// add en-passant move
				if (ep) {
					moveList.add(Move(currPawn, ep, PT_PAWN, PT_NULL, false, true));
				}

				// add each normal move
				while (normalMoves) {
					Bitboard to = normalMoves & -normalMoves;

					// promotion
					if (to & RANK_1) {
						moveList.add(Move(currPawn, to, PT_PAWN, PT_KNIGHT, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_BISHOP, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_ROOK, false, false));
						moveList.add(Move(currPawn, to, PT_PAWN, PT_QUEEN, false, false));
					}
					else {
						moveList.add(Move(currPawn, to, PT_PAWN, PT_NULL, false, false));
					}

					normalMoves &= normalMoves - 1;
				}
			}

			pawns &= pawns - 1;
		}

		// knights
		Bitboard knights = position->pieces[position->usColor * 6 + PT_KNIGHT];
		while (knights) {
			Bitboard currKnight = knights & -knights;
			Bitboard moves = KNIGHT_MOVE_MASK[std::countr_zero(currKnight)] & capturableSquares;

			// checks & pins
			if (isInCheck) {
				moves &= checkEvasionMask;
			}

			if (currKnight & pinMask) {
				moves &= LINE_MASK[std::countr_zero(currKnight)][kingSq];
			}

			addMovesToList(moveList, currKnight, moves, PT_KNIGHT);
			knights &= knights - 1;
		}

		// bishops
		Bitboard bishops = position->pieces[position->usColor * 6 + PT_BISHOP];
		while (bishops) {
			Bitboard currBishop = bishops & -bishops;
			Bitboard moves =
			    getBishopAttacks(std::countr_zero(currBishop), occ) & capturableSquares;

			// checks & pins
			if (isInCheck) {
				moves &= checkEvasionMask;
			}

			if (currBishop & pinMask) {
				moves &= LINE_MASK[std::countr_zero(currBishop)][kingSq];
			}

			addMovesToList(moveList, currBishop, moves, PT_BISHOP);
			bishops &= bishops - 1;
		}

		// rooks
		Bitboard rooks = position->pieces[position->usColor * 6 + PT_ROOK];
		while (rooks) {
			Bitboard currRook = rooks & -rooks;
			Bitboard moves = getRookAttacks(std::countr_zero(currRook), occ) & capturableSquares;

			// checks & pins
			if (isInCheck) {
				moves &= checkEvasionMask;
			}

			if (currRook & pinMask) {
				moves &= LINE_MASK[std::countr_zero(currRook)][kingSq];
			}

			addMovesToList(moveList, currRook, moves, PT_ROOK);
			rooks &= rooks - 1;
		}

		// queens
		Bitboard queens = position->pieces[position->usColor * 6 + PT_QUEEN];
		while (queens) {
			Bitboard currQueen = queens & -queens;
			Bitboard moves = (getRookAttacks(std::countr_zero(currQueen), occ) |
			                  getBishopAttacks(std::countr_zero(currQueen), occ)) &
			                 capturableSquares;

			// checks & pins
			if (isInCheck) {
				moves &= checkEvasionMask;
			}

			if (currQueen & pinMask) {
				moves &= LINE_MASK[std::countr_zero(currQueen)][kingSq];
			}

			addMovesToList(moveList, currQueen, moves, PT_QUEEN);
			queens &= queens - 1;
		}
	}

	// king
	Bitboard kingMoves = KING_MOVE_MASK[kingSq] & capturableSquares & ~attackMask;
	addMovesToList(moveList, position->pieces[position->usColor * 6 + PT_KING], kingMoves, PT_KING);

	if (!isInCheck) {
		if (position->usColor == WHITE) {
			constexpr Bitboard E1 = 1ULL << 4, F1 = 1ULL << 5, G1 = 1ULL << 6, D1 = 1ULL << 3,
			                   C1 = 1ULL << 2, B1 = 1ULL << 1;
			// we are white
			// king-side
			if (position->castlingRights & WHITE_KING_SIDE_CASTLE) {
				Bitboard between = F1 | G1;
				// squares empty && not attacked
				if (!(occ & between) && !(attackMask & between)) {
					moveList.add(Move(E1, G1, PT_KING, PT_NULL, true, false));
				}
			}
			// queen-side
			if (position->castlingRights & WHITE_QUEEN_SIDE_CASTLE) {
				Bitboard between = B1 | C1 | D1;
				Bitboard passSquares = D1 | C1;
				if (!(occ & between) && !(attackMask & passSquares)) {
					moveList.add(Move(E1, C1, PT_KING, PT_NULL, true, false));
				}
			}
		}
		else {
			constexpr Bitboard E8 = 1ULL << 60, F8 = 1ULL << 61, G8 = 1ULL << 62, D8 = 1ULL << 59,
			                   C8 = 1ULL << 58, B8 = 1ULL << 57;
			// we are black
			// king-side
			if (position->castlingRights & BLACK_KING_SIDE_CASTLE) {
				Bitboard between = F8 | G8;
				if (!(occ & between) && !(attackMask & between)) {
					moveList.add(Move(E8, G8, PT_KING, PT_NULL, true, false));
				}
			}
			// queen-side
			if (position->castlingRights & BLACK_QUEEN_SIDE_CASTLE) {
				Bitboard between = B8 | C8 | D8;
				Bitboard passSquares = D8 | C8;
				if (!(occ & between) && !(attackMask & passSquares)) {
					moveList.add(Move(E8, C8, PT_KING, PT_NULL, true, false));
				}
			}
		}
	}

	return moveList;
}

Bitboard MoveGenerator::computeAttackMask(Bitboard occ) const {
	Bitboard attackMask = 0ULL;

	// pawns
	if (position->usColor == WHITE) {
		// we are white
		attackMask |= ((position->pieces[PT_PAWN + 6] & ~FILE_A) >> 9) |
		              ((position->pieces[PT_PAWN + 6] & ~FILE_H) >> 7);
	}
	else {
		// we are black
		attackMask |= ((position->pieces[PT_PAWN] & ~FILE_H) << 9) |
		              ((position->pieces[PT_PAWN] & ~FILE_A) << 7);
	}

	Bitboard occWithoutKing = occ & ~position->pieces[position->usColor * 6 + PT_KING];

	// rooks & queens
	Bitboard oppRooksQueens = position->pieces[position->oppColor * 6 + PT_ROOK] |
	                          position->pieces[position->oppColor * 6 + PT_QUEEN];
	while (oppRooksQueens) {
		attackMask |= getRookAttacks(std::countr_zero(oppRooksQueens), occWithoutKing);
		oppRooksQueens &= oppRooksQueens - 1;
	}

	// bishops & queens
	Bitboard oppBishopsQueens = position->pieces[position->oppColor * 6 + PT_BISHOP] |
	                            position->pieces[position->oppColor * 6 + PT_QUEEN];
	while (oppBishopsQueens) {
		attackMask |= getBishopAttacks(std::countr_zero(oppBishopsQueens), occWithoutKing);
		oppBishopsQueens &= oppBishopsQueens - 1;
	}

	// knights
	Bitboard oppKnights = position->pieces[position->oppColor * 6 + PT_KNIGHT];
	while (oppKnights) {
		attackMask |= KNIGHT_MOVE_MASK[std::countr_zero(oppKnights)];
		oppKnights &= oppKnights - 1;
	}

	// king
	int enemyKingSq = std::countr_zero(position->pieces[position->oppColor * 6 + PT_KING]);
	assert(enemyKingSq < 64);
	attackMask |= KING_MOVE_MASK[enemyKingSq];

	return attackMask;
}

Bitboard MoveGenerator::computeCheckerMask(int kingSq, Bitboard occ) const {
	Bitboard checkerMask = 0ULL;

	// pawns
	if (position->usColor == WHITE) {
		// we are white
		// use white pawns beacause we are looking from the perspective of the king
		checkerMask |=
		    (WHITE_PAWN_CAPTURE_LEFT_MASK[kingSq] | WHITE_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    position->pieces[PT_PAWN + 6];
	}
	else {
		// we are black
		// use black pawns beacause we are looking from the perspective of the king
		checkerMask |=
		    (BLACK_PAWN_CAPTURE_LEFT_MASK[kingSq] | BLACK_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    position->pieces[PT_PAWN];
	}

	// rooks & queens
	Bitboard oppRooksQueens = position->pieces[position->oppColor * 6 + PT_ROOK] |
	                          position->pieces[position->oppColor * 6 + PT_QUEEN];
	checkerMask |= getRookAttacks(kingSq, occ) & oppRooksQueens;

	// bishops & queens
	Bitboard oppBishopsQueens = position->pieces[position->oppColor * 6 + PT_BISHOP] |
	                            position->pieces[position->oppColor * 6 + PT_QUEEN];
	checkerMask |= getBishopAttacks(kingSq, occ) & oppBishopsQueens;

	// knights
	checkerMask |= KNIGHT_MOVE_MASK[kingSq] & position->pieces[position->oppColor * 6 + PT_KNIGHT];

	return checkerMask;
}

Bitboard MoveGenerator::computePinMask(int kingSq, Bitboard occ) const {
	Bitboard oppRooksQueens = position->pieces[position->oppColor * 6 + PT_ROOK] |
	                          position->pieces[position->oppColor * 6 + PT_QUEEN];
	Bitboard oppBishopsQueens = position->pieces[position->oppColor * 6 + PT_BISHOP] |
	                            position->pieces[position->oppColor * 6 + PT_QUEEN];

	Bitboard potentialPinners =
	    (ROOK_XRAY_MASK[kingSq] & oppRooksQueens) | (BISHOP_XRAY_MASK[kingSq] & oppBishopsQueens);

	Bitboard pinMask = 0ULL;

	while (potentialPinners) {
		int pinnerSq = std::countr_zero(potentialPinners);
		Bitboard between = BETWEEN_MASK[pinnerSq][kingSq] & occ;

		// if there is exactly one piece in between and it is friendly
		if (std::has_single_bit(between) && (between & position->occForColor[position->usColor])) {
			pinMask |= between;
		}

		potentialPinners &= potentialPinners - 1;
	}

	return pinMask;
}

bool MoveGenerator::isEpLegal(int kingSq, Bitboard occ, Bitboard capturingPawn) const {
	// this function only does a quick horizontal check, which is not covered by pin detection

	Bitboard capturedPawn =
	    position->usColor == WHITE ? position->epSquare >> 8 : position->epSquare << 8;

	// no en-passant
	if (!capturingPawn) {
		return false;
	}

	int epRank = std::countr_zero(capturedPawn) >> 3;
	int kingRank = kingSq >> 3;

	if (epRank != kingRank) {
		return true;
	}

	// occupancy without both pawns
	Bitboard occWithoutPawns = occ & ~capturingPawn & ~capturedPawn;
	Bitboard oppRooksQueens = position->pieces[position->oppColor * 6 + PT_ROOK] |
	                          position->pieces[position->oppColor * 6 + PT_QUEEN];
	Bitboard relevantAttackers =
	    (RANK_1 << (8 * epRank)) &
	    oppRooksQueens;  // attackers are only relevant if they can check horizontally

	while (relevantAttackers) {
		int attackerSq = std::countr_zero(relevantAttackers);
		// if there is nothing between the attacker and king
		if (!(occWithoutPawns & BETWEEN_MASK[kingSq][attackerSq])) {
			return false;
		}
		relevantAttackers &= relevantAttackers - 1;
	}

	return true;
}
