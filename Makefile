compile:	
	g++ -framework GLUT -framework OpenGL -framework OpenCL main.cpp -o main
clean:
	rm -f main
