# VFS
## Table of contents
* [What is it](#what-is-it)
* [Set up](#compile-and-execute)
* [Commands](#commands)
* [Why make it?](#what-is-it)
* [Appendix](#appendix)
<br />

# What is it?
> An emulation of Virtual File System that allows the user to create and mount Internal/Remote file systems. Disks that are mounted are stored within 'disks/' folder, written as binary in a fat32 implementation with the use of a superblock, fat table and userspace which are separated by clusters. Defined szies of these segments are defined within 'config.h' and can be manipulated to the user's preference.

> Overall, hopefully this program will allow the user to store folders/files within a binary disk and perform operations on them locally or remotely.
<br />

# Commands
## VFS
> Perform action on the virtual file system to add and mount disks or remote connections. Other actions such as initialising a server socket on a specific port.
<pre>
/vfs ls     - lists the current mounted systems                          | -> /vfs ls
/vfs ifs    - controls internal file systems within the mp_vfs              | -> /vfs ifs add/rm [DISK_NAME] [FS_TYPE]
/vfs rfs    - controls remote file systems within the mp_vfs                | -> /vfs rfs add/rm [NAME] [IP] [PORT]
/vfs mnt    - initialises the file system and mounts it towards the mp_vfs  | -> /vfs mnt [DISK_NAME]
/vfs umnt   - deletes file system data/disk from mp_vfs                     | -> /vfs umnt
/vfs server - toggles server initialisation for client connection on local host on specified port the user to access control of the virtual file system.
</pre>

# File system
> Performs action on the mounted system within the virtual file system, these commands can be performed on a filesystem which is mounted internally or remotely.
<pre>
ls     - display the entries within the current working directory.
mkdir  - create directory within current directory.
cd     - changes the current directory within the file system.
rm     - removes an entry within the file system.
touch  - creates an entry within the file system.
mv     - moves an entry towards a different location.
cp     - copies an entry within the specified directory.
cp imp - copies a file from machine's filesystem and stores within a location of the mounted disk.
cp exp - Stores a file from the disk to a location within the machines filesystem.
cat    - print bytes found at entry.
</pre>
<br />

# Compile and execute
## Compile
<pre>
> make

This will produce bin/ directory of object files located within src/

Within Makefile, ${TARGET} variable will be used for the name for the executable, this can be changed.
</pre>

## Execute
<pre>
> ./$(TARGET)
</pre>

## Clean
<pre>
> make clean

This will remove the bin/ dir holding object files related to src/*.cpp. Along with $(TARGET).exe
</pre>
<br />

# Why make it?
> The sole reason of attemting to make an emulation of a virtual file system is just for fun and practicing/learning parts of c++ that I find interesting. I find it interesting dealing a with network connections through made-up protocols to understand the same 'language' and how to interpret a buffer at the client and server side. As well as this, the overall aspect of dealing with a file that is split up into clusters and therefore allowing you to navigate through the bytes to enter a directory and print out stored entries interests me.
<br />

# Appendix
## Screenshots
> Mounting a disk
![Screenshot 2022-02-28 150301](https://user-images.githubusercontent.com/64985419/156005986-81914db9-5ea7-45a7-8029-3d892768dca2.png)
> Printing directory
![Screenshot 2022-02-28 150458](https://user-images.githubusercontent.com/64985419/156006580-3b023b2e-18da-4c09-b3dd-be7edf894536.png)
> Connecting remotely
![Screenshot 2022-02-28 150710](https://user-images.githubusercontent.com/64985419/156006585-bc5ab25c-cb67-43fa-9612-85159a64eefe.png)
