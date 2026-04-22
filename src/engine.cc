#include "engine.hpp"

#include <algorithm>
#include <chrono>

#include "eval.hpp"

// Returns the time budget in milliseconds for the current move.
// Returns 0 if there are no time controls
static int64_t computeTimeBudget(const GoLimits &goLimits, int engineColor) {
	if (goLimits.moveTimeMS > 0) {
		return goLimits.moveTimeMS;
	}

	const bool hasTimeControls = goLimits.timeLeftMS[0] > 0 || goLimits.timeLeftMS[1] > 0 ||
	                             goLimits.incMS[0] > 0 || goLimits.incMS[1] > 0;

	if (!hasTimeControls || goLimits.infinite || goLimits.ponder) {
		return 0;
	}

	const int64_t myTime = goLimits.timeLeftMS[engineColor];
	const int64_t myInc = goLimits.incMS[engineColor];

	// Time lost to UCI communication, OS scheduling, etc.
	constexpr int64_t overhead = 50;

	// Available time after reserving overhead
	const int64_t safeTime = std::max<int64_t>(1, myTime - overhead);

	int64_t budget;
	if (goLimits.movesToGo > 0) {
		// moves-to-go time control: divide remaining time over remaining moves
		budget = safeTime / goLimits.movesToGo + myInc * 3 / 4;
	}
	else {
		// sudden death (+ increment): assume ~25 moves left
		budget = safeTime / 25 + myInc * 3 / 4;
	}

	// never spend more than 50% of remaining time on a single move
	budget = std::min(budget, safeTime / 2);

	// ensure at least 1ms
	budget = std::max<int64_t>(budget, 1);

	return budget;
}

// Adjust mate scores for TT storage: make them ply-independent.
// Mating score (positive, ~+100M): add ply so stored value = raw base.
// Mated score (negative, ~-100M): subtract ply so stored value = raw base.
static constexpr Score MATE_THRESHOLD = 99'999'000;

static Score scoreToTT(Score score, int ply) {
	if (score > MATE_THRESHOLD) return score + ply;
	if (score < -MATE_THRESHOLD) return score - ply;
	return score;
}

static Score scoreFromTT(Score score, int ply) {
	if (score > MATE_THRESHOLD) return score - ply;
	if (score < -MATE_THRESHOLD) return score + ply;
	return score;
}

void Engine::startSearch(const Position &pos, TranspositionTable *tt, const GoLimits &limits,
                         std::chrono::time_point<std::chrono::steady_clock> commandReceiveTime) {
	// join any previous search thread before starting a new one
	if (searchThread.joinable()) {
		searchThread.join();
	}

	bestMove = Move();  // set bestMove to NULL
	searchPos = pos;
	searchTt = tt;
	nodesSearched = 0;
	maxNodes = limits.nodeLimit == -1 ? UINT64_MAX : static_cast<uint64_t>(limits.nodeLimit);

	searchPos.resetPly();   // make sure we start at 0 ply no matter what
	searchTt->newSearch();  // trigger a new search

	const int64_t budget = computeTimeBudget(limits, pos.usColor);
	hasDeadline = budget > 0;
	deadline = commandReceiveTime + std::chrono::milliseconds(budget);
	searchStopRequested = false;
	searchThread = std::thread([this, limits] {
		this->rootNegamax(limits);
		printSafe("bestmove ", bestMove.isNull() ? "0000" : bestMove.toLan());
	});
}

void Engine::stopSearch() {
	searchStopRequested = true;
	if (searchThread.joinable()) {
		searchThread.join();
	}
}

Move Engine::fetchBestMove() {
	stopSearch();
	return bestMove;
}

