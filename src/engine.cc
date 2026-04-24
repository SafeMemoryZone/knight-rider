#include "engine.hpp"

#include <algorithm>
#include <chrono>

#include "eval.hpp"

// Returns the time budget in milliseconds for the current move.
// Returns 0 if there are no time controls
static int64_t computeTimeBudget(const GoLimits &goLimits, int engineColor) {
	// Time lost to UCI communication, OS scheduling, etc.
	constexpr int64_t overhead = 50;

	if (goLimits.moveTimeMS > 0) {
		return std::max<int64_t>(1, goLimits.moveTimeMS - overhead);
	}

	const bool hasTimeControls = goLimits.timeLeftMS[0] > 0 || goLimits.timeLeftMS[1] > 0 ||
	                             goLimits.incMS[0] > 0 || goLimits.incMS[1] > 0;

	if (!hasTimeControls || goLimits.infinite || goLimits.ponder) {
		return 0;
	}

	const int64_t myTime = goLimits.timeLeftMS[engineColor];
	const int64_t myInc = goLimits.incMS[engineColor];

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

static int pieceTypeOn(const Position &pos, Bitboard sq, int color) {
	for (int pt = 0; pt < 6; pt++) {
		if (pos.pieces[color * 6 + pt] & sq) return pt;
	}
	return PT_NULL;
}

static void scoreMoves(const MoveList &moves, int scores[], const Position &pos, Move ttMove) {
	// MVV-LVA[victim][attacker]
	static constexpr int MVV_LVA[6][6] = {
	    /* victim P */ {0, -220, -230, -400, -800, -19900},
	    /* victim N */ {220, 0, -10, -180, -580, -19680},
	    /* victim B */ {230, 10, 0, -170, -570, -19670},
	    /* victim R */ {400, 180, 170, 0, -400, -19500},
	    /* victim Q */ {800, 580, 570, 400, 0, -19100},
	};

	static constexpr int TT_MOVE_SCORE = 10'000'000;
	static constexpr int CAPTURE_BASE = 1'000'000;

	for (size_t i = 0; i < moves.size(); i++) {
		const Move &m = moves[i];

		if (m == ttMove) {
			scores[i] = TT_MOVE_SCORE;
			continue;
		}

		int victim = m.getIsEp() ? PT_PAWN : pieceTypeOn(pos, m.getTo(), pos.oppColor);

		if (victim != PT_NULL) {
			scores[i] = CAPTURE_BASE + MVV_LVA[victim][m.getMovingPt()];
		}
		else {
			scores[i] = 0;
		}
	}
}

// Partial selection sort: swap the highest-scored remaining move into position i
static void pickNext(MoveList &moves, int scores[], size_t i) {
	size_t best = i;
	for (size_t j = i + 1; j < moves.size(); j++) {
		if (scores[j] > scores[best]) {
			best = j;
		}
	}
	if (best != i) {
		Move tmp = moves[best];
		moves[best] = moves[i];
		moves[i] = tmp;
		int ts = scores[best];
		scores[best] = scores[i];
		scores[i] = ts;
	}
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

		// update bestMove even on partial iterations (first move from prev iter is reliable)
		if (!bestMoveFound.isNull()) {
			bestMove = bestMoveFound;
		}

		if (aborted) {
			break;
		}

		// only store TT_EXACT after a fully completed iteration
		const Score storedScore = scoreToTT(bestChildScore, searchPos.ply);
		searchTt->store(searchPos.hash, depth, storedScore, TT_EXACT, bestMoveFound);

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
		bool quiescenceCancelled = false;
		// WARN: do NOT invert alpha and beta here
		Score evalScore = quiescence(alpha, beta, quiescenceCancelled);

		if (quiescenceCancelled) {
			searchCancelledOut = true;
			return alpha;
		}

		TTFlag flag = TT_EXACT;
		if (evalScore <= originalAlpha) {
			flag = TT_UPPER;  // Fail-Low (we didn't beat alpha)
		}
		else if (evalScore >= beta) {
			flag = TT_LOWER;  // Fail-High (beta cutoff)
		}

		const Score storedScore = scoreToTT(evalScore, ply);
		searchTt->store(key, depth, storedScore, flag, Move());
		return evalScore;
	}

	Score bestScore = -INF;
	Move bestMoveLocal;

	int moveScores[MAX_MOVES];
	scoreMoves(legalMoves, moveScores, searchPos, ttMove);

	for (size_t i = 0; i < legalMoves.size(); i++) {
		pickNext(legalMoves, moveScores, i);
		const Move &move = legalMoves[i];

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

Score Engine::quiescence(Score alpha, Score beta, bool &searchCancelledOut) {
	searchCancelledOut = false;

	if (nodesSearched >= maxNodes || searchStopRequested) {
		searchCancelledOut = true;
		return alpha;
	}

	nodesSearched++;

	if (hasDeadline && (nodesSearched & 4095) == 0) {
		if (std::chrono::steady_clock::now() >= deadline) {
			searchStopRequested = true;
			searchCancelledOut = true;
			return alpha;
		}
	}

	// generate captures to detect check status
	MoveList moves = gen.generateLegalMoves(true);
	bool inCheck = moves.inCheck();

	// in check: need ALL legal moves (evasions), not just captures
	if (inCheck) {
		moves = gen.generateLegalMoves();
		if (moves.size() == 0) {
			return MATED_SCORE + searchPos.ply;
		}
	}

	Score bestScore;
	if (inCheck) {
		bestScore = -INF;  // no stand-pat when in check — must escape
	}
	else {
		bestScore = eval(searchPos);
		if (bestScore >= beta) return bestScore;
		if (bestScore > alpha) alpha = bestScore;
	}

	int moveScores[MAX_MOVES];
	scoreMoves(moves, moveScores, searchPos, Move());

	for (size_t i = 0; i < moves.size(); i++) {
		pickNext(moves, moveScores, i);
		const Move &m = moves[i];

		searchPos.makeMove(m);
		bool childCancelled = false;
		Score score = -quiescence(-beta, -alpha, childCancelled);
		searchPos.undoMove();

		if (childCancelled) {
			searchCancelledOut = true;
			return alpha;
		}

		if (score >= beta) {
			return score;
		}
		if (score > bestScore) {
			bestScore = score;
		}
		if (score > alpha) {
			alpha = score;
		}
	}

	return bestScore;
}
