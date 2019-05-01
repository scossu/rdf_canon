default:
	gcc -Wall rdf_canon.c test.c -I/usr/include/raptor2 \
	-I/usr/include/rasqal -I. -lrasqal -lraptor2 -lrdf -lcork -o test.o

