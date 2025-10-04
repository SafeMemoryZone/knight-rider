#include "position.hpp"

#include <cassert>
#include <cctype>
#include <sstream>
#include <string>

#include "bitboards.hpp"
#include "misc.hpp"
#include "zobrist.hpp"

Position::Position(void)
    : occForColor{},
      epSquare(0),
      rule50(0),
      castlingRights(0b1111),
      usColor(WHITE),
      oppColor(BLACK) {
	pieces[PT_PAWN] = 0x000000000000FF00ULL;
	pieces[PT_KNIGHT] = 0x0000000000000042ULL;
	pieces[PT_BISHOP] = 0x0000000000000024ULL;
	pieces[PT_ROOK] = 0x0000000000000081ULL;
	pieces[PT_QUEEN] = 0x0000000000000008ULL;
	pieces[PT_KING] = 0x0000000000000010ULL;
	pieces[PT_PAWN + 6] = 0x00FF000000000000ULL;
	pieces[PT_KNIGHT + 6] = 0x4200000000000000ULL;
	pieces[PT_BISHOP + 6] = 0x2400000000000000ULL;
	pieces[PT_ROOK + 6] = 0x8100000000000000ULL;
	pieces[PT_QUEEN + 6] = 0x0800000000000000ULL;
	pieces[PT_KING + 6] = 0x1000000000000000ULL;

	for (int color = 0; color < 2; color++) {
		for (int pt = 0; pt < 6; pt++) {
			occForColor[color] |= pieces[color * 6 + pt];
		}
	}

	hash = computeHash();
}

Position Position::fromFen(const std::string &fen, bool &success) noexcept {
	success = false;

	Position pos;

	// reset to empty
	for (int i = 0; i < 12; ++i) pos.pieces[i] = 0ULL;
	pos.occForColor[0] = pos.occForColor[1] = 0ULL;
	pos.epSquare = 0ULL;
	pos.rule50 = 0;
	pos.castlingRights = 0;
	pos.usColor = WHITE;
	pos.oppColor = BLACK;

	try {
		std::istringstream stream(fen);

		std::string piecePlacement;
		std::string activeColorStr;
		std::string castlingRightsStr;
		std::string epSqStr;
		std::string rule50Str;
		std::string fullMoveStr;

		if (!(stream >> piecePlacement >> activeColorStr >> castlingRightsStr >> epSqStr >>
		      rule50Str >> fullMoveStr)) {
			return pos;  // missing fields
		}

		int rank = 7;
		int file = 0;

		static const std::unordered_map<char, int> charToPieceIdx = {
		    {'P', PT_PAWN},       {'N', PT_KNIGHT},   {'B', PT_BISHOP},    {'R', PT_ROOK},
		    {'Q', PT_QUEEN},      {'K', PT_KING},     {'p', PT_PAWN + 6},  {'n', PT_KNIGHT + 6},
		    {'b', PT_BISHOP + 6}, {'r', PT_ROOK + 6}, {'q', PT_QUEEN + 6}, {'k', PT_KING + 6}};

		for (char c : piecePlacement) {
			if (std::isdigit(static_cast<unsigned char>(c))) {
				file += c - '0';
				if (file > 8) return pos;  // too many squares in a rank
				continue;
			}
			if (c == '/') {
				if (file != 8) return pos;  // rank incomplete
				rank--;
				if (rank < 0) return pos;  // too many ranks
				file = 0;
				continue;
			}

			if (rank < 0 || rank > 7 || file < 0 || file > 7) return pos;

			auto it = charToPieceIdx.find(c);
			if (it == charToPieceIdx.end()) return pos;  // invalid piece char

			pos.pieces[it->second] |= (1ULL << (rank * 8 + file));
			file++;
		}

		if (rank != 0 || file != 8) return pos;

		// occupancy
		pos.occForColor[0] = 0;
		pos.occForColor[1] = 0;
		for (int color = 0; color < 2; ++color) {
			for (int pt = 0; pt < 6; ++pt) {
				pos.occForColor[color] |= pos.pieces[color * 6 + pt];
			}
		}

		if (activeColorStr == "w") {
			pos.usColor = WHITE;
			pos.oppColor = BLACK;
		}
		else if (activeColorStr == "b") {
			pos.usColor = BLACK;
			pos.oppColor = WHITE;
		}
		else {
			return pos;
		}

		pos.castlingRights = 0;
		if (castlingRightsStr != "-") {
			for (char c : castlingRightsStr) {
				switch (c) {
					case 'K':
						pos.castlingRights |= WHITE_KING_SIDE_CASTLE;
						break;
					case 'Q':
						pos.castlingRights |= WHITE_QUEEN_SIDE_CASTLE;
						break;
					case 'k':
						pos.castlingRights |= BLACK_KING_SIDE_CASTLE;
						break;
					case 'q':
						pos.castlingRights |= BLACK_QUEEN_SIDE_CASTLE;
						break;
					default:
						return pos;
				}
			}
		}

		pos.epSquare = 0ULL;
		if (epSqStr != "-") {
			if (epSqStr.size() != 2) {
				return pos;
			}

			int epFile = epSqStr[0] - 'a';
			int epRank = epSqStr[1] - '1';

			if (epFile < 0 || epFile > 7 || epRank < 0 || epRank > 7) {
				return pos;
			}

			pos.epSquare = (1ULL << (epRank * 8 + epFile));
		}

		int value = std::stoi(rule50Str);
		if (value < 0) {
			return pos;
		}
		pos.rule50 = value;

		success = true;
		pos.hash = pos.computeHash();
		return pos;

	} catch (...) {
		success = false;
		return pos;
	}
}

