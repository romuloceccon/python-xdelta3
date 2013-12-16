build :
	python setup.py build

test : build
	PYTHONPATH=$$(ls -d build/lib.*):$$PYTHONPATH python tests/xdelta3_test.py
