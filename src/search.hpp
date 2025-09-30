#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "move.hpp"
#include "movegen.hpp"
#include "movelist.hpp"
#include "position.hpp"

// search limits & options
struct GoLimits {
	int64_t timeLeftMS[2];
	int64_t nodeLimit;
	int incMS[2], movesToGo, depthLimit, proveMateInN, moveTimeMS;
	bool infinite, ponder;
	MoveList searchMoves;
};

// single-threaded search engine
class SearchEngine {
   public:
	SearchEngine(void) = default;

	void search(const Position& searchPosition, const GoLimits& goLimits);

	template <bool HasNodeLimit>
	Score coreSearch(int depth, Score alpha, Score beta);

	Move fetchBestMove(void) const;

	std::atomic<bool> requestedStop;

   private:
	Move bestMove;
	Position position;
	int64_t nodesRemaining;  // used only when a node limit is set
	MoveGenerator moveGenerator = MoveGenerator(&position);
};

// manager for running search + time control
class SearchManager {
   public:
	SearchManager(void) = default;
	~SearchManager(void);

	void runSearch(const Position& searchPosition, const GoLimits& goLimits,
	               std::chrono::time_point<std::chrono::steady_clock> commandReceiveT,
	               std::function<void(Move)> onFinishCallbackP);

	void stopSearch(void);      // returns best move
	void blockUntilDone(void);  // wait for completion and return best move

   private:
	void timeControlManager(const GoLimits& goLimits,
	                        std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP,
	                        int engineColor);

	// threads
	std::thread searchThread;
	std::thread timerThread;

	// time-control signaling
	std::condition_variable timeControlCV;
	std::mutex timeControlMtx;
	bool timeControlWakeRequested;

	// worker
	SearchEngine searchEngine;
};

#endif  // SEARCH_HPP