bool Position::operator==(const Position &other) const noexcept {
	return std::equal(other.occForColor, other.occForColor + 2, occForColor) &&
	       std::equal(other.pieces, other.pieces + 12, pieces) && other.epSquare == epSquare &&
	       other.rule50 == rule50 && other.castlingRights == castlingRights &&
	       other.usColor == usColor && other.oppColor == oppColor;
}

std::string Position::toFen(void) const {
	std::ostringstream stream;

	static const char pieceIdxToChar[12] = {'P', 'N', 'B', 'R', 'Q', 'K',
	                                        'p', 'n', 'b', 'r', 'q', 'k'};

	// piece placement
	for (int rank = 7; rank >= 0; --rank) {
		int runningEmptyCount = 0;
		for (int file = 0; file < 8; file++) {
			uint64_t pieceBb = 1ULL << (rank * 8 + file);
			bool found = false;

			for (int pieceIdx = 0; pieceIdx < 12; pieceIdx++) {
				if (pieces[pieceIdx] & pieceBb) {
					if (runningEmptyCount) {
						stream << runningEmptyCount;
						runningEmptyCount = 0;
					}
					stream << pieceIdxToChar[pieceIdx];
					found = true;
					break;
				}
			}
			if (!found) {
				runningEmptyCount++;
			}
		}
		if (runningEmptyCount) {
			stream << runningEmptyCount;
		}
		if (rank > 0) {
			stream << '/';
		}
	}

	// active color
	stream << ' ' << (usColor == WHITE ? 'w' : 'b');

	// castling rights
	stream << ' ';
	if (!castlingRights) {
		stream << '-';
	}
	else {
		if (castlingRights & WHITE_KING_SIDE_CASTLE) {
			stream << 'K';
		}
		if (castlingRights & WHITE_QUEEN_SIDE_CASTLE) {
			stream << 'Q';
		}
		if (castlingRights & BLACK_KING_SIDE_CASTLE) {
			stream << 'k';
		}
		if (castlingRights & BLACK_QUEEN_SIDE_CASTLE) {
			stream << 'q';
		}
	}

	// en-passant square
	stream << ' ';
	if (!epSquare) {
		stream << '-';
	}
	else {
		int sq = std::countr_zero(epSquare);
		int f = sq % 8;
		int r = sq / 8;
		stream << char('a' + f) << char('1' + r);
	}

	// half-move and full-move clocks
	stream << ' ' << rule50 << ' ' << 1;  // full-move clock ignored
	return stream.str();
}

// rook endpoints used for castling mechanisms
constexpr Bitboard A1 = 1ULL << 0;
constexpr Bitboard D1 = 1ULL << 3;
constexpr Bitboard F1 = 1ULL << 5;
constexpr Bitboard G1 = 1ULL << 6;
constexpr Bitboard H1 = 1ULL << 7;
constexpr Bitboard A8 = 1ULL << 56;
constexpr Bitboard D8 = 1ULL << 59;
constexpr Bitboard F8 = 1ULL << 61;
constexpr Bitboard G8 = 1ULL << 62;
constexpr Bitboard H8 = 1ULL << 63;

void Position::makeMove(Move move) {
	if (usColor == WHITE) {
		makeMoveT<WHITE>(move);
	}
	else {
		makeMoveT<BLACK>(move);
	}
}

