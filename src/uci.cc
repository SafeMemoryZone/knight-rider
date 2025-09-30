#include "uci.hpp"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bitboards.hpp"

static std::vector<std::string> tokenizeLine(const std::string& line) {
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token) tokens.push_back(token);
	return tokens;
}

static inline bool advanceIfPossible(const std::vector<std::string>& tokens, size_t& pos) {
	if (pos + 1 >= tokens.size()) {
		return false;
	}
	pos++;
	return true;
}

static void printBestMove(Move move) { std::cout << "bestmove " << move.toLan() << std::endl; }

void UciEngine::start(void) {
	std::string line;

	for (;;) {
		if (!std::getline(std::cin, line)) {
			break;
		}
		if (line.empty()) {
			continue;
		}

		// lowercased copy for command recognition
		std::string lowerLine = line;
		for (char& c : lowerLine) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		// original for FEN
		lowerTokens = tokenizeLine(lowerLine);
		tokens = tokenizeLine(line);
		tokenPos = 0;

		if (lowerTokens.empty()) {
			continue;
		}

		const std::string& cmd = lowerTokens.at(0);

		if (cmd == "uci") {
			handleUciCmd();
		}
		else if (cmd == "debug") {
			handleDebugCmd();
		}
		else if (cmd == "isready") {
			handleIsReadyCmd();
		}
		else if (cmd == "setoption") {
			// TODO: implement
			if (isDebugMode) {
				std::cout << "info string 'setoption' not implemented yet" << std::endl;
			}
		}
		else if (cmd == "ucinewgame") {
			handleUcinewgameCmd();
		}
		else if (cmd == "position") {
			handlePositionCmd();
		}
		else if (cmd == "go") {
			handleGoCmd();
		}
		else if (cmd == "ponderhit") {
			// TODO: implement
			if (isDebugMode) {
				std::cout << "info string 'ponderhit' not implemented yet" << std::endl;
			}
		}
		else if (cmd == "stop") {
			handleStopCmd();
		}
		else if (cmd == "quit") {
			return;
		}
	}
}

void UciEngine::handleUciCmd(void) {
	std::cout << "id name Knightrider\n";
	std::cout << "id author Viliam Holly\n";
	// TODO: std::cout << "option name Hash type spin default 1 min 1 max 1024\n";
	initBitboards();
	std::cout << "uciok" << std::endl;
}

void UciEngine::handleDebugCmd(void) {
	tokenPos = 1;
	if (tokenPos >= lowerTokens.size()) {
		if (isDebugMode) {
			std::cout << "info string missing argument" << std::endl;
		}
		return;
	}
	const std::string& arg = lowerTokens.at(tokenPos);
	if (arg == "on") {
		isDebugMode = true;
	}
	else if (arg == "off") {
		isDebugMode = false;
	}
	else {
		if (isDebugMode) {
			std::cout << "info string expected 'on' or 'off'" << std::endl;
		}
	}
}

void UciEngine::handleIsReadyCmd(void) { std::cout << "readyok" << std::endl; }

void UciEngine::handleUcinewgameCmd(void) {
	pos = Position();
	if (isDebugMode) {
		std::cout << "info string new UCI game initialized" << std::endl;
	}
}

void UciEngine::handlePositionCmd(void) {
	// position [startpos | fen <FEN...>] [moves <m1> <m2> ...]
	tokenPos = 1;
	if (tokenPos >= lowerTokens.size()) {
		if (isDebugMode) {
			std::cout << "info string missing argument" << std::endl;
		}
		return;
	}

	// startpos
	if (lowerTokens.at(tokenPos) == "startpos") {
		pos = Position();

		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				std::cout << "info string position set" << std::endl;
			}
			return;
		}
	}
	// FEN position
	else if (lowerTokens.at(tokenPos) == "fen") {
		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				std::cout << "info string missing FEN" << std::endl;
			}
			return;
		}

		size_t fenStart = tokenPos;
		size_t fenEnd = fenStart;

		while (fenEnd < lowerTokens.size() && lowerTokens[fenEnd] != "moves") {
			fenEnd++;
		}

		if (fenStart == fenEnd) {
			if (isDebugMode) {
				std::cout << "info string invalid FEN string" << std::endl;
			}
			return;
		}

		// build fen from tokens
		std::string fen;
		for (size_t j = fenStart; j < fenEnd; j++) {
			if (j > fenStart) {
				fen.push_back(' ');
			}
			fen += tokens.at(j);
		}

		bool success;
		pos = Position::fromFen(fen, success);
		if (!success) {
			if (isDebugMode) {
				std::cout << "info string invalid FEN string" << std::endl;
			}
			return;
		}
		tokenPos = fenEnd;
	}
	else {
		if (isDebugMode) {
			std::cout << "info string expected 'startpos' or 'fen'" << std::endl;
		}
		return;
	}

	// move list (optional)
	if (tokenPos < lowerTokens.size() && lowerTokens.at(tokenPos) == "moves") {
		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				std::cout << "info string position set" << std::endl;
			}
			return;
		}

		while (tokenPos < tokens.size()) {
			const std::string& lan = tokens.at(tokenPos);

			bool applied = false;
			MoveList legalMoves = gen.generateLegalMoves();

			for (const Move move : legalMoves) {
				if (move.toLan() == lan) {
					pos.makeMove(move);
					applied = true;
					break;
				}
			}

			if (!applied && isDebugMode) {
				std::cout << "info string illegal or unknown move: " << lan << std::endl;
			}

			tokenPos++;
		}
	}

	if (isDebugMode) {
		std::cout << "info string position set" << std::endl;
	}
}

