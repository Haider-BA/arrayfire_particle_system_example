cpu:
	g++ -std=c++11 -lafcpu -o part_cpu particle_engine.cpp -O2
cuda:
	g++ -std=c++11 -lafcuda -o part_cuda particle_engine.cpp -O2
opencl:
	g++ -std=c++11 -lafopencl -o part_opencl particle_engine.cpp -O2

