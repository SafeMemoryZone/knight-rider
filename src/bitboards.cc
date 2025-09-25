#include "bitboards.hpp"

#include <bit>

// moving masks
Bitboard KING_MOVE_MASK[64];
Bitboard KNIGHT_MOVE_MASK[64];
Bitboard WHITE_PAWN_SINGLE_PUSH_MASK[64];
Bitboard WHITE_PAWN_CAPTURE_LEFT_MASK[64];
Bitboard WHITE_PAWN_CAPTURE_RIGHT_MASK[64];
Bitboard BLACK_PAWN_SINGLE_PUSH_MASK[64];
Bitboard BLACK_PAWN_CAPTURE_LEFT_MASK[64];
Bitboard BLACK_PAWN_CAPTURE_RIGHT_MASK[64];

// masks for pin detection & movement restriction
Bitboard ROOK_XRAY_MASK[64];
Bitboard BISHOP_XRAY_MASK[64];
Bitboard BETWEEN_MASK[64][64];
Bitboard LINE_MASK[64][64];  // line that goes through 2 points

// blocker masks for magic bitboards
Bitboard ROOK_BLOCKER_MASK[64];
Bitboard BISHOP_BLOCKER_MASK[64];
int ROOK_RELEVANT_BITS[64];
int BISHOP_RELEVANT_BITS[64];

// attack masks
Bitboard ROOK_ATTACK_MASK[64][4096];
Bitboard BISHOP_ATTACK_MASK[64][512];

static void initKingMoveMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard sqBb = 1ULL << sq;

		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_H) << 1;
		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_A) >> 1;
		KING_MOVE_MASK[sq] |= (sqBb & ~RANK_8) << 8;
		KING_MOVE_MASK[sq] |= (sqBb & ~RANK_1) >> 8;
		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_H & ~RANK_8) << 9;
		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~RANK_8) << 7;
		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_H & ~RANK_1) >> 7;
		KING_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~RANK_1) >> 9;
	}
}

static void initKnightMoveMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard sqBb = 1ULL << sq;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_H & ~RANK_8 & ~RANK_7) << 17;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~RANK_8 & ~RANK_7) << 15;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_H & ~RANK_2 & ~RANK_1) >> 15;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~RANK_2 & ~RANK_1) >> 17;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_G & ~FILE_H & ~RANK_8) << 10;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~FILE_B & ~RANK_8) << 6;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_G & ~FILE_H & ~RANK_1) >> 6;
		KNIGHT_MOVE_MASK[sq] |= (sqBb & ~FILE_A & ~FILE_B & ~RANK_1) >> 10;
	}
}

static void initPawnMoveMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard sqBb = 1ULL << sq;
		WHITE_PAWN_SINGLE_PUSH_MASK[sq] = sqBb << 8;
		WHITE_PAWN_CAPTURE_LEFT_MASK[sq] = (sqBb & ~FILE_A) << 7;
		WHITE_PAWN_CAPTURE_RIGHT_MASK[sq] = (sqBb & ~FILE_H) << 9;
		BLACK_PAWN_SINGLE_PUSH_MASK[sq] = sqBb >> 8;
		BLACK_PAWN_CAPTURE_LEFT_MASK[sq] = (sqBb & ~FILE_A) >> 9;
		BLACK_PAWN_CAPTURE_RIGHT_MASK[sq] = (sqBb & ~FILE_H) >> 7;
	}
}

static void initRookBlockerMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		Bitboard rookBlockerMask = 0ULL;

		for (int rank = sqRank + 1; rank <= 6; rank++) {
			rookBlockerMask |= 1ULL << (rank * 8 + sqFile);
		}
		for (int rank = sqRank - 1; rank >= 1; rank--) {
			rookBlockerMask |= 1ULL << (rank * 8 + sqFile);
		}
		for (int file = sqFile + 1; file <= 6; file++) {
			rookBlockerMask |= 1ULL << (sqRank * 8 + file);
		}
		for (int file = sqFile - 1; file >= 1; file--) {
			rookBlockerMask |= 1ULL << (sqRank * 8 + file);
		}
		ROOK_BLOCKER_MASK[sq] = rookBlockerMask;
		ROOK_RELEVANT_BITS[sq] = std::popcount(rookBlockerMask);
	}
}

