# Testing
The primary purpose of a backup is to have the data available for later.  As such, it's critical that zbackup is able to restore data.  This is a test driver aimed at running zbackup through its paces and stress testing it.
**This is not aimed at being a simple to use tool (yet)**
## Basic tests
The system includes some hardcoded tests where is creates some pseudo-ramdom files and checks that the data is deduplicated, that garbage collection reclaims the data, and that data is recovered exactly as it was stored.
## Stress Test
This is a more random test, aimed at throwing a variety of situations at zbackup to try and break it.

 - The idea is that the program is given a sample data set
 - The system will loop a specific number of times
 - Each loop, the system will:
	 - Randomly pick some files and backup each file
	 - Check all backups restore identically to the original data
	 - Randomly pick a smaller number files backed up previously and delete them
	 - Check all backups
	 - Garbage collect
	 - Check all backups
	 - Compact the backups
	 - Check all backups

#### Example Test
```
mkdir /tmp/zbackuptest/
rm -rfv /tmp/zbackuptest/* 
python3 tests/regression/regression.py \
	--zbackup /my/copy/of/zbackup \
	--tmp /tmp/zbackuptest 
	--samples /sample/data/ 
	--seed 123 
	--loops 80 
	--add-per-loop 60 
	--delete-per-loop 50 
	--log-level 3 
	--key /tmp/zkey
``` 
#### Arguments
The arguments you supply are:
- `--zbackup`: The zbackup binary to run.
- `--tmp`: Where to create the temporary zbackup store
- `--samples`: The files to randomly select and backup
- `--seed`: Used to generate the random numbers.  If you have a failure, using the same seed should reproduce it.
- `--loops`: How many times to run the loop sequence
- `--add-per-loop`: How many files to add from samples on each loop
- `--delete-per-loop`: Hoe many files to delete from the backup per loop.  These could be from a previous cycle.
- `--log-level`: 0 - 3.  1 is quite useful, but 2 or 3 might be informative to see how zbackup is being called.
- `--key`: The key for zbackup encryption.  Can be omitted, implying no encryption.

