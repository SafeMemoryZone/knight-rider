#include "movegen.hpp"

#include <bit>
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
		moveList.push_back(Move(from, currMove, pt, PT_NULL, false, false));
		allMoves &= allMoves - 1;
	}
}

MoveGenerator::MoveGenerator(Position *positionPtr)
    : position(positionPtr), P(positionPtr->pieces) {}

MoveList MoveGenerator::generateLegalMoves(bool onlyCaptures) const {
	if (position->usColor == WHITE) {
		return onlyCaptures ? generateLegalMovesT<WHITE, true>()
		                    : generateLegalMovesT<WHITE, false>();
	}
	else {
		return onlyCaptures ? generateLegalMovesT<BLACK, true>()
		                    : generateLegalMovesT<BLACK, false>();
	}
}

static Bitboard usOcc;
static Bitboard oppOcc;
static Bitboard occ;
static Bitboard oppRooksQueens;
static Bitboard oppBishopsQueens;
static int kingSq;

template <int UsColor, bool OnlyCaptures>
MoveList MoveGenerator::generateLegalMovesT(void) const {
	constexpr int OppColor = UsColor ^ 1;

	MoveList moveList;

	// globals used in other functions

	kingSq = std::countr_zero(P[UsColor * 6 + PT_KING]);
	assert(kingSq < 64);

	usOcc = position->occForColor[UsColor];
	oppOcc = position->occForColor[OppColor];
	occ = position->occForColor[WHITE] | position->occForColor[BLACK];
	oppRooksQueens = P[OppColor * 6 + PT_ROOK] | P[OppColor * 6 + PT_QUEEN];
	oppBishopsQueens = P[OppColor * 6 + PT_BISHOP] | P[OppColor * 6 + PT_QUEEN];

	// locals

	const Bitboard attackMask = computeAttackMaskT<UsColor>();
	const Bitboard checkerMask = computeCheckerMaskT<UsColor>();
	const int checkCount = std::popcount(checkerMask);
	const bool isInCheck = checkCount != 0;
	moveList.setInCheck(isInCheck);
	const bool isInDoubleCheck = checkCount > 1;
	// if in normal check and the checking piece is a slider piece, generate the block mask
	const Bitboard sliderCheckers =
	    checkerMask & ~(P[OppColor * 6 + PT_PAWN] | P[OppColor * 6 + PT_KNIGHT]);
	const Bitboard checkBlockMask = (checkCount == 1 && sliderCheckers)
	                                    ? BETWEEN_MASK[kingSq][std::countr_zero(sliderCheckers)]
	                                    : 0ULL;
	const Bitboard checkEvasionMask = isInCheck ? checkerMask | checkBlockMask : ~0ULL;
	const Bitboard pinMask = computePinMaskT<UsColor>();
	const Bitboard capturableSquares = [] {
		if constexpr (OnlyCaptures)
			return oppOcc;  // only enemy squares
		else
			return ~usOcc;  // normal: empty or enemy
	}();

	// move generation start

	if (!isInDoubleCheck) [[likely]] {
		// pawns
		Bitboard freeSquares = ~occ;
		Bitboard pawns = P[UsColor * 6 + PT_PAWN];

		while (pawns) {
			Bitboard currPawn = pawns & -pawns;
			int currPawnSq = std::countr_zero(currPawn);

			if constexpr (UsColor == WHITE) {
				// we are white
				// pushes
				Bitboard singlePush = [&] {
					if constexpr (OnlyCaptures)
						return 0ULL;
					else
						return WHITE_PAWN_SINGLE_PUSH_MASK[currPawnSq] & freeSquares;
				}();

				Bitboard doublePush = [&] {
					if constexpr (OnlyCaptures)
						return 0ULL;
					else
						return ((singlePush & RANK_3) << 8) & freeSquares;
				}();

				// captures
				Bitboard leftCapture = WHITE_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & oppOcc;
				Bitboard rightCapture = WHITE_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & oppOcc;

				Bitboard normalMoves = singlePush | doublePush | leftCapture | rightCapture;

				// en-passant
				Bitboard ep =
				    isEpLegalT<UsColor>(currPawn)
				        ? (WHITE_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & position->epSquare) |
				              (WHITE_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & position->epSquare)
				        : 0ULL;

				normalMoves &= checkEvasionMask;
				if (isInCheck) {
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
					moveList.push_back(Move(currPawn, ep, PT_PAWN, PT_NULL, false, true));
				}

				// add each normal move
				while (normalMoves) {
					Bitboard to = normalMoves & -normalMoves;

					// promotion
					if (to & RANK_8) {
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_KNIGHT, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_BISHOP, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_ROOK, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_QUEEN, false, false));
					}
					else {
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_NULL, false, false));
					}

					normalMoves &= normalMoves - 1;
				}
			}
			else {
				// we are black
				// pushes
				Bitboard singlePush = [&] {
					if constexpr (OnlyCaptures)
						return 0ULL;
					else
						return BLACK_PAWN_SINGLE_PUSH_MASK[currPawnSq] & freeSquares;
				}();

				Bitboard doublePush = [&] {
					if constexpr (OnlyCaptures)
						return 0ULL;
					else
						return ((singlePush & RANK_6) >> 8) & freeSquares;
				}();

				// captures
				Bitboard leftCapture = BLACK_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & oppOcc;
				Bitboard rightCapture = BLACK_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & oppOcc;

				Bitboard normalMoves = singlePush | doublePush | leftCapture | rightCapture;

				// en-passant
				Bitboard ep =
				    isEpLegalT<UsColor>(currPawn)
				        ? (BLACK_PAWN_CAPTURE_LEFT_MASK[currPawnSq] & position->epSquare) |
				              (BLACK_PAWN_CAPTURE_RIGHT_MASK[currPawnSq] & position->epSquare)
				        : 0ULL;

				normalMoves &= checkEvasionMask;
				if (isInCheck) {
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
					moveList.push_back(Move(currPawn, ep, PT_PAWN, PT_NULL, false, true));
				}

				// add each normal move
				while (normalMoves) {
					Bitboard to = normalMoves & -normalMoves;

					// promotion
					if (to & RANK_1) {
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_KNIGHT, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_BISHOP, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_ROOK, false, false));
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_QUEEN, false, false));
					}
					else {
						moveList.push_back(Move(currPawn, to, PT_PAWN, PT_NULL, false, false));
					}

					normalMoves &= normalMoves - 1;
				}
			}

			pawns &= pawns - 1;
		}

		// knights
		Bitboard knights = P[UsColor * 6 + PT_KNIGHT];
		while (knights) {
			Bitboard currKnight = knights & -knights;
			int currKnightSq = std::countr_zero(currKnight);

			Bitboard moves = KNIGHT_MOVE_MASK[currKnightSq] & capturableSquares;

			// checks & pins
			moves &= checkEvasionMask;

			if (currKnight & pinMask) {
				moves &= LINE_MASK[currKnightSq][kingSq];
			}

			addMovesToList(moveList, currKnight, moves, PT_KNIGHT);
			knights &= knights - 1;
		}

		// bishops
		Bitboard bishops = P[UsColor * 6 + PT_BISHOP];
		while (bishops) {
			Bitboard currBishop = bishops & -bishops;
			int currBishopSq = std::countr_zero(currBishop);
			Bitboard moves = getBishopAttacks(currBishopSq, occ) & capturableSquares;

			// checks & pins
			moves &= checkEvasionMask;

			if (currBishop & pinMask) {
				moves &= LINE_MASK[currBishopSq][kingSq];
			}

			addMovesToList(moveList, currBishop, moves, PT_BISHOP);
			bishops &= bishops - 1;
		}

		// rooks
		Bitboard rooks = P[UsColor * 6 + PT_ROOK];
		while (rooks) {
			Bitboard currRook = rooks & -rooks;
			int currRookSq = std::countr_zero(currRook);
			Bitboard moves = getRookAttacks(currRookSq, occ) & capturableSquares;

			// checks & pins
			moves &= checkEvasionMask;

			if (currRook & pinMask) {
				moves &= LINE_MASK[currRookSq][kingSq];
			}

			addMovesToList(moveList, currRook, moves, PT_ROOK);
			rooks &= rooks - 1;
		}

		// queens
		Bitboard queens = P[UsColor * 6 + PT_QUEEN];
		while (queens) {
			Bitboard currQueen = queens & -queens;
			int currQueenSq = std::countr_zero(currQueen);
			Bitboard moves =
			    (getRookAttacks(currQueenSq, occ) | getBishopAttacks(currQueenSq, occ)) &
			    capturableSquares;

			// checks & pins
			moves &= checkEvasionMask;

			if (currQueen & pinMask) {
				moves &= LINE_MASK[currQueenSq][kingSq];
			}

			addMovesToList(moveList, currQueen, moves, PT_QUEEN);
			queens &= queens - 1;
		}
	}

	// king
	Bitboard kingMoves = KING_MOVE_MASK[kingSq] & capturableSquares & ~attackMask;
	addMovesToList(moveList, P[UsColor * 6 + PT_KING], kingMoves, PT_KING);

	// castling generation if not in check
	if constexpr (!OnlyCaptures) {
		if (!isInCheck) {
			if constexpr (UsColor == WHITE) {
				constexpr Bitboard E1 = 1ULL << 4, F1 = 1ULL << 5, G1 = 1ULL << 6, D1 = 1ULL << 3,
				                   C1 = 1ULL << 2, B1 = 1ULL << 1;
				// we are white
				// king-side
				if (position->castlingRights & WHITE_KING_SIDE_CASTLE) {
					Bitboard between = F1 | G1;
					// squares empty && not attacked
					if (!(occ & between) && !(attackMask & between)) {
						moveList.push_back(Move(E1, G1, PT_KING, PT_NULL, true, false));
					}
				}
				// queen-side
				if (position->castlingRights & WHITE_QUEEN_SIDE_CASTLE) {
					Bitboard between = B1 | C1 | D1;
					Bitboard passSquares = D1 | C1;
					if (!(occ & between) && !(attackMask & passSquares)) {
						moveList.push_back(Move(E1, C1, PT_KING, PT_NULL, true, false));
					}
				}
			}
			else {
				constexpr Bitboard E8 = 1ULL << 60, F8 = 1ULL << 61, G8 = 1ULL << 62,
				                   D8 = 1ULL << 59, C8 = 1ULL << 58, B8 = 1ULL << 57;
				// we are black
				// king-side
				if (position->castlingRights & BLACK_KING_SIDE_CASTLE) {
					Bitboard between = F8 | G8;
					if (!(occ & between) && !(attackMask & between)) {
						moveList.push_back(Move(E8, G8, PT_KING, PT_NULL, true, false));
					}
				}
				// queen-side
				if (position->castlingRights & BLACK_QUEEN_SIDE_CASTLE) {
					Bitboard between = B8 | C8 | D8;
					Bitboard passSquares = D8 | C8;
					if (!(occ & between) && !(attackMask & passSquares)) {
						moveList.push_back(Move(E8, C8, PT_KING, PT_NULL, true, false));
					}
				}
			}
		}
	}

	return moveList;
}

