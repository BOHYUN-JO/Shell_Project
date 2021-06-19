# Shell_Project

How to compile 
```
> make
```

How to run 
```
> ./myshell
```

# About Program

## This program is simple shell.

### user can use commands such as

cd, cd .. : to navigaite the directories in your shell

ls : listing the directory contents

mkdir, rmdir : create and remove directory using your shell

touch, cat, echo : creating, reading and printing the contents of a file

exit : shell quits and returns to original program(parent process)

### user can pipe commands such as

ls -al | grep filename

cat filename | less

cat filename | grep -v "abc" | sort -r

..etc

### user can use background commands such as

jobs : list the running and stopped background jobs

bg %(job_id) : change a stopped background job to a running background job.

kill %(job_id) : terminate a job.

ls -al | grep filename &

cat sample | grep -v a &

..etc
