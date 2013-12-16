import unittest
import xdelta3

class Xdelta3TestCase(unittest.TestCase):
  
  def test_test(self):
    x = xdelta3.Xdelta3(None, None)
    self.assertEqual(None, x.test())
    
if __name__ == '__main__':
  unittest.main()
