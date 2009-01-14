CXXFLAGS=-ggdb -Wall -Wextra -Wno-variadic-macros -pipe -pedantic

testprog: unitt.o libcrpcut.a
	g++ $(CXXFLAGS) unitt.o -L . -lcrpcut -lrt -o testprog

libcrpcut.a: crpcut.o
	ar -r libcrpcut.a crpcut.o

unitt.o: unitt.cpp crpcut.hpp
crpcut.o: crpcut.cpp crpcut.hpp poll.hpp array_v.hpp
#	g++ -O3 -Wall -Wextra -Wno-variadic-macros -s -c crpcut.cpp
clean:
	rm -f *.a *.o *.rpo *.core testprog

