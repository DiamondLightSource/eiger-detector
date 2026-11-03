import cbor2
import numpy as np


def decode_multi_dim_array(tag, column_major):
    dimensions, contents = tag.value
    if isinstance(contents, list):
        array = np.empty((len(contents),), dtype=object)
        array[:] = contents
    elif isinstance(contents, (np.ndarray, np.generic)):
        array = contents
    else:
        raise cbor2.CBORDecodeValueError("expected array or typed array")
    return array.reshape(dimensions, order="F" if column_major else "C")


def decode_typed_array(tag, dtype):
    if not isinstance(tag.value, bytes):
        raise cbor2.CBORDecodeValueError("expected byte string in typed array")
    return np.frombuffer(tag.value, dtype=dtype)


tag_decoders = {
    40: lambda tag: decode_multi_dim_array(tag, column_major=False),
    64: lambda tag: decode_typed_array(tag, dtype="u1"),
    65: lambda tag: decode_typed_array(tag, dtype=">u2"),
    66: lambda tag: decode_typed_array(tag, dtype=">u4"),
    67: lambda tag: decode_typed_array(tag, dtype=">u8"),
    68: lambda tag: decode_typed_array(tag, dtype="u1"),
    69: lambda tag: decode_typed_array(tag, dtype="<u2"),
    70: lambda tag: decode_typed_array(tag, dtype="<u4"),
    71: lambda tag: decode_typed_array(tag, dtype="<u8"),
    72: lambda tag: decode_typed_array(tag, dtype="i1"),
    73: lambda tag: decode_typed_array(tag, dtype=">i2"),
    74: lambda tag: decode_typed_array(tag, dtype=">i4"),
    75: lambda tag: decode_typed_array(tag, dtype=">i8"),
    77: lambda tag: decode_typed_array(tag, dtype="<i2"),
    78: lambda tag: decode_typed_array(tag, dtype="<i4"),
    79: lambda tag: decode_typed_array(tag, dtype="<i8"),
    80: lambda tag: decode_typed_array(tag, dtype=">f2"),
    81: lambda tag: decode_typed_array(tag, dtype=">f4"),
    82: lambda tag: decode_typed_array(tag, dtype=">f8"),
    83: lambda tag: decode_typed_array(tag, dtype=">f16"),
    84: lambda tag: decode_typed_array(tag, dtype="<f2"),
    85: lambda tag: decode_typed_array(tag, dtype="<f4"),
    86: lambda tag: decode_typed_array(tag, dtype="<f8"),
    87: lambda tag: decode_typed_array(tag, dtype="<f16"),
    1040: lambda tag: decode_multi_dim_array(tag, column_major=True),
}


def stream2_tag_decoder(_decoder, tag):
    tag_decoder = tag_decoders.get(tag.tag)
    return tag_decoder(tag) if tag_decoder else tag