static void initBishopBlockerMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		Bitboard bishopBlockerMask = 0ULL;

		for (int rank = sqRank - 1, file = sqFile + 1; rank >= 1 && file <= 6; rank--, file++) {
			bishopBlockerMask |= 1ULL << (rank * 8 + file);
		}
		for (int rank = sqRank - 1, file = sqFile - 1; rank >= 1 && file >= 1; rank--, file--) {
			bishopBlockerMask |= 1ULL << (rank * 8 + file);
		}
		for (int rank = sqRank + 1, file = sqFile + 1; rank <= 6 && file <= 6; rank++, file++) {
			bishopBlockerMask |= 1ULL << (rank * 8 + file);
		}
		for (int rank = sqRank + 1, file = sqFile - 1; rank <= 6 && file >= 1; rank++, file--) {
			bishopBlockerMask |= 1ULL << (rank * 8 + file);
		}
		BISHOP_BLOCKER_MASK[sq] = bishopBlockerMask;
		BISHOP_RELEVANT_BITS[sq] = std::popcount(bishopBlockerMask);
	}
}

static void initRookAttackMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		for (Bitboard blockerPattern = ROOK_BLOCKER_MASK[sq];;
		     blockerPattern = (blockerPattern - 1) & ROOK_BLOCKER_MASK[sq]) {
			Bitboard rookAttacksBb = 0ULL;

			for (int rank = sqRank + 1; rank <= 7; rank++) {
				Bitboard sqBb = 1ULL << (rank * 8 + sqFile);
				rookAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int rank = sqRank - 1; rank >= 0; rank--) {
				Bitboard sqBb = 1ULL << (rank * 8 + sqFile);
				rookAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int file = sqFile + 1; file <= 7; file++) {
				Bitboard sqBb = 1ULL << (sqRank * 8 + file);
				rookAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int file = sqFile - 1; file >= 0; file--) {
				Bitboard sqBb = 1ULL << (sqRank * 8 + file);
				rookAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}

			int idx = (int)((blockerPattern * ROOK_MAGIC[sq]) >> (64 - ROOK_RELEVANT_BITS[sq]));
			ROOK_ATTACK_MASK[sq][idx] = rookAttacksBb;

			if (!blockerPattern) {
				break;
			}
		}
	}
}

static void initBishopAttackMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		for (Bitboard blockerPattern = BISHOP_BLOCKER_MASK[sq];;
		     blockerPattern = (blockerPattern - 1) & BISHOP_BLOCKER_MASK[sq]) {
			Bitboard bishopAttacksBb = 0L;

			for (int rank = sqRank - 1, file = sqFile + 1; rank >= 0 && file <= 7; rank--, file++) {
				Bitboard sqBb = 1ULL << (rank * 8 + file);
				bishopAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int rank = sqRank - 1, file = sqFile - 1; rank >= 0 && file >= 0; rank--, file--) {
				Bitboard sqBb = 1ULL << (rank * 8 + file);
				bishopAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int rank = sqRank + 1, file = sqFile + 1; rank <= 7 && file <= 7; rank++, file++) {
				Bitboard sqBb = 1ULL << (rank * 8 + file);
				bishopAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}
			for (int rank = sqRank + 1, file = sqFile - 1; rank <= 7 && file >= 0; rank++, file--) {
				Bitboard sqBb = 1ULL << (rank * 8 + file);
				bishopAttacksBb |= sqBb;
				if (blockerPattern & sqBb) {
					break;
				}
			}

			int idx = (int)((blockerPattern * BISHOP_MAGIC[sq]) >> (64 - BISHOP_RELEVANT_BITS[sq]));
			BISHOP_ATTACK_MASK[sq][idx] = bishopAttacksBb;

			if (!blockerPattern) {
				break;
			}
		}
	}
}

static void initRookXRayMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		Bitboard xRays = 0ULL;

		for (int rank = sqRank + 1; rank <= 7; rank++) {
			xRays |= 1ULL << (rank * 8 + sqFile);
		}
		for (int rank = sqRank - 1; rank >= 0; rank--) {
			xRays |= 1ULL << (rank * 8 + sqFile);
		}
		for (int file = sqFile + 1; file <= 7; file++) {
			xRays |= 1ULL << (sqRank * 8 + file);
		}
		for (int file = sqFile - 1; file >= 0; file--) {
			xRays |= 1ULL << (sqRank * 8 + file);
		}

		ROOK_XRAY_MASK[sq] = xRays;
	}
}

