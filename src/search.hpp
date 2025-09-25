#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <thread>

#include "move.hpp"
#include "movegen.hpp"
#include "movelist.hpp"
#include "position.hpp"

struct GoLimits {
	int64_t timeLeftMS[2];
	int64_t nodeLimit;
	int incMS[2], movesToGo, depthLimit, proveMateInN, moveTimeMS;
	bool infinite, ponder;
	MoveList searchMoves;
};

class SearchEngine {
   public:
	SearchEngine(void) = default;

	void search(const Position &searchPosition, const GoLimits &goLimits);

	template <bool HasNodeLimit>
	Score coreSearch(int depth, Score alpha, Score beta);

	Move fetchBestMove(void) const;

	std::atomic<bool> requestedStop;

   private:
	Move bestMove;
	Position position;
	int64_t nodesRemaining;  // only used when there's a node limit
	MoveGenerator moveGenerator = MoveGenerator(&position);
};

class SearchManager {
   public:
	SearchManager(void) = default;
	~SearchManager(void);

	void timeControlManager(const GoLimits &goLimits,
	                        std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP,
	                        int engineColor);

	void runSearch(const Position &searchPosition, const GoLimits &goLimits,
	               std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP);
	Move stopSearch(void);  // returns best move

   private:
	std::thread searchThread;
	std::thread timerThread;

	std::condition_variable timeControlCV;
	std::mutex timeControlMtx;
	bool timeControlWakeRequested;

	SearchEngine searchEngine;
};

#endif  // SEARCH_HPP
