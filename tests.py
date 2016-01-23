import io
from unittest import TestCase
from xdelta import DeltaFile

class DeltaFileTest(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ENCODED = b"\xd6\xc3\xc4\x00\x01\x01\x04\x83\x18\x85U\x01\x81|U<>Q\x0ca\x83\x14`bf\x15\x16Q\x01|K\x8c\xc7" \
                      b"\x08\xd6\x87\x81$111b[r\xcfW\xa6j\x1fd\x04\x02\xfe\x94\xcb\xc9,\xc7\xbd=\x05s\x9bFR\xe1\x8c" \
                      b"\x8a\x03\xa1\x03j:ueM\xac2\x13\xbc\x9c\xeb\xe2\xc3\x14mx\xb6U\xde\x92\xb0\xeaY\x81A\x99\x07" \
                      b"\x82\t\x99\x9a\xb8\xdf\xe2\x9a.X\x0c\xc3\xbd\xa3\xd4\x15zi6\xfc\x9ee{\x81+\xd5\x87~J\x0f|\x8a" \
                      b"\xa1\xc6l\xf8yK\xa8Y\x85K~J&.\x97\x87C\x0eTBz\xaf\x19\xb2\x1a\xfe\xeb\xc0\xe2\xf9\x86\xe9Fl3v" \
                      b"f\r.DE\xe7\xfdw/\xed\x10\x84\xdc\xd4\x05\x0e\x93\xed\xaa\x89\x97\xd6EIhBz\xc3\x97.\x95\x85" \
                      b"\xe4\xea\x1c\xb5\xf2xB'l\x9f\xdb\x93\xd1\xf0e\x9cR\xce\x80\xd6\xed\x9a\xac\xd9\xf0\xcbGf\x05$" \
                      b"\xb1\x07\xf7\x94kI\xc3?\x99c\xdbv\xf4\xbd\x06\xe56\xe9\r?\xbe9u\xf0Z\x8c\xe0\x8c\xae\xa6\xce" \
                      b"\xf8\x946|\xbf\xd4_R\x9dg\x851\x1a\x01:\x14\x08\x1a\x01<\x14\xa9\n$\x0b\x17\x10\x1a\xb6\x11" \
                      b"\x19&\x0f)\x06\x16\x01\x18$\x12(\x01\x15\x15\x01\x12Y%\xaf\x02Z\rT\x06U)\nU\x1a\x01\x18\x15" \
                      b"\x06\x18)\x06D\x155\x0c\x17\x02'D\xd0D\x07<\xb2\x14UWE\n'\x0fe%\xb7\x15GF\x16Y6\tD\x050\x00" \
                      b"\x13+\r\t{<NM\x0e=x\n^:u)-EJ\x16(\x0f\x1fW\x81\x0c\x81\x0bw2'\x81\x01\x81\x0b#J8^\x81\x04\x00" \
                      b"5]\x81\nnaw\x81KeE\x18~\x81<\x18"
        cls.DATA = b"Curabitur porta molestie risus, sed elementum nulla fringilla vitae. Curabitur volutpat luctus" \
                   b" diam, sit amet pharetra nisl posuere eget. Vestibulum non interdum neque, quis porta elit." \
                   b" Phasellus posuere erat non quam gravida, ut volutpat neque accumsan. Sed accumsan nibh vitae" \
                   b" leo sollicitudin suscipit. Donec a sapien id sapien laoreet feugiat et ac diam. Aenean" \
                   b" tincidunt interdum nibh, ac vestibulum sem lobortis non. Aliquam lobortis mauris eu elit" \
                   b" molestie pretium. Vivamus at odio sed magna luctus tincidunt. Nam ut nisl quis dolor" \
                   b" condimentum aliquam nec placerat leo. Proin tincidunt metus eget leo laoreet, ut dignissim" \
                   b" magna scelerisque. Mauris nec fermentum erat. Donec vitae risus lobortis quam faucibus mollis."
        cls.SOURCE = io.BytesIO(b"In et elementum augue. Sed at euismod ligula. Nullam tristique nibh augue, vel "
                                b" rhoncus eros laoreet id. Sed eu enim vel purus elementum tincidunt. Vivamus vel"
                                b" pellentesque nisl. Maecenas congue feugiat lacinia. Suspendisse odio justo,"
                                b" imperdiet vitae risus ac, elementum luctus tellus. Cras sit amet sapien sit amet"
                                b" lacus pharetra sagittis eu a ante. Mauris fermentum massa congue feugiat"
                                b" fringilla. Donec non maximus felis, ac blandit orci. Pellentesque at molestie mi."
                                b" Ut tempor vestibulum enim, nec porttitor libero semper vitae. Fusce dapibus, erat"
                                b" eu euismod elementum, libero metus semper lorem, id aliquam mi mi quis orci."
                                b" Fusce id justo dictum, interdum nunc at, sagittis massa. Integer posuere enim nec"
                                b" venenatis molestie. Morbi sodales eget erat a ornare.")
        cls.TARGET = io.BytesIO(cls.ENCODED)

    def setUp(self):
        self.file = io.BytesIO()

    def tearDown(self):
        self.file.close()

    def test_can_write_data(self):
        with DeltaFile(self.file) as df:
            df.write(self.DATA)
            df.flush()
            self.assertEqual(df.size, len(self.ENCODED))

    def test_can_read_data(self):
        with DeltaFile(self.TARGET) as df:
            self.assertEqual(df.read(), self.DATA)

    def test_can_write_and_read_data(self):
        with DeltaFile(self.file) as df:
            df.write(self.DATA)
            df.flush()
            df.open('rb')
            self.assertEqual(df.read(), self.DATA)

    def test_can_assign_source(self):
        with DeltaFile(self.file) as df:
            df.source = self.SOURCE

    def test_cannot_assign_source_after_write(self):
        with DeltaFile(self.file) as df:
            df.write(self.DATA)
            with self.assertRaises(AttributeError):
                df.source = self.SOURCE

    def test_can_assign_source_after_open(self):
        with DeltaFile(self.file) as df:
            df.write(self.DATA)
            df.open('rb')
            df.source = self.SOURCE

    def test_cannot_assign_source_after_read(self):
        with DeltaFile(self.file) as df:
            df.write(self.DATA)
            df.flush()
            df.open('rb')
            df.read()
            with self.assertRaises(AttributeError):
                df.source = self.SOURCE
