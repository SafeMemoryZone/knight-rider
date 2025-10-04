#include "uci.hpp"

#include <cctype>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bitboards.hpp"
#include "perft.hpp"
#include "zobrist.hpp"

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

static void printBestMove(Move move) { printSafe("bestmove ", move.toLan()); }

void UciEngine::preUciInit(void) {
	initBitboards();
	initZobristTables();
	tt.resize(10);  // 10mib default size
}

void UciEngine::start(void) {
	preUciInit();

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
			handleSetoptionCmd();
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
				printSafe("info string 'ponderhit' not implemented yet");
			}
		}
		else if (cmd == "stop") {
			handleStopCmd();
		}
		else if (cmd == "quit") {
			searchManager.stopSearch();
			return;
		}
	}
}

void UciEngine::handleUciCmd(void) {
	printSafe("id name Knightrider");
	printSafe("id author Viliam Holly");
	printSafe("option name Hash type spin default 10 min 1 max 131072");
	printSafe("option name Clear Hash type button");
	printSafe("uciok");
}

void UciEngine::handleDebugCmd(void) {
	tokenPos = 1;
	if (tokenPos >= lowerTokens.size()) {
		if (isDebugMode) {
			printSafe("info string missing argument");
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
			printSafe("info string expected 'on' or 'off'");
		}
	}
}

void UciEngine::handleIsReadyCmd(void) { printSafe("readyok"); }

void UciEngine::handleUcinewgameCmd(void) {
	pos = Position();
	if (isDebugMode) {
		printSafe("info string new UCI game initialized");
	}
}

