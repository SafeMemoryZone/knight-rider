#include "position.hpp"

#include <cassert>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bitboards.hpp"
#include "movegen.hpp"

static UndoInfo undoStack[MAX_PLY];
static int ply = 0;

Position::Position()
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
}

Position::Position(const std::string &fen)
    : occForColor{}, pieces{}, epSquare(0), rule50(0), castlingRights(0) {
	std::istringstream stream(fen);

	std::string piecePlacement;
	std::string activeColorStr;
	std::string castlingRightsStr;
	std::string epSqStr;
	std::string rule50Str;
	std::string fullMoveStr;

	if (!(stream >> piecePlacement >> activeColorStr >> castlingRightsStr >> epSqStr >> rule50Str >>
	      fullMoveStr)) {
		throw std::runtime_error("Invalid FEN: missing fields");
	}

	// piece placement
	int rank = 7;
	int file = 0;
	for (char c : piecePlacement) {
		if (std::isdigit(c)) {
			file += c - '0';
			continue;
		}
		if (c == '/') {
			if (file != 8) {
				throw std::runtime_error("Invalid FEN: row not complete");
			}
			rank--;
			file = 0;
			continue;
		}
		if (rank < 0 || rank > 7 || file < 0 || file > 7) {
			throw std::runtime_error("Invalid FEN: bad board coords");
		}

		static const std::unordered_map<char, int> charToPieceIdx = {
		    {'P', PT_PAWN},       {'N', PT_KNIGHT},   {'B', PT_BISHOP},    {'R', PT_ROOK},
		    {'Q', PT_QUEEN},      {'K', PT_KING},     {'p', PT_PAWN + 6},  {'n', PT_KNIGHT + 6},
		    {'b', PT_BISHOP + 6}, {'r', PT_ROOK + 6}, {'q', PT_QUEEN + 6}, {'k', PT_KING + 6}};

		auto it = charToPieceIdx.find(c);
		if (it == charToPieceIdx.end()) {
			throw std::runtime_error("Invalid FEN: invalid piece char in placement");
		}

		pieces[it->second] |= 1ULL << (rank * 8 + file);
		file++;
	}

	if (rank != 0 || file != 8) {
		throw std::runtime_error("Invalid FEN: board dimensions incorrect");
	}

	// update occupancy
	for (int color = 0; color < 2; color++) {
		for (int pt = 0; pt < 6; pt++) {
			occForColor[color] |= pieces[color * 6 + pt];
		}
	}

	// active color
	if (activeColorStr == "w") {
		usColor = WHITE;
		oppColor = BLACK;
	}
	else if (activeColorStr == "b") {
		usColor = BLACK;
		oppColor = WHITE;
	}
	else {
		throw std::runtime_error("Invalid FEN: invalid active color");
	}

	// castling rights
	if (castlingRightsStr != "-") {
		for (char c : castlingRightsStr) {
			switch (c) {
				case 'k':
					castlingRights |= BLACK_KING_SIDE_CASTLE;
					break;
				case 'q':
					castlingRights |= BLACK_QUEEN_SIDE_CASTLE;
					break;
				case 'K':
					castlingRights |= WHITE_KING_SIDE_CASTLE;
					break;
				case 'Q':
					castlingRights |= WHITE_QUEEN_SIDE_CASTLE;
					break;
				default:
					throw std::runtime_error("Invalid FEN: invalid castling right");
			}
		}
	}

	// en-passant square
	if (epSqStr != "-") {
		if (epSqStr.length() != 2) {
			throw std::runtime_error("Invalid FEN: invalid en-passant square");
		}

		int epFile = epSqStr.at(0) - 'a';
		int epRank = epSqStr.at(1) - '1';

		if (epFile < 0 || epFile > 7 || epRank < 0 || epRank > 7) {
			throw std::runtime_error("Invalid FEN: invalid en-passant square");
		}

		epSquare |= 1ULL << (epRank * 8 + epFile);
	}

	// half-move clock
	try {
		int value = std::stoi(rule50Str);
		rule50 = value;
	} catch (const std::exception &e) {
		throw std::runtime_error("Invalid FEN: invalid half-move clock");
	}

	// full-move clock ignored
}