template <int UsColor>
Bitboard MoveGenerator::computeAttackMaskT(void) const {
	constexpr int OppColor = UsColor ^ 1;

	Bitboard attackMask = 0ULL;

	// pawns
	if constexpr (UsColor == WHITE) {
		// we are white
		attackMask |= ((P[PT_PAWN + 6] & ~FILE_A) >> 9) | ((P[PT_PAWN + 6] & ~FILE_H) >> 7);
	}
	else {
		// we are black
		attackMask |= ((P[PT_PAWN] & ~FILE_H) << 9) | ((P[PT_PAWN] & ~FILE_A) << 7);
	}

	Bitboard occWithoutKing = occ & ~P[UsColor * 6 + PT_KING];

	// rooks & queens
	Bitboard localOppRooksQueens = oppRooksQueens;
	while (localOppRooksQueens) {
		attackMask |= getRookAttacks(std::countr_zero(localOppRooksQueens), occWithoutKing);
		localOppRooksQueens &= localOppRooksQueens - 1;
	}

	// bishops & queens
	Bitboard localOppBishopsQueens = oppBishopsQueens;
	while (localOppBishopsQueens) {
		attackMask |= getBishopAttacks(std::countr_zero(localOppBishopsQueens), occWithoutKing);
		localOppBishopsQueens &= localOppBishopsQueens - 1;
	}

	// knights
	Bitboard oppKnights = P[OppColor * 6 + PT_KNIGHT];
	while (oppKnights) {
		attackMask |= KNIGHT_MOVE_MASK[std::countr_zero(oppKnights)];
		oppKnights &= oppKnights - 1;
	}

	// king
	int enemyKingSq = std::countr_zero(P[OppColor * 6 + PT_KING]);
	assert(enemyKingSq < 64);
	attackMask |= KING_MOVE_MASK[enemyKingSq];

	return attackMask;
}

