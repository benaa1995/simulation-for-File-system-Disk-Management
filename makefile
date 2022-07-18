all:code_stub.cpp  
	g++ code_stub.cpp -o final
all-GDB: code_stub.cpp 
	g++ -g code_stub.cpp -o final-debug
