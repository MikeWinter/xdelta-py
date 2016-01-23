import django.core.files as files

import _xdelta


class DeltaFile(files.File):
    """
    The DeltaFile class allows the manipulation of delta-compressed files.

    When associated with a source file---typically some other version of the information---data that is written to a
    DeltaFile will be encoded as differences against that source. Similarly, data read from a DeltaFile will be decoded
    with reference to the source file. It is also possible to compress a file without a secondary source file similar to
    other archival methods. However, if a secondary source was used during encoding, it will be required for decoding as
    well.

    The following example demonstrates encoding:

        # Create a file for storing the encoding.
        target = open('example.2.diff', 'wb')
        with DeltaFile(target) as df:
            # Optional: Specify data from another version.
            df.source = open('example.1.txt', 'r')
            # Add data for encoding. The write method may be called multiple
            # times.
            df.write(data)

    Decoding is analogous:

        # Open the encoded file.
        diff = open('example.2.diff', 'rb')
        with DeltaFile(diff) as df:
            # Optional: Specify data from another version.
            df.source = open('example.1.txt', 'r')
            # Get decoded data. The read method may be called multiple times.
            data = df.read()

    It should be noted that while multiple DeltaFile objects may be chained by setting an existing instance to be the
    source file of another, this may result in high memory consumption as these files will need to be decoded
    on-the-fly in order to provide the necessary decoded data.

    Use of built-in file object methods, such as seek and readline, may result in undefined behaviour and should be
    avoided.
    """
    DEFAULT_CHUNK_SIZE = 8 * 2**20
    """Default chunks to 8 MB."""

    def __init__(self, file, name=None):
        super(DeltaFile, self).__init__(file, name)
        self._stream = None

    def _get_source(self):
        """The file against which differences will be calculated during encoding."""
        return self._stream.source if self._stream else None

    def _set_source(self, source):
        if not self._stream:
            self._stream = _xdelta.Stream(self.file, source)
        else:
            self._stream.source = source
    source = property(_get_source, _set_source)

    def open(self, mode=None):
        super(DeltaFile, self).open(mode)
        self._stream = _xdelta.Stream(self.file, self._stream.source)

    def read(self, num_bytes=-1):
        """
        Read content from the file.

        The optional size is the number of bytes to read; if not specified, the file will be read to the end.
        """
        if not self._stream:
            self._stream = _xdelta.Stream(self.file)
        return self._stream.read(num_bytes)

    def write(self, content):
        """
        Writes the specified content string to the file.

        Depending on the storage system behind the scenes, this content might not be fully committed until close() is
        called on the file.
        """
        if not self._stream:
            self._stream = _xdelta.Stream(self.file)
        self._stream.write(content)

    def flush(self):
        """
        Write any pending data to output.
        """
        if not self._stream:
            self._stream = _xdelta.Stream(self.file)
        self._stream.flush()
        super(DeltaFile, self).flush()

    def close(self):
        if self._stream:
            self.flush()
            self._stream = None
        self.source = None
        super(DeltaFile, self).close()

    def multiple_chunks(self, chunk_size=None):
        """
        Returns True if the file is large enough to require multiple chunks to access all of its content given some
        chunk_size.
        """
        if chunk_size is None:
            chunk_size = self.DEFAULT_CHUNK_SIZE
        try:
            return self.decoded_size > chunk_size
        except AttributeError:
            # If the decoded size is not available, the encoded size may still provide a clue as to the viability of
            # chunking.
            return super(DeltaFile, self).multiple_chunks(chunk_size)