template <int UsColor>
Bitboard MoveGenerator::computeCheckerMaskT(void) const {
	constexpr int OppColor = UsColor ^ 1;

	Bitboard checkerMask = 0ULL;

	// pawns
	if constexpr (UsColor == WHITE) {
		// we are white
		// use white pawns because we are looking from the perspective of the king
		checkerMask |=
		    (WHITE_PAWN_CAPTURE_LEFT_MASK[kingSq] | WHITE_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    P[PT_PAWN + 6];
	}
	else {
		// we are black
		// use black pawns because we are looking from the perspective of the king
		checkerMask |=
		    (BLACK_PAWN_CAPTURE_LEFT_MASK[kingSq] | BLACK_PAWN_CAPTURE_RIGHT_MASK[kingSq]) &
		    P[PT_PAWN];
	}

	// rooks & queens
	checkerMask |= getRookAttacks(kingSq, occ) & oppRooksQueens;

	// bishops & queens
	checkerMask |= getBishopAttacks(kingSq, occ) & oppBishopsQueens;

	// knights
	checkerMask |= KNIGHT_MOVE_MASK[kingSq] & P[OppColor * 6 + PT_KNIGHT];

	return checkerMask;
}

template <int UsColor>
Bitboard MoveGenerator::computePinMaskT(void) const {
	Bitboard potentialPinners =
	    (ROOK_XRAY_MASK[kingSq] & oppRooksQueens) | (BISHOP_XRAY_MASK[kingSq] & oppBishopsQueens);

	Bitboard pinMask = 0ULL;

	while (potentialPinners) {
		int pinnerSq = std::countr_zero(potentialPinners);
		Bitboard between = BETWEEN_MASK[pinnerSq][kingSq] & occ;

		// if there is exactly one piece in between and it is friendly
		if (std::has_single_bit(between) && (between & usOcc)) {
			pinMask |= between;
		}

		potentialPinners &= potentialPinners - 1;
	}

	return pinMask;
}

template <int UsColor>
bool MoveGenerator::isEpLegalT(Bitboard capturingPawn) const {
	// this function only does a quick horizontal check, which is not covered by pin detection

	// no en-passant
	if (!position->epSquare) {
		return false;
	}

	Bitboard capturedPawn;
	if constexpr (UsColor == WHITE) {
		capturedPawn = position->epSquare >> 8;
	}
	else {
		capturedPawn = position->epSquare << 8;
	}

	int epRank = std::countr_zero(capturedPawn) >> 3;
	int kingRank = kingSq >> 3;

	if (epRank != kingRank) {
		return true;
	}

	// occupancy without both pawns
	Bitboard occWithoutPawns = occ & ~capturingPawn & ~capturedPawn;
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