void Engine::rootNegamax(const GoLimits &limits) {
	MoveList legalMoves =
	    limits.searchMoves.size() > 0 ? limits.searchMoves : gen.generateLegalMoves();

	// no legal moves
	if (legalMoves.size() == 0) {
		return;
	}

	// put tt entry in the front (if exists)
	TTEntry entry;
	if (searchTt->probe(searchPos.hash, entry) && !entry.bestMove.isNull()) {
		for (size_t i = 0; i < legalMoves.size(); i++) {
			if (legalMoves[i] == entry.bestMove) {
				if (i != 0) {
					Move tmp = legalMoves[0];
					legalMoves[0] = legalMoves[i];
					legalMoves[i] = tmp;
				}
				break;
			}
		}
	}

	int depthLimit = limits.depthLimit > 0 ? std::min(MAX_PLY, limits.depthLimit) : MAX_PLY;
	if (limits.proveMateInN > 0) {
		depthLimit = std::min(depthLimit, limits.proveMateInN * 2);
	}

	for (int depth = 1; depth <= depthLimit; depth++) {
		Score bestChildScore = -INF;
		Move bestMoveFound;

		std::vector<std::pair<Move, Score>> childScores;
		childScores.reserve(legalMoves.size());

		Score alpha = -INF;
		bool aborted = false;

		for (const Move &move : legalMoves) {
			searchPos.makeMove(move);
			bool childAborted = false;
			Score childScore = -negamax(depth - 1, -INF, -alpha, childAborted);
			searchPos.undoMove();

			if (childAborted || searchStopRequested) {
				aborted = true;
				break;
			}

			alpha = std::max(alpha, childScore);

			if (childScore > bestChildScore) {
				bestChildScore = childScore;
				bestMoveFound = move;
			}

			childScores.emplace_back(move, childScore);
		}

		// safe to set because we always explore the best move (from previous iteration) first
		if (!bestMoveFound.isNull()) {
			bestMove = bestMoveFound;
			const Score storedScore = scoreToTT(bestChildScore, searchPos.ply);
			searchTt->store(searchPos.hash, depth, storedScore, TT_EXACT, bestMoveFound);
		}

		if (aborted) {
			break;
		}

		// simple move ordering
		std::stable_sort(childScores.begin(), childScores.end(),
		                 [](auto &a, auto &b) { return a.second > b.second; });
		for (size_t i = 0; i < childScores.size(); i++) {
			legalMoves[i] = childScores[i].first;
		}
	}
}

Score Engine::negamax(int depth, Score alpha, Score beta, bool &searchCancelledOut) {
	searchCancelledOut = false;

	if (nodesSearched >= maxNodes || searchStopRequested) {
		searchCancelledOut = true;
		return alpha;
	}

	nodesSearched++;

	// time polling every 2048 nodes
	if (hasDeadline && (nodesSearched & 2047) == 0) {
		if (std::chrono::steady_clock::now() >= deadline) {
			searchStopRequested = true;
			searchCancelledOut = true;
			return alpha;
		}
	}

	const int ply = searchPos.ply;
	const uint64_t key = searchPos.hash;
	const Score originalAlpha = alpha;

	Move ttMove;
	TTEntry entry;
	if (searchTt->probe(key, entry)) {
		if (!entry.bestMove.isNull()) {
			ttMove = entry.bestMove;
		}
		if (entry.depth >= depth) {
			const Score ttScore = scoreFromTT(entry.value, ply);
			if (entry.flag == TT_EXACT || (entry.flag == TT_LOWER && ttScore >= beta) ||
			    (entry.flag == TT_UPPER && ttScore <= alpha)) {
				return ttScore;
			}
		}
	}

	// quick bailout for terminal nodes
	MoveList legalMoves = gen.generateLegalMoves();
	if (legalMoves.size() == 0) {
		Score terminalScore = legalMoves.inCheck() ? MATED_SCORE + ply : 0;
		const Score storedScore = scoreToTT(terminalScore, ply);
		searchTt->store(key, depth, storedScore, TT_EXACT, Move());
		return terminalScore;
	}

	if (depth == 0) {
		Score evalScore = eval(searchPos);
		const Score storedScore = scoreToTT(evalScore, ply);
		searchTt->store(key, depth, storedScore, TT_EXACT, Move());
		return evalScore;
	}

	Score bestScore = -INF;
	Move bestMoveLocal;

	if (!ttMove.isNull()) {
		for (size_t i = 0; i < legalMoves.size(); i++) {
			if (legalMoves[i] == ttMove) {
				if (i != 0) {
					Move tmp = legalMoves[0];
					legalMoves[0] = legalMoves[i];
					legalMoves[i] = tmp;
				}
				break;
			}
		}
	}

	for (const Move &move : legalMoves) {
		searchPos.makeMove(move);
		bool childCancelled = false;
		Score childScore = -negamax(depth - 1, -beta, -alpha, childCancelled);
		searchPos.undoMove();

		if (childCancelled) {
			searchCancelledOut = true;
			return alpha;
		}

		if (childScore > bestScore || bestMoveLocal.isNull()) {
			bestScore = childScore;
			bestMoveLocal = move;
		}
		if (childScore > alpha) {
			alpha = childScore;
		}
		if (alpha >= beta) {
			break;
		}
	}

	TTFlag flag = TT_EXACT;
	if (bestScore <= originalAlpha) {
		flag = TT_UPPER;
	}
	else if (bestScore >= beta) {
		flag = TT_LOWER;
	}
	const Score storedScore = scoreToTT(bestScore, ply);
	searchTt->store(key, depth, storedScore, flag, bestMoveLocal);

	return bestScore;
}