void UciEngine::handlePositionCmd(void) {
	// position [startpos | fen <FEN...>] [moves <m1> <m2> ...]
	tokenPos = 1;
	if (tokenPos >= lowerTokens.size()) {
		if (isDebugMode) {
			printSafe("info string missing argument");
		}
		return;
	}

	// startpos
	if (lowerTokens.at(tokenPos) == "startpos") {
		pos = Position();

		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				printSafe("info string position set");
			}
			return;
		}
	}
	// FEN position
	else if (lowerTokens.at(tokenPos) == "fen") {
		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				printSafe("info string missing FEN");
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
				printSafe("info string invalid FEN string");
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
				printSafe("info string invalid FEN string");
			}
			return;
		}
		tokenPos = fenEnd;
	}
	else {
		if (isDebugMode) {
			printSafe("info string expected 'startpos' or 'fen'");
		}
		return;
	}

	// move list (optional)
	if (tokenPos < lowerTokens.size() && lowerTokens.at(tokenPos) == "moves") {
		if (!advanceIfPossible(lowerTokens, tokenPos)) {
			if (isDebugMode) {
				printSafe("info string position set");
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
				printSafe("info string illegal or unknown move: ", lan);
			}

			tokenPos++;
		}
	}

	if (isDebugMode) {
		printSafe("info string position set");
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
			out = std::stoi(tokens[tokenPos++]);
			return true;
		} catch (...) {
			return false;
		}
	};
	auto parseI64 = [&](int64_t& out) -> bool {
		if (tokenPos >= tokens.size()) return false;
		try {
			out = std::stoll(tokens[tokenPos++]);
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

	bool isPerft = false;
	int perftDepth = 0;

	while (tokenPos < lowerTokens.size()) {
		const std::string& kw = lowerTokens[tokenPos++];
		if (kw == "searchmoves") {
			// get move tokens until we hit another keyword or end
			while (tokenPos < lowerTokens.size() && !isKeyword(lowerTokens[tokenPos])) {
				const std::string& mvTxt = tokens[tokenPos];
				if (!addSearchMove(mvTxt) && isDebugMode) {
					printSafe("info string ignoring unknown searchmove '", mvTxt, "'");
				}
				tokenPos++;
			}
		}
		else if (kw == "ponder") {
			limits.ponder = true;
		}
		else if (kw == "wtime") {
			if (!parseI64(limits.timeLeftMS[0]) && isDebugMode)
				printSafe("info string missing/invalid wtime value");
		}
		else if (kw == "btime") {
			if (!parseI64(limits.timeLeftMS[1]) && isDebugMode)
				printSafe("info string missing/invalid btime value");
		}
		else if (kw == "winc") {
			if (!parseI32(limits.incMS[0]) && isDebugMode)
				printSafe("info string missing/invalid winc value");
		}
		else if (kw == "binc") {
			if (!parseI32(limits.incMS[1]) && isDebugMode)
				printSafe("info string missing/invalid binc value");
		}
		else if (kw == "movestogo") {
			if (!parseI32(limits.movesToGo) && isDebugMode)
				printSafe("info string missing/invalid movestogo value");
		}
		else if (kw == "depth") {
			if (!parseI32(limits.depthLimit) && isDebugMode)
				printSafe("info string missing/invalid depth value");
		}
		else if (kw == "nodes") {
			if (!parseI64(limits.nodeLimit) && isDebugMode)
				printSafe("info string missing/invalid nodes value");
		}
		else if (kw == "mate") {
			if (!parseI32(limits.proveMateInN) && isDebugMode)
				printSafe("info string missing/invalid mate value");
		}
		else if (kw == "movetime") {
			if (!parseI32(limits.moveTimeMS) && isDebugMode)
				printSafe("info string missing/invalid movetime value");
		}
		else if (kw == "infinite") {
			limits.infinite = true;
		}
		else if (kw == "perft") {
			if (!parseI32(perftDepth) && isDebugMode)
				printSafe("info string missing depth parameter");
			isPerft = true;
		}
		else {
			if (isDebugMode) {
				printSafe("info string unknown go-token '", kw, "'");
			}
			return;
		}
	}

	if (isPerft) {
		auto startTime = std::chrono::high_resolution_clock::now();
		size_t nodes = perft(pos, perftDepth, true);
		auto endTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsedTime = endTime - startTime;

		std::cout << "\nNodes searched: " << nodes << " in " << elapsedTime << '\n' << std::endl;
	}
	else {
		searchManager.runSearch(pos, limits, recvTP, printBestMove, &tt);
	}
}

void UciEngine::handleStopCmd(void) { searchManager.stopSearch(); }

void UciEngine::handleSetoptionCmd(void) {
	// setoption name <id> [value <x>]
	tokenPos = 1;
	if (tokenPos >= lowerTokens.size() || lowerTokens[tokenPos] != "name") {
		if (isDebugMode) {
			printSafe("info string setoption: expected 'name'");
		}
		return;
	}
	tokenPos++;

	std::string name, lname;
	while (tokenPos < tokens.size() && lowerTokens[tokenPos] != "value") {
		if (!name.empty()) {
			name.push_back(' ');
			lname.push_back(' ');
		}
		name += tokens[tokenPos];
		lname += lowerTokens[tokenPos];
		tokenPos++;
	}

	// optional value
	std::string value;
	if (tokenPos < tokens.size() && lowerTokens[tokenPos] == "value") {
		tokenPos++;
		// collect the rest as value
		while (tokenPos < tokens.size()) {
			if (!value.empty()) value.push_back(' ');
			value += tokens[tokenPos++];
		}
	}

	// known options
	if (lname == "hash") {
		if (value.empty()) {
			if (isDebugMode) {
				printSafe("info string setoption Hash: missing value");
			}
			return;
		}
		try {
			long long mib = std::stoll(value);
			if (mib < 1) mib = 1;
			if (mib > 131072) mib = 131072;

			tt.resize(static_cast<std::size_t>(mib));

			if (isDebugMode) {
				printSafe("info string TT resized to ", std::to_string(mib), " MiB");
			}
		} catch (...) {
			if (isDebugMode) {
				printSafe("info string setoption Hash: invalid value '", value, "'");
			}
		}
	}
	else if (lname == "clear hash") {
		tt.clear();
		if (isDebugMode) {
			printSafe("info string TT cleared");
		}
	}
	else {
		if (isDebugMode) {
			printSafe("info string setoption: unknown option '", name, "'");
		}
	}
}
