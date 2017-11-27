#!/bin/sh

set -e

# Clean up old test output, just in case "make check" is run twice.
rm -f test.tx_ test.new

# We need to copy the source files because the output file will be placed in
# the same directory. However, $srcdir might not be writable, while $PWD is.
if [ ! -e "test.txt" ]; then
    cp $srcdir/test.txt $srcdir/test.compressed .
    trap 'rm -f test.txt test.compressed' INT TERM EXIT
fi

$mscompress_abspath/src/mscompress test.txt
$mscompress_abspath/src/msexpand < test.tx_ > test.new
diff -q test.compressed test.tx_
diff -q test.new test.txt
