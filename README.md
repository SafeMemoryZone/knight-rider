# knight-rider

A lightweight chess engine written in C++ with a focus on clean code and solid tactical play.

## Performance test

Tested on a MacBook Air (Apple M2, 8 cores: 4 performance + 4 efficiency, 16 GB RAM).  
Compiled with: `-O3 -DNDEBUG -flto -march=native`.
On this setup, knight-rider achieves about 342M nodes/second in perft.

## License

This project is licensed under the MIT License.