static void initBishopXRayMask(void) {
	for (int sq = 0; sq < 64; sq++) {
		int sqRank = sq >> 3;
		int sqFile = sq & 7;

		Bitboard xRays = 0L;

		for (int rank = sqRank - 1, file = sqFile + 1; rank >= 0 && file <= 7; rank--, file++) {
			xRays |= 1L << (rank * 8 + file);
		}
		for (int rank = sqRank - 1, file = sqFile - 1; rank >= 0 && file >= 0; rank--, file--) {
			xRays |= 1L << (rank * 8 + file);
		}
		for (int rank = sqRank + 1, file = sqFile + 1; rank <= 7 && file <= 7; rank++, file++) {
			xRays |= 1L << (rank * 8 + file);
		}
		for (int rank = sqRank + 1, file = sqFile - 1; rank <= 7 && file >= 0; rank++, file--) {
			xRays |= 1L << (rank * 8 + file);
		}

		BISHOP_XRAY_MASK[sq] = xRays;
	}
}

static constexpr int signum(long long x) noexcept {
	if (x > 0) return +1;
	if (x < 0) return -1;
	return 0;
}

static void initBetweenMask(void) {
	for (int fromSq = 0; fromSq < 64; fromSq++) {
		int fromSqRank = fromSq >> 3;
		int fromSqFile = fromSq & 7;

		for (int toSq = 0; toSq < 64; toSq++) {
			Bitboard toSqBb = 1L << toSq;
			int toSqRank = toSq >> 3;
			int toSqFile = toSq & 7;

			if (!(ROOK_XRAY_MASK[fromSq] & toSqBb) && !(BISHOP_XRAY_MASK[fromSq] & toSqBb)) {
				continue;  // not on a valid ray
			}

			int dRank = signum(toSqRank - fromSqRank);
			int dFile = signum(toSqFile - fromSqFile);

			Bitboard betweenMask = 0ULL;

			for (int rank = fromSqRank + dRank, file = fromSqFile + dFile;
			     rank >= 0 && rank <= 7 && file >= 0 && file <= 7; rank += dRank, file += dFile) {
				if (rank == toSqRank && file == toSqFile) {
					break;
				}

				Bitboard sqBb = 1L << (rank * 8 + file);
				betweenMask |= sqBb;
			}

			BETWEEN_MASK[fromSq][toSq] = betweenMask;
		}
	}
}

static void initLineMask(void) {
	for (int sq1 = 0; sq1 < 64; sq1++) {
		int sq1Rank = sq1 >> 3;
		int sq1File = sq1 & 7;

		for (int sq2 = 0; sq2 < 64; sq2++) {
			Bitboard sq2BB = 1L << sq2;
			int sq2Rank = sq2 >> 3;
			int sq2File = sq2 & 7;

			if (!(ROOK_XRAY_MASK[sq1] & sq2BB) && !(BISHOP_XRAY_MASK[sq1] & sq2BB)) {
				continue;  // not on a valid ray
			}

			int dRank = signum(sq2Rank - sq1Rank);
			int dFile = signum(sq2File - sq1File);

			Bitboard ray = 0L;

			for (int rank = sq1Rank, file = sq1File;
			     rank >= 0 && rank <= 7 && file >= 0 && file <= 7; rank += dRank, file += dFile) {
				Bitboard sqBB = 1L << (rank * 8 + file);
				ray |= sqBB;
			}

			for (int rank = sq1Rank, file = sq1File;
			     rank >= 0 && rank <= 7 && file >= 0 && file <= 7; rank -= dRank, file -= dFile) {
				Bitboard sqBB = 1L << (rank * 8 + file);
				ray |= sqBB;
			}

			LINE_MASK[sq1][sq2] = ray;
		}
	}
}

void initBitboards(void) {
	initKingMoveMask();
	initKnightMoveMask();
	initPawnMoveMask();
	initRookBlockerMask();
	initBishopBlockerMask();
	initRookAttackMask();
	initBishopAttackMask();
	initRookXRayMask();
	initBishopXRayMask();
	initBetweenMask();
	initLineMask();
}
