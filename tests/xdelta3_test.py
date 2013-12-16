import unittest
import xdelta3
import hashlib

class SourceReader(object):
  
  def __init__(self, f):
    self._f = f
    
  def read(self, block, block_size):
    self._f.seek(block * block_size)
    return self._f.read(block_size)

class Xdelta3TestCase(unittest.TestCase):
  
  def test_test(self):
    with open('fixtures/source.dat') as source:
      with open('test.dat', 'w+') as output:
        x = xdelta3.Xdelta3(SourceReader(source).read, output.write)
        with open('fixtures/input.dat') as _input:
          while True:
            data = _input.read(10000)
            if not data:
              break
            x.input(data)
            
    with open('test.dat') as f:
      h1 = hashlib.new('md5')
      h1.update(f.read())
      
    with open('fixtures/output.dat') as f:
      h2 = hashlib.new('md5')
      h2.update(f.read())
      
    self.assertEqual(h2.digest(), h1.digest())
            
if __name__ == '__main__':
  unittest.main()
