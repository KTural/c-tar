# c-tar

### Content

A GNU tar compatible tape archiver in C with functionalities -f, -t, -x, -v

### Usage

Compiling:

```sh
$ gcc -Wall -Wextra -o mytar mytar.c
```

Info:

```sh
$ ./mytar --usage
mytar: Usage: ./mytar [-f ARCHIVE] [-f ARCHIVE -t] [-t -v -f ARCHIVE]
	[-x -f ARCHIVE] [-v -x -f ARCHIVE] [--help] [--usage]...
$ ./mytar --help
mytar: Usage: ./mytar [OPTION...] [FILE]...
'mytar' saves many files together into a single tape or disk archive, and can
restore individual files from the archive.

Examples:
  ./mytar -f archive.tar -t # List all files in archive.tar.
  ./mytar -x -f archive.tar # Extract all files from archive.tar.
  ./mytar -v -x -f archive.tar # Extract all files from archive.tar verbosely.

 Main operation mode:

  -t		list the contents of an archive
  -x		extract files from an archive

 Device selection and switching:

  -f		use archive file or device ARCHIVE

 Informative output:

  -v		verbosely list files processed

 Other options:

  --help		give this help list
  --usage		give a short usage message
```
Extra:

- The tar implementation must be able to list GNU tar archives created via the following invocation:

```sh
$ tar -f archive.tar -c <files>
```

- Invalid invocation prints some error message and fails:

```sh
$ ./mytar
mytar: need at least one option
$ echo $?
2

$ ./mytar -f archive.tar -X
mytar: Unknown option: -X
$ echo $?
2
```

- Listing:

```sh
$ touch file1 file2 file3 file4
$ tar -f archive.tar -c file*
$ ./mytar -f archive.tar -t
file1
file2
file3
file4
$ ./mytar -f archive.tar -t file4 file1
```

- Files not present in the archive are reported to stderr at the end:

```sh
$ ./mytar -f archive.tar -t file6 file4 file1 file5
file1
file4
mytar: file6: Not found in archive
mytar: file5: Not found in archive
mytar: Exiting with failure status due to previous errors
$ echo $?
2
```

- Listing truncated archive (that is, there is not enough data for the listed
  archived file size in the archive) reports an error:
  
```sh
$ ./mytar -f partial.tar -t
aaa-file
mytar: Unexpected EOF in archive
mytar: Error is not recoverable: exiting now
```

- A tar archive finishes with two zero blocks.  Missing both is silently
  accepted but missing just one emits a warning (but the command still returns
  0):
  
```sh
$ ./mytar -f one-zero-block-missing.tar -t
aaa-file
mytar: A lone zero block at 4
$ echo $?
0
```
 
- Recognize and report if a file is not a tar file (use the 'magic' field for
  that):
  
```sh
$ ./mytar -x -f mytar.c
mytar: This does not look like a tar archive
mytar: Exiting with failure status due to previous errors
$ echo $?
2
```

- Extract works silently as usual:

```sh
$ ./mytar -x -f archive.tar
$ echo $?
0
```
- Extract works verbosely with -v:

```sh
$ ./mytar -v -x -f archive.tar
file1
file2
file3
file4
$ echo $?
0
```

- Working with truncated archives results in the similar behavior as when
  listing them but the extract will do as much as possible:
  
```sh
$ ./mytar -x -f partial.tar
mytar: Unexpected EOF in archive
mytar: Error is not recoverable: exiting now
```

- Selective extraction of files works similarly to the listing:

```sh
$ ./mytar -v -x -f archive.tar file2 file4
file2
file4
$ echo $?
0
```

- Listing a truncated archive still lists each file with -v (file2 is
  truncated in our example, file1 is not):
  
```sh
$ ./mytar -v -x -f partial.tar
file1
file2
mytar: Unexpected EOF in archive
mytar: Error is not recoverable: exiting now
```

  - Also note that the file2 is created and all the data from the archive
    pertaining to file2 is still written there.  That is, the tar does as
    much as it is possible before bailing out.


### Tests

Refer to the tests directory for detailed information
