CXXFLAGS=-ggdb -Wall -Wextra

testprog: unitt.o ciut.o
	g++ $(CXXFLAGS) unitt.o ciut.o -o testprog

unitt.o: unitt.cpp ciut.hpp
ciut.o: ciut.hpp ciut.hpp

clean:
	rm -f *.o *.rpo *.core testprog

