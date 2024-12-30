# SimdRainbowCrack

`SimdRainboxCrack` is a utility to build and query [rainbow tables](https://en.wikipedia.org/wiki/Rainbow_table) for use in unsalted hash recovery. It is based on the work by [Philippe Oechslin](https://infoscience.epfl.ch/record/99512/files/Oech03.pdf) in which he proposes a [space-time tradeoff](https://en.wikipedia.org/wiki/Space%E2%80%93time_tradeoff) technique. This implementation uses [Single Instruction Multiple Data](https://en.wikipedia.org/wiki/Single_instruction,_multiple_data) (SIMD) instructions to massively speed up the creation of the table.

# Quickstart Guide

First build a table with the chosen parameters, including hash algorithm, length, number of chains and the password space (by specifying the character set and min/max length).

```bash
simdrainbowcrack build --sha1 --length 4096 --charset ascii --min 1 --max 7 --threads 64 ~/sha1_1_7_ascii.tbl
```

By default, if no value is passed to `--count` it will generate 110% of the password space. Alternatively, if you know how many chains you want to generate, this can be specified.

The process can be interrupted at any time by pressing `ctrl-c`. Note that the program does not guarantee the proper flushing of data and this _may_ result in a corrupted table, however, this has not been observed during development and testing.

The table can then be resumed:

```bash
simdrainbowcrack resume sha1_1_7_ascii.tbl
```

The table can be queried using the `crack` operation.

```bash
$ simdrainbowcrack crack sha1_1_7_ascii.tbl aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d
SimdRainbowCrack (AVX-512)
Indexing table.. done.
aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d:hello
```

By default, SimdRainbowCrack generates compressed tables. The details will not be covered here, but essentially these are tables that only contain the endpoints. Lookup _can_ be performed on compressed tables but it is extremely slow. For any real scenario it is strongly encouraged to first decompress the tables.

```bash
$ simdrainbowcrack decompress sha1_1_7_ascii.tbl sha1_1_7_ascii.utbl
```

Then perform lookups as above on the decompressed table.

```bash
$ simdrainbowcrack crack sha1_1_7_ascii.utbl 8cbb829763a4cf999a016aa626a60de98fcc7f82
SimdRainbowCrack (AVX-512)
Indexing table.. done.
8cbb829763a4cf999a016aa626a60de98fcc7f82:HE110
```

Finally, `SimdRainbowCrack` can output information about a table file with the `info` operation.

```bash
$ simdrainbowcrack info sha1_1_7_ascii.tbl 
SimdRainbowCrack (AVX-512)
Type:        Compressed
Algorithm:   SHA1
Min:         1
Max:         7
Length:      4096
Count:       18949061700
Charset:     " !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
Charset Len: 95
KS Coverage: 110%
```

# Supported Algorithms

`SimdRainbowCrack` supports the following algorithms:

- MD5
- SHA1
- SHA256

# Support Character Sets

Any character set can be supported by passing characters to the `--charset` switch. However, passing any of the following keywords will use a predefined character set. By default the full printable ASCII character set is used.

- ascii - All printable ASCII characters
- lower - All lower case ASCII characters
- upper - All upper case ASCII characters
- numeric - All digits 0-9
- alphanumeric - All upper case, lower case and digits
- common* - 76 most common characters from real data sets
- commonshort** - 67 most common characters from real data sets

*common = `a1e20ion9r3sl85746tumdychbkgfpvjwzxqAE._SRMNILTODCBKPHG-UF!YJVWZ@QX*$#?& :+/`
**commonshort = `a1e20ion9r3sl85746tumdychbkgfpvjwzxqAE._SRMNILTODCBKPHG-UF!YJVWZ@QX`