void Position::undoMove(void) {
	if (usColor == WHITE) {
		// already give inverted color
		undoMoveT<BLACK>();
	}
	else {
		undoMoveT<WHITE>();
	}
}

template <int UsColor>
void Position::makeMoveT(Move move) {
	constexpr int OppColor = UsColor ^ 1;

	UndoInfo &u = undoStack[ply++];
	u.move = move;
	u.castlingRights = castlingRights;
	u.epSquare = epSquare;
	u.halfmoveClock = rule50;
	u.hash = hash;

	Bitboard from = move.getFrom();
	Bitboard to = move.getTo();
	int movingPt = move.getMovingPt();
	int promoPt = move.getPromoPt();
	bool isEp = move.getIsEp();
	bool isCastling = move.getIsCastling();

	int fromSq = std::countr_zero(from);
	int toSq = std::countr_zero(to);

	if (epSquare) {
		int epFile = std::countr_zero(epSquare) & 7;
		hash ^= Z_EP_FILE[epFile];
	}

	hash ^= Z_CASTLING[castlingRights];

	int capturedType = PT_NULL;
	if (isEp) {
		Bitboard capSquare;
		if constexpr (UsColor == WHITE) {
			capSquare = to >> 8;
		}
		else {
			capSquare = to << 8;
		}
		// find the pawn on that square
		capturedType = PT_PAWN;
		pieces[OppColor * 6 + PT_PAWN] ^= capSquare;
		occForColor[OppColor] ^= capSquare;

		int capSq = std::countr_zero(capSquare);
		hash ^= Z_PSQ[OppColor * 6 + PT_PAWN][capSq];
	}
	else {
		// normal capture
		Bitboard hit = to & occForColor[OppColor];
		if (hit) {
			// find which piece type
			for (int pt = 0; pt < 6; ++pt) {
				if (pieces[OppColor * 6 + pt] & hit) {
					capturedType = pt;
					pieces[OppColor * 6 + pt] ^= hit;
					occForColor[OppColor] ^= hit;

					int capSq = std::countr_zero(hit);
					hash ^= Z_PSQ[OppColor * 6 + pt][capSq];
					break;
				}
			}
		}
	}
	u.capturedType = (uint8_t)capturedType;

	// move the actual piece
	int base = UsColor * 6 + movingPt;
	pieces[base] ^= (from | to);
	occForColor[UsColor] ^= (from | to);

	hash ^= Z_PSQ[base][fromSq];

	// promotions
	if (promoPt != PT_NULL) {
		pieces[base] ^= to;
		pieces[UsColor * 6 + promoPt] ^= to;

		hash ^= Z_PSQ[UsColor * 6 + promoPt][toSq];
	}
	else {
		hash ^= Z_PSQ[base][toSq];
	}

	// move rook when castling
	if (isCastling) {
		if constexpr (UsColor == WHITE) {
			// white to move
			if (to == G1) {
				// king side: rook H1 -> F1
				pieces[PT_ROOK] ^= H1 | F1;
				occForColor[UsColor] ^= H1 | F1;

				int rookFrom = std::countr_zero(H1);
				int rookTo = std::countr_zero(F1);
				hash ^= Z_PSQ[PT_ROOK][rookFrom];
				hash ^= Z_PSQ[PT_ROOK][rookTo];
			}
			else {
				// queen side: rook A1 -> D1
				pieces[PT_ROOK] ^= A1 | D1;
				occForColor[UsColor] ^= A1 | D1;

				int rookFrom = std::countr_zero(A1);
				int rookTo = std::countr_zero(D1);
				hash ^= Z_PSQ[PT_ROOK][rookFrom];
				hash ^= Z_PSQ[PT_ROOK][rookTo];
			}
		}
		else {
			// black to move
			if (to == G8) {
				// king side: rook H8 -> F8
				pieces[6 + PT_ROOK] ^= H8 | F8;
				occForColor[UsColor] ^= H8 | F8;

				int rookFrom = std::countr_zero(H8);
				int rookTo = std::countr_zero(F8);
				hash ^= Z_PSQ[6 + PT_ROOK][rookFrom];
				hash ^= Z_PSQ[6 + PT_ROOK][rookTo];
			}
			else {
				// queen side: rook A8 -> D8
				pieces[6 + PT_ROOK] ^= A8 | D8;
				occForColor[UsColor] ^= A8 | D8;

				int rookFrom = std::countr_zero(A8);
				int rookTo = std::countr_zero(D8);
				hash ^= Z_PSQ[6 + PT_ROOK][rookFrom];
				hash ^= Z_PSQ[6 + PT_ROOK][rookTo];
			}
		}
	}

	// update ep square
	// white double-push
	if (movingPt == PT_PAWN && (from & RANK_2) && (to & RANK_4)) {
		epSquare = to >> 8;
	}
	// black double-push
	else if (movingPt == PT_PAWN && (from & RANK_7) && (to & RANK_5)) {
		epSquare = to << 8;
	}
	else {
		epSquare = 0;
	}

	if (epSquare) {
		int epFile = std::countr_zero(epSquare) & 7;
		hash ^= Z_EP_FILE[epFile];
	}

	// update castling rights
	// case 1: rook moved
	if (from == H1 || to == H1) {
		castlingRights &= ~1;
	}
	else if (from == A1 || to == A1) {
		castlingRights &= ~2;
	}
	if (from == H8 || to == H8) {
		castlingRights &= ~4;
	}
	else if (from == A8 || to == A8) {
		castlingRights &= ~8;
	}
	// case 2: king moved
	if (movingPt == PT_KING) {
		if constexpr (UsColor == WHITE) {
			castlingRights &= 0b1100;
		}
		else {
			castlingRights &= 0b0011;
		}
	}

	hash ^= Z_CASTLING[castlingRights];

	// update 50-move rule counter
	if (movingPt == PT_PAWN || capturedType != PT_NULL) {
		rule50 = 0;
	}
	else {
		rule50++;
	}

	// switch side to move
	usColor ^= 1;
	oppColor ^= 1;
	hash ^= Z_BLACK_TO_MOVE;
}

