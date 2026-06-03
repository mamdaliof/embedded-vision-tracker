test_lut: test_lut.cpp
	g++ -O3 test_lut.cpp -o test_lut

test_yuv: test_yuv.cpp yuv_process.hpp
	g++ -O3 test_yuv.cpp -o test_yuv `pkg-config --cflags --libs opencv4`

run_test: test_lut test_yuv
	./test_lut
	./test_yuv

clean:
	rm -f test_lut test_yuv
