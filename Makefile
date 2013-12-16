.PHONY : test
test : build
	PYTHONPATH=$$(ls -d build/lib.*):$$PYTHONPATH python tests/xdelta3_test.py
	
.PHONY : build
build :
	python setup.py build
