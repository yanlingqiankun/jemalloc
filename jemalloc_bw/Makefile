all: bandwidth_detection.c
	gcc -I../include bandwidth_detection.c ../src/cpu.c ../src/systemcall.c ../src/monitor.c -lpthread -o bandwidth_detection

clean: 
	rm bandwidth_detection