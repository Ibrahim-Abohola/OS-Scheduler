build:
	gcc  process_generator.c clk_functions.c -o process_generator.out
	gcc  clk.c -o clk.out
	gcc scheduler.c PrioQueue.c DataStructures.c clk_functions.c HPF.c circQ.c RR.c -o scheduler.out -lm
	gcc  process.c clk_functions.c  -o process.out
	gcc  test_generator.c -o test_generator.out

clean:
	rm -f *.out  

all: clean build

run:
	./process_generator.out