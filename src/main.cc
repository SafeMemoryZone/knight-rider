#include "uci.hpp"

std::mutex printMutex;

int main() {
	UciEngine uciEngine;
	uciEngine.start();

	return 0;
}
