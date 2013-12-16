import unittest
import xdelta3

class Xdelta3TestCase(unittest.TestCase):
  
  def test_test(self):
    self.assertEqual(None, xdelta3.test())
    
if __name__ == '__main__':
  unittest.main()
