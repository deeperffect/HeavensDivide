"""Lossless PNG reading for numerical masks and atlas metadata; never repaints artwork."""
import struct
import zlib

def read_png(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    chunks, offset = [], 8
    while offset < len(data):
        size = struct.unpack('>I', data[offset:offset + 4])[0]
        kind, value = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        crc = struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]
        assert zlib.crc32(kind + value) & 0xffffffff == crc
        offset += size + 12
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', value)
            assert depth == 8 and color in (2, 6) and interlace == 0
            channels = 3 if color == 2 else 4
        elif kind == b'IDAT':
            chunks.append(value)
    raw, pixels = zlib.decompress(b''.join(chunks)), bytearray()
    stride, previous = width * channels, bytearray(width * channels)
    for y in range(height):
        start = y * (stride + 1)
        method, row = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up, upper_left = previous[x], previous[x - channels] if x >= channels else 0
            if method == 0:
                predictor = 0
            elif method == 1:
                predictor = left
            elif method == 2:
                predictor = up
            elif method == 3:
                predictor = (left + up) // 2
            elif method == 4:
                p = left + up - upper_left
                distances = abs(p - left), abs(p - up), abs(p - upper_left)
                predictor = (left, up, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError('Unsupported PNG row filter')
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return width, height, channels, pixels


def write_rgba(path, width, height, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    raw = b"".join(b"\x00" + pixels[y*width*4:(y+1)*width*4] for y in range(height))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width,height,8,6,0,0,0)) + chunk(b"IDAT", zlib.compress(raw,9)) + chunk(b"IEND", b""))
