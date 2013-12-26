import unittest
import _xdelta3
import xdelta3
import hashlib
import os

class SourceReader(object):
  
  def __init__(self, f):
    self._f = f
    
  def read(self, block, block_size):
    self._f.seek(block * block_size)
    return self._f.read(block_size)
    
class Xdelta3TestCase(unittest.TestCase):
  
  def tearDown(self):
    if os.path.exists('test.tmp'):
      os.remove('test.tmp')
      
  def test_create_stream(self):
    _xdelta3.Stream(32768)
    
  def test_set_source(self):
    stream = _xdelta3.Stream(32768)
    source = _xdelta3.Source(32768)
    stream.set_source(source)
    self.assertEqual(source, stream.src)
    
  def test_create_source(self):
    s = _xdelta3.Source(32768)
    self.assertEqual(0, s.getblkno)
    s.set_curblk(10, "x" * 1024)
    s.set_curblk(11, "y" * 1024)
    
  def test_input(self):
    with open('fixtures/wget-1.11.tar') as source:
      with open('test.tmp', 'w+') as output:
        x = xdelta3.Decoder(SourceReader(source).read, output.write)
        with open('fixtures/wget-1.11-1.11.4.patch') as _input:
          while True:
            data = _input.read(16384)
            if not data:
              break
            x.input(data)
            
    self.assertEqual(self.calc_md5('test.tmp'),
        self.calc_md5('fixtures/wget-1.11.4.tar'))
    
  def test_encode(self):
    winsize = 32768
    
    with open('fixtures/wget-1.11.tar') as source:
      with open('test.tmp', 'w+') as output:
        x = xdelta3.Encoder(SourceReader(source).read, output.write, winsize)
        with open('fixtures/wget-1.11.4.tar') as _input:
          while True:
            data = _input.read(winsize)
            if not data:
              x.input('', True)
              break
            x.input(data)
            
    sz = os.path.getsize('test.tmp')
    self.assert_(sz > 100000)
    self.assert_(sz < 150000)
    
  def calc_md5(self, filename):
    with open(filename) as f:
      h = hashlib.new('md5')
      while True:
        s = f.read(65536)
        if not s:
          break
        h.update(s)
      return h.digest()
            
if __name__ == '__main__':
  unittest.main()
