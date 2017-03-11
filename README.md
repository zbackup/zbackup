[![Build Status](https://travis-ci.org/zbackup/zbackup.svg)](https://travis-ci.org/zbackup/zbackup) [![Coverity Scan Build Status](https://scan.coverity.com/projects/4229/badge.svg)](https://scan.coverity.com/projects/4229)

# Introduction

**zbackup** is a globally-deduplicating backup tool, based on the ideas found in [rsync](http://rsync.samba.org/).  Feed a large `.tar` into it, and it will store duplicate regions of it only once, then compress and optionally encrypt the result. Feed another `.tar` file, and it will also re-use any data found in any previous backups. This way only new changes are stored, and as long as the files are not very different, the amount of storage required is very low. Any of the backup files stored previously can be read back in full at any time. The program is format-agnostic, so you can feed virtually any files to it (any types of archives, proprietary formats, even raw disk images -- but see [Caveats](#caveats)).

This is achieved by sliding a window with a rolling hash over the input at a byte granularity and checking whether the block in focus was ever met already. If a rolling hash matches, an additional full cryptographic hash is calculated to ensure the block is indeed the same. The deduplication happens then.

# Features

The program has the following features:

 * Parallel LZMA or LZO compression of the stored data
 * Built-in AES encryption of the stored data
 * Possibility to delete old backup data
 * Use of a 64-bit rolling hash, keeping the amount of soft collisions to zero
 * Repository consists of immutable files. No existing files are ever modified
 * Written in C++ only with only modest library dependencies
 * Safe to use in production (see [below](#safety))
 * Possibility to exchange data between repos without recompression

# Build dependencies

 * `cmake` >= 2.8.3 (though it should not be too hard to compile the sources by hand if needed)
 * `libssl-dev` for all encryption, hashing and random numbers
 * `libprotobuf-dev` and `protobuf-compiler` for data serialization
 * `liblzma-dev` for compression
 * `liblzo2-dev` for compression (optional)
 * `zlib1g-dev` for adler32 calculation

# Quickstart

To build and install:

```bash
cd zbackup
cmake .
make
sudo make install
# or just run as ./zbackup
```

`zbackup` is also part of the [Fedora/EPEL](https://apps.fedoraproject.org/packages/zbackup), [Debian](https://packages.debian.org/search?keywords=zbackup), [Ubuntu](http://packages.ubuntu.com/search?keywords=zbackup), [Arch Linux](https://aur.archlinux.org/packages/zbackup/) and [FreeBSD](http://www.freshports.org/sysutils/zbackup/).

To use:

```bash
zbackup init --non-encrypted /my/backup/repo
tar c /my/precious/data | zbackup backup /my/backup/repo/backups/backup-`date '+%Y-%m-%d'`
zbackup restore /my/backup/repo/backups/backup-`date '+%Y-%m-%d'` > /my/precious/backup-restored.tar
```

If you have a lot of RAM to spare, you can use it to speed-up the restore process -- to use 512 MB more, pass `--cache-size 512mb` when restoring.

If encryption is wanted, create a file with your password:

``` bash
# more secure to use an editor
echo mypassword > ~/.my_backup_password
chmod 600 ~/.my_backup_password
```

Then init the repo the following way:

```bash
zbackup init --password-file ~/.my_backup_password /my/backup/repo
```

And always pass the same argument afterwards:
```bash
tar c /my/precious/data | zbackup --password-file ~/.my_backup_password backup /my/backup/repo/backups/backup-`date '+%Y-%m-%d'`
zbackup --password-file ~/.my_backup_password restore /my/backup/repo/backups/backup-`date '+%Y-%m-%d'` > /my/precious/backup-restored.tar
```

If you have a 32-bit system and a lot of cores, consider lowering the number of compression threads by passing `--threads 4` or `--threads 2` if the program runs out of address space when backing up (see why [below](#caveats), item 2). There should be no problem on a 64-bit system.

# Caveats

 * While you can pipe any data into the program, the data should be uncompressed and unencrypted -- otherwise no deduplication could be performed on it. `zbackup` would compress and encrypt the data itself, so there's no need to do that yourself. So just run `tar c` and pipe it into `zbackup` directly. If backing up disk images employing encryption, pipe the unencrypted version (the one you normally mount). If you create `.zip` or `.rar` files, use no compression (`-0` or `-m0`) and no encryption.
 * Parallel LZMA compression uses a lot of RAM (several hundreds of megabytes, depending on the number of threads used), and ten times more virtual address space. The latter is only relevant on 32-bit architectures where it's limited to 2 or 3 GB. If you hit the ceiling, lower the number of threads with `--threads`.
 * Since the data is deduplicated, there's naturally no redundancy in it. A loss of a single file can lead to a loss of virtually all data. Make sure you store it on a redundant storage (RAID1, a cloud provider etc).
 * The encryption key, if used, is stored in the `info` file in the root of the repo. It is encrypted with your password. Technically thus you can change your password without re-encrypting any data, and as long as no one possesses the old `info` file and knows your old password, you would be safe (note that ability to change repo type between encrypted and non-encrypted is not implemented yet -- someone who needs this is welcome to create a pull request -- the possibility is all there). Also note that it is crucial you don't lose your `info` file, as otherwise the whole backup would be lost. 

# Limitations

 * Right now the only modes supported are reading from standard input and writing to standard output. FUSE mounts and NBD servers may be added later if someone contributes the code.
 * The program keeps all known blocks in an in-RAM hash table, which may create scalability problems for very large repos (see [below](#scalability)).
 * The only encryption mode currently implemented is `AES-128` in `CBC` mode with `PKCS#7` padding. If you believe that this is not secure enough, patches are welcome. Before you jump to conclusions however, read [this article](http://www.schneier.com/blog/archives/2009/07/another_new_aes.html).
 * It's only possible to fully restore the backup in order to get to a required file, without any option to quickly pick it out. `tar` would not allow to do it anyway, but e.g. for `zip` files it could have been possible. This is possible to implement though, e.g. by exposing the data over a FUSE filesystem.

Most of those limitations can be lifted by implementing the respective features.

# Safety

Is it safe to use `zbackup` for production data? Being free software, the program comes with no warranty of any kind. That said, it's perfectly safe for production, and here's why. When performing a backup, the program never modifies or deletes any existing files -- only new ones are created. It specifically checks for that, and the code paths involved are short and easy to inspect. Furthermore, each backup is protected by its `SHA256` sum, which is calculated before piping the data into the deduplication logic. The code path doing that is also short and easy to inspect. When a backup is being restored, its `SHA256` is calculated again and compared against the stored one. The program would fail on a mismatch. Therefore, to ensure safety it is enough to restore each backup to `/dev/null` immediately after creating it. If it restores fine, it will restore fine ever after.  
To add some statistics, the author of the program has been using an older version of `zbackup` internally for over a year. The `SHA256` check never ever failed. Again, even if it does, you would know immediately, so no work would be lost. Therefore you are welcome to try the program in production, and if you like it, stick with it.

# Usage notes

The repository has the following directory structure:

```
/repo
    backups/
    bundles/
        00/
        01/
        02/
        ...
    index/
    info
```

 * The `backups` directory contain your backups. Those are very small files which are needed for restoration. They are encrypted if encryption is enabled. The names can be arbitrary. It is possible to arrange files in subdirectories, too. Free renaming is also allowed.
 * The `bundles` directory contains the bulk of data. Each bundle internally contains multiple small chunks, compressed together and encrypted. Together all those chunks account for all deduplicated data stored.
 * The `index` directory contains the full index of all chunks in the repository, together with their bundle names. A separate index file is created for each backup session. Technically those files are redundant, all information is contained in the bundles themselves. However, having a separate `index` is nice for two reasons: 1) it's faster to read as it incurs less seeks, and 2) it allows making backups while storing bundles elsewhere. Bundles are only needed when restoring -- otherwise it's sufficient to only have `index`. One could then move all newly created bundles into another machine after each backup.
 * `info` is a very important file which contains all global repository metadata, such as chunk and bundle sizes, and an encryption key encrypted with the user password. It is paramount not to lose it, so backing it up separately somewhere might be a good idea. On the other hand, if you absolutely don't trust your remote storage provider, you might consider not storing it with the rest of the data. It would then be impossible to decrypt it at all, even if your password gets known later.

The program does not have any facilities for sending your backup over the network. You can `rsync` the repo to another computer or use any kind of cloud storage capable of storing files. Since `zbackup` never modifies any existing files, the latter is especially easy -- just tell the upload tool you use not to upload any files which already exist on the remote side (e.g. with `gsutil` it's `gsutil cp -R -n /my/backup gs:/mybackup/`).

To aid with creating backups, there's an utility called `tartool` included with `zbackup`. The idea is the following: one sprinkles empty files called `.backup` and `.no-backup` across the entire filesystem. Directories where `.backup` files are placed are marked for backing up. Similarly, directories with `.no-backup` files are marked not to be backed up. Additionally, it is possible to place `.backup-XYZ` in the same directory where `XYZ` is to mark `XYZ` for backing up, or place `.no-backup-XYZ` to mark it not to be backed up. Then `tartool` can be run with three arguments -- the root directory to start from (can be `/`), the output `includes` file, and the output `excludes` file. The tool traverses over the given directory noting the `.backup*` and `.no-backup*` files and creating include and exclude lists for the `tar` utility. The `tar` utility could then be run as  `tar c --files-from includes --exclude-from excludes` to store all chosen data.

# Scalability

This section tries do address the question on the maximum amount of data which can be held in a backup repository. What is meant here is the deduplicated data. The number of bytes in all source files ever fed into the repository doesn't matter, but the total size of the resulting repository does.
Internally all input data is split into small blocks called chunks (up to `64k` each by default). Chunks are collected into bundles (up to `2MB` each by default), and those bundles are then compressed and encrypted.

There are then two problems with the total number of chunks in the repository:

 * Hashes of all existing chunks are needed to be kept in RAM while the backup is ongoing. Since the sliding window performs checking with a single-byte granularity, lookups would otherwise be too slow. The amount of data needed to be stored is technically only 24 bytes for each chunk, where the size of the chunk is up to `64k`. In an example real-life `18GB` repo, only `18MB` are taken by in its hash index. Multiply this roughly by two to have an estimate of RAM needed to store this index as an in-RAM hash table. However, as this size is proportional to the total size of the repo, for `2TB` repo you could already require `2GB` of RAM. Most repos are much smaller though, and as long as the deduplication works properly, in many cases you can store terabytes of highly-redundant backup files in a `20GB` repo easily.
 * We use a 64-bit rolling hash, which allows to have an `O(1)` lookup cost at each byte we process. Due to [birthday paradox](https://en.wikipedia.org/wiki/Birthday_paradox), we would start having collisions when we approach `2^32` hashes. If each chunk we have is `32k` on average, we would get there when our repo grows to `128TB`. We would still be able to continue, but as the number of collisions would grow, we would have to resort to calculating the full hash of a block at each byte more and more often, which would result in a considerable slowdown.

All in all, as long as the amount of RAM permits, one can go up to several terabytes in deduplicated data, and start having some slowdown after having hundreds of terabytes, RAM-permitting.

# Design choices

  * We use a 64-bit modified Rabin-Karp rolling hash (see `rolling_hash.hh` for details), while most other programs use a 32-bit one. As noted previously, one problem with the hash size is its birthday bound, which with the 32-bit hash is met after having only `2^16` hashes. The choice of a 64-bit hash allows us to scale much better while having virtually the same calculation cost on a typical 64-bit machine.
  * `rsync` uses `MD5` as its strong hash. While `MD5` is known to be fast, it is also known to be broken, allowing a malicious user to craft colliding inputs. `zbackup` uses `SHA1` instead. The cost of `SHA1` calculations on modern machines is actually less than that of `MD5` (run `openssl speed md5 sha1` on yours), so it's a win-win situation. We only keep the first 128 bits of the `SHA1` output, and therefore together with the rolling hash we have a 192-bit hash for each chunk. It's a multiple of 8 bytes which is a nice properly on 64-bit machines, and it is long enough not to worry about possible collisions.
  * `AES-128` in `CBC` mode with `PKCS#7` padding is used for encryption. This seems to be a reasonbly safe classic solution. Each encrypted file has a random IV as its first 16 bytes.
  * We use Google's [protocol buffers](https://developers.google.com/protocol-buffers/) to represent data structures in binary form. They are very efficient and relatively simple to use.

# Compression

`zbackup` uses LZMA to compress stored data. It compresses very well, but it will slow down your backup
(unless you have a very fast CPU).

LZO is much faster, but the files will be bigger. If you don't
want your backup process to be cpu-bound, you should consider using LZO. However, there are some caveats:

* LZO is so fast that other parts of `zbackup` consume significant portions of the CPU. In fact, it is only using one core on my machine because compression is the only thing that can run in parallel.
* I've hacked the LZO support in a day. You shouldn't trust it. Please make sure that restore works before you assume that your data is safe. That may still be faster than a backup with LZMA ;-)
* LZMA is still the default, so make sure that you use the `-o bundle.compression_method=lzo` argument when you init the repo or whenever you do a backup.
* In case you want to use filesystem-level compression (think ZFS), there's `-o bundle.compression_method=zero` argument which disables compression.

You can mix LZMA and LZO in a repository. Each bundle file has a field that says how it was compressed, so
`zbackup` will use the right method to decompress it. You could use an old `zbackup` respository with only LZMA
bundles and start using LZO. However, please think twice before you do that because old versions of `zbackup`
won't be able to read those bundles.

# Improvements

There's a lot to be improved in the program. It was released with the minimum amount of functionality to be useful. It is also stable. This should hopefully stimulate people to join the development and add all those other fancy features. Here's a list of ideas:

 * Ability to change bundle type (between encrypted and non-encrypted).
 * Improved garbage collection. The program should support ability to specify maximum index file size / maximum index file count (for better compatibility with cloud storages as well) or something like retention policy.
 * A command to fsck the repo by doing something close to what garbage collection does, but also checking all hashes and so on.
 * Parallel decompression. Right now decompression is single-threaded, but it is possible to look ahead in the stream and perform prefetching.
 * Support for mounting the repo over FUSE. Random access to data would then be possible.
 * Support for exposing a backed up file over a userspace NBD server. It would then be possible to mount raw disk images without extracting them.
 * Support for other encryption types (preferably for everything `openssl` supports with its `evp`).
 * Support for other compression methods.
 * You name it!

# Communication

 * The program's website is at <http://zbackup.org/>.
 * Development happens at <https://github.com/zbackup/zbackup>.
 * Discussion forum is at <https://groups.google.com/forum/#!forum/zbackup>. Please ask for help there!

The author is reachable over email at <ikm@zbackup.org>. Please be constructive and don't ask for help using the program, though. In most cases it's best to stick to the forum, unless you have something to discuss with the author in private.

# Similar projects

`zbackup` is certainly not the first project to embrace the idea of using a rolling hash for deduplication. Here's a list of other projects the author found on the web:

 * [bup](https://github.com/bup/bup), based on storing data in `git` packs. No possibility of removing old data. This program was the initial inspiration for `zbackup`.
 * [ddar](https://github.com/basak/ddar), seems to be a little bit outdated. Contains a nice list of alternatives with comparisons.
 * [rdiff-backup](http://www.nongnu.org/rdiff-backup/), based on the original `rsync` algorithm. Does not do global deduplication, only working over the files with the same file name.
 * [duplicity](http://duplicity.nongnu.org/), which looks similar to `rdiff-backup` with regards to mode of operation.
 * Some filesystems (most notably [ZFS](http://en.wikipedia.org/wiki/ZFS) and [Btrfs](http://en.wikipedia.org/wiki/Btrfs)) provide deduplication features. They do so only at the block level though, without a sliding window, so they can not accommodate arbitrary byte insertion/deletion in the middle of data.
 * [Attic](https://attic-backup.org/), which looks very similar to `zbackup`.

# Credits

Copyright (c) 2012-2014 Konstantin Isakov (<ikm@zbackup.org>) and ZBackup contributors, see CONTRIBUTORS. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
