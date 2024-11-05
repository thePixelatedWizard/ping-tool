default:
	clang++ -c -g main.cpp ping.cpp ip_checksum.cpp
	clang++ -g -o ping-tool.exe main.o ping.o ip_checksum.o -lws2_32

clean:
	rm *.o
	rm *.exe
