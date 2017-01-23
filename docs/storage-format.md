# ZBackup storage format

This document describes the low level details of the ZBackup storage format.

The repository consists of a single `info` file, an `info_extended` file in
newer versions, plus a number of `backup`, `index` and `bundle` files. An
encrypted repository also requires a password file, which is managed directly by
the user. Files are written to a `tmp` directory and then moved into place when
writing to the repository.

```
repository
+-- backups
|   +-- arbitrary-backup-name
|   +-- ... 
+-- bundles
|   +-- 00
|   |   +-- 00xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
|   |   +-- ...
|   +-- ...
+-- index
|   +-- xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
|   +-- ...
+-- info
+-- info_extended
+-- tmp
    +-- temporary-file-name
    +-- ...

```

## General concepts

### Protobuf

Most data is stored using Google's protobuf messages, which are defined in
`zbackup.proto`. These are written with their own length at the start so that
they can be read correctly from a stream with multiple data items. For more
information about the low level format, please see the protobuf documentation
here:

https://developers.google.com/protocol-buffers/

https://developers.google.com/protocol-buffers/docs/encoding

### Encryption

Encryption is performed on backup, index and bundle files if enabled. This uses
AES with 128 bit (16 byte) blocks in CBC mode with PKCS7 padding. There is a
random IV written to the start of each file. The encryption key is stored using
PBKDF2 using SHA-1 HMAC and the user supplied password, in the info file.

The IV implementation actually writes out files starting with a completely zero
IV, then writes out 16 random bytes to the file. When reading, a zero IV is also
used and then the first 16 byte block is read over and ignored. This does not
affect the encryption in any way since CBC uses the previous block as the IV
state for the next, but it does affect the adler32 checksum, which includes the
random data in its "decrypted" form, whereas the actual IV is this random data
"encrypted" with the zero IV.

https://en.wikipedia.org/wiki/Advanced_Encryption_Standard

https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#CBC

https://en.wikipedia.org/wiki/Padding_(cryptography)#PKCS7

https://en.wikipedia.org/wiki/PBKDF2

### Checksum

Files are terminated with an adler32 checksum of all the preceding bytes, which
can be used to verify the integrity of the files contents. Bundle files also
contain a checksum after the bundle info at the start, but before the compressed
chunk data. More information about adler32 is available here:

https://en.wikipedia.org/wiki/Adler-32

The adler32 checksum is applied to the decrypted data in the file, and it is
itself encrypted. It includes the IV in its "decrypted" form, as described in
the previous section.

### Compression

Compression is provided by standard LZMA or LZO. This is only used in bundle
files, since the other files consist of data which would not benefit from
compression. The compression method used is specified in the file's header.

https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm

https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Oberhumer

### Versioning

All files contain a version number in the initial header. This is currently
always set to `1`, and a reader should fail with an error if it encounters any
other version number.

## Info and info_extended files

The `info` file contains basic information about the repository, in the form of
a single `StorageInfo` message. This is entirely deprecated, with the exception
of an optional embedded `EncryptionKeyInfo` message. If this is present, the
repository's other data files are encrypted, using the encryption key stored
here.

| Section      | Description                              |
| ------------ | ---------------------------------------- |
| File header  | A single `FileHeader` message            |
| Storage info | A single `StorageInfo` message.          |
| Checksum     | Adler32 checksum of all preceding bytes. |

The `info_extended` file contains various tunable parameters relating to the
amount of data stored in chunks and bundles and the default compression method
and compression level. For more information, see the comments in
`zbackup.proto`.

| Section               | Description                              |
| --------------------- | ---------------------------------------- |
| File header           | A single `FileHeader` message            |
| Extended storage info | A single `ExtendedStorageInfo` message.  |
| Checksum              | Adler32 checksum of all preceding bytes. |

## Backup files

These are stored in the `backups` subdirectory of the repository, and can be
arbitrarily named, and nested in subdirectories, by the user. They can also be
freely renamed or even deleted.

