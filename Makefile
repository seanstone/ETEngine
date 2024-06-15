.PHONY: Demo
Demo: Projects/Demo/build/Makefile
	cmake --build Projects/Demo/build --target all --config Develop

Projects/Demo/build/Makefile:
	cmake -S Projects/Demo -B Projects/Demo/build

.PHONY: clean
clean:
	rm -rf Projects/Demo/build