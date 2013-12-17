FIXTURES=fixtures/wget-1.11.tar fixtures/wget-1.11.4.tar \
	fixtures/wget-1.11-1.11.4.patch

.PHONY : test
test : build $(FIXTURES)
	PYTHONPATH=$$(ls -d build/lib.*):$$PYTHONPATH python tests/xdelta3_test.py
	
.PHONY : build
build :
	python setup.py build
	
fixtures/wget-1.11.tar :
	wget -O $@.bz2 --progress=dot:binary http://ftp.gnu.org/gnu/wget/wget-1.11.tar.bz2
	bunzip2 $@.bz2

fixtures/wget-1.11.4.tar :
	wget -O $@.bz2 --progress=dot:binary http://ftp.gnu.org/gnu/wget/wget-1.11.4.tar.bz2
	bunzip2 $@.bz2

fixtures/wget-1.11-1.11.4.patch : fixtures/wget-1.11.tar fixtures/wget-1.11.4.tar
	xdelta3 -e -W 32768 -s $^ $@

