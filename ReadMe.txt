Problems I get:
the FUSE callback 'read' is supposed to return *exactly* the requested
size. If it returns less than the size, the remaining is padded with 0
and as far as I can see it returns the full data...

To get a "normal" read behavior (i.e. the returned value from read is
passed as return value from the syscall), I have to use mount option
"direct_io".
It works fine, but it seems from mailing list that it prevent execve stuff
to work fine, so this is not suitable for a "real" filesystem which allows
to run program from it.

For my particular use this is enougth.


Well...
At least 'xine' does not support well to read video from 'direct_io'
filesystems... 'mplayer' works fine.

Will try to make code modifications to handle 'read' stuff has FUSE wants it.


Note : updates on the 'metadata' file are checked only when a 'close' is
performed on a file.
So you will not see changes until you open (and then close) a file.
This should be improved.
The current problem is that I need to destroy existing caches when re-building
FS tree (because of existing pointers to FS tree), and this can more easely
be done at 'close'.


Note: now 'read' should behave the expected way, so removing 'direct_io'
should work.


Note: on my computer, with a local HTTP server (so "no" network involved)
dd or md5sum are able to read file at >50Mb/s (on a video of ~700Mb/s).

Faster was ~75Mb/s (dd with large blocksize).
In direct, the original file was treated ~3 time faster.

