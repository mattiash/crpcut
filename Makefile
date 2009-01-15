CXXFLAGS=-Iinclude -ggdb -Wall -Wextra -Wno-variadic-macros -pipe -pedantic

all:    test doc

test:   test/testprog
doc:	html/doc.html


test/testprog: test/unitt.o lib/libcrpcut.a
	@[ -d test ] || mkdir test
	g++ $(CXXFLAGS) test/unitt.o -L lib -lcrpcut -lrt -o test/testprog

lib/libcrpcut.a: obj/crpcut.o
	@[ -d lib ] || mkdir lib
	ar -r lib/libcrpcut.a obj/crpcut.o

test/unitt.o: test-src/unitt.cpp include/crpcut.hpp include/array_v.hpp
	@[ -d test ] || mkdir test
	g++ $(CXXFLAGS) test-src/unitt.cpp -c -o test/unitt.o

obj/crpcut.o: src/crpcut.cpp include/crpcut.hpp src/poll.hpp include/array_v.hpp
	@[ -d obj ] || mkdir obj
	g++ $(CXXFLAGS) src/crpcut.cpp -c -o obj/crpcut.o

html/doc.html: doc-src/doc.xml doc-src/doc2html.xsl
	@[ -d html ] || mkdir html
	xsltproc doc-src/doc2html.xsl doc-src/doc.xml > html/doc.html

clean:
	rm -f */*.o */*.a */*.html test/testprog

