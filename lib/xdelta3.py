import _xdelta3

class Decoder(object):
  
  def __init__(self, reader, writer):
    self._reader = reader
    self._writer = writer
    
    self._stream = _xdelta3.Stream()
    self._source = _xdelta3.Source()
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
            self._reader(self._source.getblkno, self._source.blksize))
        
class Encoder(object):

  def __init__(self, reader, writer, winsize=32768):
    self._reader = reader
    self._writer = writer

    self._stream = _xdelta3.Stream(winsize)
    self._source = _xdelta3.Source()
    self._stream.set_source(self._source)

  def input(self, data, flush=False):
    if flush:
      flag = _xdelta3.FLUSH
    else:
      flag = 0

    self._stream.flags = self._stream.flags & ~_xdelta3.FLUSH | flag
    self._stream.avail_input(data)

    while True:
      ret = self._stream.encode_input()

      if ret == _xdelta3.INPUT:
        return

      if ret == _xdelta3.OUTPUT:
        self._writer(self._stream.next_out)
        self._stream.consume_output()

      if ret == _xdelta3.GETSRCBLK:
        self._source.set_curblk(self._source.getblkno,
            self._reader(self._source.getblkno, self._source.blksize))