They consist of a single `BackupInfo` message, followed by the backup's raw
data. The backup info contains the size of the following backup data as encoded,
the number of iterations required to expand it, and the size and SHA-256
checksum of the expanded data.

The backup data itself consists of a series of `BackupInstruction` messages, the
interpretation of these is described below.

The iterations are used to increase the compression/deduplication of the backed
up data, by applying the deduplication process recursively, with the number of
iterations stored in this field. This ensures that backup files are always very
small, and so don't need to be compressed.

The iterations also provide compression of the deduplicated data, by allowing
the same deduplication algorithm applied to the raw data to be applied to the
series of backup instructions. As such, if two files contain a long stretch of
identical data which generates identical backup instructions, that long stretch
can then be referenced by a single backup instruction.

| Section     | Description                              |
| ----------- | ---------------------------------------- |
| IV          | Optional 16-byte IV                      |
| File header | A single `FileHeader` message            |
| Backup info | A single `BackupInfo` message            |
| Backup data | A series of `BackupInstruction` messages |
| Checksum    | Adler32 checksum of all preceding bytes  |

## Bundle files

These are stored in the `bundles` subdirectory of the repository, named by 24
random bytes (the bundle ID), encoded as 48 hexadecimal digits. These are split
into 256 subdirectories based on the first byte (or first two characters). 

A bundle contains a single `BundleInfo` message with a list of chunks stored,
including their ID and size. Bundle files contain a `BundleFileHeader` instead
of the standard `FileHeader`, which includes the compression method used for the
bundle file.

The `BundleInfo` is followed by an adler32 checksum, which allows this data to
be read and verified without reading the (potentially) large amount of data
which follows it. This information is also stored in the index files.

This checksum is then followed by an optionally compressed stream of chunk data.
Offsets of specific chunks in this data can be obtained by summing the size of
the previous chunks, as described in the bundle info.

The final checksum covers the whole file, including the intermediate checksum.

| Section            | Description                                |
| ------------------ | ------------------------------------------ |
| IV                 | Optional 16-byte IV                        |
| Bundle file header | A single `BundleFileHeader` message        |
| Bundle info        | A single `BundleInfo` message              |
| Checksum           | Adler32 checksum of all preceding bytes    |
| Chunk data         | Optionally compressed stream of chunk data |
| Checksum           | Adler32 checksum of all preceding bytes    |

## Index files

Index files contain copies of the `BundleInfo` messages from every bundle. This
is used to quickly access this data, which must be accessed both during encoding
and decoding of backups.

It consists of a series of `IndexBundleHeader` messsages, which identifies a
bundle, then the same `BundleInfo` message stored at the start of the bundle,
which identifies the chunks stored in the bundle. It is terminated by a final
`IndexBundleHeader` with no bundle ID.

Indexes are named in a similar fashion to bundles, with a 24-byte random ID,
encoded as 48 hexadecimal characters. The index name has no semantic meaning.

The index files do not contain any critical data, and can be entirely
regenerated from the bundles themselves, or this information can be read
directly from the bundles, although this is likely to be far less performant.

| Section             | Description                              |
| ------------------- | ---------------------------------------- |
| IV                  | Optional 16-byte IV                      |
| File header         | A single `FileHeader` message            |
| Index bundle header | Single `IndexBundleHeader` message       |
| Bundle info         | Single `BundleInfo` message              |
| ...                 | Repeat the last two elements many times  |
| Index bundle header | Single empty `IndexBundleHeader` message |
| Checksum            | Adler32 checksum of all preceding bytes  |

## Backup data stream

Backup files, and their expanded data during intermediate iterations, contain a
series of `BackupInstruction` messages. These may reference a chunk, which
expands to the chunk's data, or a series of bytes to be included directly. It
may contain both, in which case they are both included in this order.

Chunks may be located using the index files, or by a brute-force search through
all bundles.

If random access is required to a backup, then it should be expanded to the
penultimate iteration, which should only require decompression of a relatively
small number of bundles. At this stage, you should have a stream of backup
instructions, each with a size which can be summed to provide an offset into the
uncompressed data.
