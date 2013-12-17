import _xdelta3

BLOCK_SIZE = 65536 * 2

class Decoder(object):
  
  def __init__(self, reader, writer):
    self._reader = reader
    self._writer = writer
    
    self._stream = _xdelta3.Stream(BLOCK_SIZE)
    self._source = _xdelta3.Source(BLOCK_SIZE)
    self._stream.set_source(self._source)
    
  def input(self, data):
    self._stream.avail_input(data)
    
    while True:
      ret = self._stream.decode_input()
      
      if ret == _xdelta3.INPUT:
        return
        
      if ret == _xdelta3.OUTPUT:
        self._writer(self._stream.next_out)
        self._stream.consume_output()
        
      if ret == _xdelta3.GETSRCBLK:
        self._source.set_curblk(self._source.getblkno,
            self._reader(self._source.getblkno, BLOCK_SIZE))
