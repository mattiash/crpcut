CXXFLAGS=-ggdb -Wall -Wextra

testprog: unitt.o libciut.a
	g++ $(CXXFLAGS) unitt.o -L . -lciut -o testprog

libciut.a: ciut.o
	ar -r libciut.a ciut.o

unitt.o: unitt.cpp ciut.hpp
ciut.o: ciut.cpp ciut.hpp poll.hpp
	g++ -O3 -Wall -Wextra -c ciut.cpp
clean:
	rm -f *.a *.o *.rpo *.core testprog

