#include "zobrist.hpp"

uint64_t Z_PSQ[12][64];
uint64_t Z_CASTLING[16];
uint64_t Z_EP_FILE[8];
uint64_t Z_BLACK_TO_MOVE;

static uint64_t splitmix64(uint64_t &x) {
	uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

void initZobristTables(uint64_t seed) {
	uint64_t s = seed;

	for (int p = 0; p < 12; p++) {
		for (int sq = 0; sq < 64; sq++) {
			Z_PSQ[p][sq] = splitmix64(s);
		}
	}

	for (int i = 0; i < 16; i++) {
		Z_CASTLING[i] = splitmix64(s);
	}

	for (int i = 0; i < 8; i++) {
		Z_EP_FILE[i] = splitmix64(s);
	}

	Z_BLACK_TO_MOVE = splitmix64(s);
}
