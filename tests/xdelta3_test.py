import unittest
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
    xdelta3.Stream(32768)
    
  def test_input(self):
    with open('fixtures/wget-1.11.tar') as source:
      with open('test.tmp', 'w+') as output:
        x = xdelta3.Xdelta3(SourceReader(source).read, output.write)
        with open('fixtures/wget-1.11-1.11.4.patch') as _input:
          while True:
            data = _input.read(16384)
            if not data:
              break
            x.input(data)
            
    self.assertEqual(self.calc_md5('test.tmp'),
        self.calc_md5('fixtures/wget-1.11.4.tar'))
    
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
