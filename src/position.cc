#include "position.hpp"

#include <cassert>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

Position::Position()
    : occ(0), epSq(0), usColor(0), castlingRights(0b1111), rule50(0), fullMoveCount(1) {
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

	for (int pieceIdx = 0; pieceIdx < 12; pieceIdx++) {
		occ |= pieces[pieceIdx];
	}
}

Position::Position(const std::string &fen)
    : pieces{}, epSq(0), usColor(0), castlingRights(0), rule50(0), fullMoveCount(0) {
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
	for (int pieceIdx = 0; pieceIdx < 12; pieceIdx++) {
		occ |= pieces[pieceIdx];
	}

	// active color
	if (activeColorStr == "w") {
		usColor = 0;
	}
	else if (activeColorStr == "b") {
		usColor = 1;
	}
	else {
		throw std::runtime_error("Invalid FEN: invalid active color");
	}

	// castling rights
	if (castlingRightsStr != "-") {
		for (char c : castlingRightsStr) {
			switch (c) {
				case 'k':
					castlingRights |= 4;
					break;
				case 'q':
					castlingRights |= 8;
					break;
				case 'K':
					castlingRights |= 1;
					break;
				case 'Q':
					castlingRights |= 2;
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

		epSq |= 1ULL << (epRank * 8 + epFile);
	}

	// half-move clock
	try {
		int value = std::stoi(rule50Str);
		rule50 = value;
	} catch (const std::exception &e) {
		throw std::runtime_error("Invalid FEN: invalid half-move clock");
	}

	// full-move clock
	try {
		int value = std::stoi(fullMoveStr);
		fullMoveCount = value;
	} catch (const std::exception &e) {
		throw std::runtime_error("Invalid FEN: invalid full-move clock");
	}
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
	stream << ' ' << (usColor == 0 ? 'w' : 'b');

	// castling rights
	stream << ' ';
	if (!castlingRights) {
		stream << '-';
	}
	else {
		if (castlingRights & 1) {
			stream << 'K';
		}
		if (castlingRights & 2) {
			stream << 'Q';
		}
		if (castlingRights & 4) {
			stream << 'k';
		}
		if (castlingRights & 8) {
			stream << 'q';
		}
	}

	// en-passant square
	stream << ' ';
	if (!epSq) {
		stream << '-';
	}
	else {
		int sq = std::countr_zero(epSq);
		int f = sq % 8;
		int r = sq / 8;
		stream << char('a' + f) << char('1' + r);
	}

	// half-move and full-move clocks
	stream << ' ' << rule50 << ' ' << fullMoveCount;
	return stream.str();
}
