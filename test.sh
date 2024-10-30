zig cc ./tensor.c ./tests/simulate-ops-cpu.c ./prng/pcg.c -o simulate-ops-cpu -lm -lOpenCL -Wall -Wextra -pedantic -ggdb
./simulate-ops-cpu
zig cc ./tensor.c ./tests/simulate-linearized.c ./prng/pcg.c -o simulate-linearized -lm -lOpenCL -Wall -Wextra -pedantic -ggdb
./simulate-linearized 1000 10
zig cc ./tensor.c ./compiler/codegen.c ./compiler/compile.c ./runtimes/cl.c ./tests/simulate-compiler.c ./utils.c ./prng/pcg.c -o simulate-compiler -lm -lOpenCL -Wall -Wextra -pedantic -ggdb
./simulate-compiler