template <int UsColor>
void Position::undoMoveT() {
	UndoInfo &u = undoStack[--ply];
	epSquare = u.epSquare;
	rule50 = u.halfmoveClock;
	castlingRights = u.castlingRights;
	hash = u.hash;

	usColor ^= 1;
	oppColor ^= 1;

	constexpr int OppColor = UsColor ^ 1;

	// recreate move
	Move move = u.move;
	Bitboard from = move.getFrom();
	Bitboard to = move.getTo();
	int movingPt = move.getMovingPt();
	int promoPt = move.getPromoPt();
	bool isEp = move.getIsEp();
	bool isCastling = move.getIsCastling();
	uint8_t capturedType = u.capturedType;

	// move the rook back if it was castling
	if (isCastling) {
		if constexpr (UsColor == WHITE) {
			// white just castled
			if (to == G1) {
				pieces[PT_ROOK] ^= (F1 | H1);
				occForColor[WHITE] ^= (F1 | H1);
			}
			else {  // to == C1
				pieces[PT_ROOK] ^= (D1 | A1);
				occForColor[WHITE] ^= (D1 | A1);
			}
		}
		else {
			// black just castled
			if (to == G8) {
				pieces[6 + PT_ROOK] ^= (F8 | H8);
				occForColor[BLACK] ^= (F8 | H8);
			}
			else {  // to == C8
				pieces[6 + PT_ROOK] ^= (D8 | A8);
				occForColor[BLACK] ^= (D8 | A8);
			}
		}
	}

	// undo the moving piece or promotion
	int base = UsColor * 6 + movingPt;
	if (promoPt != PT_NULL) {
		pieces[UsColor * 6 + promoPt] ^= to;
		pieces[base] ^= from;
		occForColor[UsColor] ^= (from | to);
	}
	else {
		pieces[base] ^= (from | to);
		occForColor[UsColor] ^= (from | to);
	}

	// restore captured piece
	if (capturedType != PT_NULL) {
		Bitboard capturedSquare;
		if (isEp) {
			if constexpr (UsColor == WHITE) {
				capturedSquare = to >> 8;
			}
			else {
				capturedSquare = to << 8;
			}
		}
		else {
			capturedSquare = to;
		}
		pieces[OppColor * 6 + capturedType] ^= capturedSquare;
		occForColor[OppColor] ^= capturedSquare;
	}
}

void Position::resetPly(void) { ply = 0; }

uint64_t Position::computeHash(void) {
	uint64_t h = 0;
	// pieces
	for (int p = 0; p < 12; ++p) {
		uint64_t b = pieces[p];
		while (b) {
			int sq = std::countr_zero(b);
			b &= b - 1;
			h ^= Z_PSQ[p][sq];
		}
	}

	h ^= Z_CASTLING[castlingRights];

	if (epSquare) {
		int file = std::countr_zero(epSquare) & 7;
		h ^= Z_EP_FILE[file];
	}

	if (usColor == BLACK) {
		h ^= Z_BLACK_TO_MOVE;
	}

	return h;
}