bool Position::operator==(const Position &other) const noexcept {
	return std::equal(other.occForColor, other.occForColor + 12, occForColor) &&
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
	UndoInfo &u = undoStack[ply++];
	uint32_t rawMove = std::bit_cast<uint32_t>(move);
	u.move = rawMove;
	u.castlingRights = castlingRights;
	u.epSquare = epSquare;
	u.halfmoveClock = rule50;

	Bitboard from = move.getFrom();
	Bitboard to = move.getTo();
	int movingPt = move.getMovingPt();
	int promoPt = move.getPromoPt();
	bool isEp = move.getIsEp();
	bool isCastling = move.getIsCastling();

	int capturedType = PT_NULL;
	if (isEp) {
		Bitboard capSquare = usColor == WHITE ? (to >> 8) : (to << 8);
		// find the pawn on that square
		capturedType = PT_PAWN;
		pieces[oppColor * 6 + PT_PAWN] ^= capSquare;
		occForColor[oppColor] ^= capSquare;
	}
	else {
		// normal capture
		Bitboard hit = to & occForColor[oppColor];
		if (hit) {
			// find which piece type
			for (uint8_t pt = 0; pt < 6; ++pt) {
				if (pieces[oppColor * 6 + pt] & hit) {
					capturedType = pt;
					pieces[oppColor * 6 + pt] ^= hit;
					occForColor[oppColor] ^= hit;
					break;
				}
			}
		}
	}
	u.capturedType = (uint8_t)capturedType;

	// move the actual piece
	int base = usColor * 6 + movingPt;
	pieces[base] ^= (from | to);
	occForColor[usColor] ^= (from | to);

	// promotions
	if (promoPt != PT_NULL) {
		pieces[base] ^= to;
		pieces[usColor * 6 + promoPt] ^= to;
	}

	// move rook when castling
	if (isCastling) {
		if (usColor == WHITE) {
			// white to move
			if (to == G1) {
				// king side: rook H1 -> F1
				pieces[PT_ROOK] ^= H1 | F1;
				occForColor[usColor] ^= H1 | F1;
			}
			else {
				// queen side: rook A1 -> D1
				pieces[PT_ROOK] ^= A1 | D1;
				occForColor[usColor] ^= A1 | D1;
			}
		}
		else {
			// black to move
			if (to == G8) {
				// king side: rook H8 -> F8
				pieces[6 + PT_ROOK] ^= H8 | F8;
				occForColor[usColor] ^= H8 | F8;
			}
			else {
				// queen side: rook A8 -> D8
				pieces[6 + PT_ROOK] ^= A8 | D8;
				occForColor[usColor] ^= A8 | D8;
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

	// update castling rights
	// case 1: rook moved or captured (cheap implementation, may zero-out castling rights even
	// though they already are)
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
		castlingRights &= usColor == WHITE ? 0b1100 : 0b0011;
	}

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
}

void Position::undoMove() {
	UndoInfo &undoInfo = undoStack[--ply];
	epSquare = undoInfo.epSquare;
	rule50 = undoInfo.halfmoveClock;
	castlingRights = undoInfo.castlingRights;

	usColor ^= 1;
	oppColor ^= 1;

	// recreate move
	Move move = std::bit_cast<Move>(undoInfo.move);
	Bitboard from = move.getFrom();
	Bitboard to = move.getTo();
	int movingPt = move.getMovingPt();
	int promoPt = move.getPromoPt();
	bool isEp = move.getIsEp();
	bool isCastling = move.getIsCastling();
	uint8_t capturedType = undoInfo.capturedType;

	// move the rook back if it was castling
	if (isCastling) {
		if (usColor == WHITE) {
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
	int base = usColor * 6 + movingPt;
	if (promoPt != PT_NULL) {
		pieces[usColor * 6 + promoPt] ^= to;
		pieces[base] ^= from;
		occForColor[usColor] ^= (from | to);
	}
	else {
		pieces[base] ^= (from | to);
		occForColor[usColor] ^= (from | to);
	}

	// restore captured piece
	if (capturedType != PT_NULL) {
		Bitboard capturedSquare;
		if (isEp) {
			capturedSquare = (usColor == WHITE ? (to >> 8) : (to << 8));
		}
		else {
			capturedSquare = to;
		}
		pieces[oppColor * 6 + capturedType] ^= capturedSquare;
		occForColor[oppColor] ^= capturedSquare;
	}
}