void UciEngine::handleGoCmd(void) {
	const auto recvTP = std::chrono::steady_clock::now();
	tokenPos = 1;

	GoLimits limits{};
	limits.timeLeftMS[0] = limits.timeLeftMS[1] = -1;
	limits.incMS[0] = limits.incMS[1] = 0;
	limits.movesToGo = -1;
	limits.depthLimit = -1;
	limits.nodeLimit = -1;
	limits.proveMateInN = -1;
	limits.moveTimeMS = -1;
	limits.infinite = false;
	limits.ponder = false;
	limits.searchMoves = MoveList();

	auto isKeyword = [](const std::string& s) -> bool {
		return s == "searchmoves" || s == "ponder" || s == "wtime" || s == "btime" || s == "winc" ||
		       s == "binc" || s == "movestogo" || s == "depth" || s == "nodes" || s == "mate" ||
		       s == "movetime" || s == "infinite";
	};

	// parse int helpers
	auto parseI32 = [&](int& out) -> bool {
		if (tokenPos >= tokens.size()) return false;
		try {
			out = std::stoi(tokens[tokenPos]);
			tokenPos++;
			return true;
		} catch (...) {
			return false;
		}
	};
	auto parseI64 = [&](int64_t& out) -> bool {
		if (tokenPos >= tokens.size()) return false;
		try {
			out = std::stoll(tokens[tokenPos]);
			tokenPos++;
			return true;
		} catch (...) {
			return false;
		}
	};

	// move generator
	auto addSearchMove = [&](const std::string& txt) -> bool {
		MoveList legal = gen.generateLegalMoves();
		for (const Move m : legal) {
			if (m.toLan() == txt) {
				limits.searchMoves.push_back(m);
				return true;
			}
		}
		return false;
	};

	while (tokenPos < lowerTokens.size()) {
		const std::string& kw = lowerTokens[tokenPos++];
		if (kw == "searchmoves") {
			// get move tokens until we hit another keyword or end
			while (tokenPos < lowerTokens.size() && !isKeyword(lowerTokens[tokenPos])) {
				const std::string& mvTxt = tokens[tokenPos];
				if (!addSearchMove(mvTxt) && isDebugMode) {
					std::cout << "info string ignoring unknown searchmove '" << mvTxt << "'"
					          << std::endl;
				}
				tokenPos++;
			}
		}
		else if (kw == "ponder") {
			limits.ponder = true;
		}
		else if (kw == "wtime") {
			if (!parseI64(limits.timeLeftMS[0]) && isDebugMode)
				std::cout << "info string missing/invalid wtime value" << std::endl;
		}
		else if (kw == "btime") {
			if (!parseI64(limits.timeLeftMS[1]) && isDebugMode)
				std::cout << "info string missing/invalid btime value" << std::endl;
		}
		else if (kw == "winc") {
			if (!parseI32(limits.incMS[0]) && isDebugMode)
				std::cout << "info string missing/invalid winc value" << std::endl;
		}
		else if (kw == "binc") {
			if (!parseI32(limits.incMS[1]) && isDebugMode)
				std::cout << "info string missing/invalid binc value" << std::endl;
		}
		else if (kw == "movestogo") {
			if (!parseI32(limits.movesToGo) && isDebugMode)
				std::cout << "info string missing/invalid movestogo value" << std::endl;
		}
		else if (kw == "depth") {
			if (!parseI32(limits.depthLimit) && isDebugMode)
				std::cout << "info string missing/invalid depth value" << std::endl;
		}
		else if (kw == "nodes") {
			if (!parseI64(limits.nodeLimit) && isDebugMode)
				std::cout << "info string missing/invalid nodes value" << std::endl;
		}
		else if (kw == "mate") {
			if (!parseI32(limits.proveMateInN) && isDebugMode)
				std::cout << "info string missing/invalid mate value" << std::endl;
		}
		else if (kw == "movetime") {
			if (!parseI32(limits.moveTimeMS) && isDebugMode)
				std::cout << "info string missing/invalid movetime value" << std::endl;
		}
		else if (kw == "infinite") {
			limits.infinite = true;
		}
		else {
			if (isDebugMode) {
				std::cout << "info string unknown go-token '" << kw << "'" << std::endl;
			}
			return;
		}
	}

	searchManager.runSearch(pos, limits, recvTP, printBestMove);
}

void UciEngine::handleStopCmd(void) { searchManager.stopSearch(); }
