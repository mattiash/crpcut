CXXFLAGS=-ggdb -Wall -Wextra -Wno-variadic-macros -Wno-non-virtual-dtor -pipe

testprog: unitt.o libciut.a
	g++ $(CXXFLAGS) unitt.o -L . -lciut -lrt -o testprog

libciut.a: ciut.o
	ar -r libciut.a ciut.o

unitt.o: unitt.cpp ciut.hpp
ciut.o: ciut.cpp ciut.hpp poll.hpp array_v.hpp
#	g++ -O3 -Wall -Wextra -Wno-variadic-macros -Wno-non-virtual-dtor -s -c ciut.cpp
clean:
	rm -f *.a *.o *.rpo *.core testprog

