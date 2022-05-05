# shellington

This repository contains the source code of an extended version of the bash shell called Shellington. Extra functionalities are:
### short command
> Can be used to jump to a saved path.
```
short set <name>: Saves the current path.
short jump <name>: Jumps to the path with <name>.
```
### bookmark command
> Enables saving and executing frequently used shell commands with shortcuts.
 ```
bookmark <cmd>: Saves the <cmd>.
bookmark -i <index> Runs the command in <index>.
bookmark -l : Lists all commands.
bookmark -d <index> : Deletes the command at <index>.
 ```
### remindme command
> Displays a given reminder at a specified time on the screen.
```
remindme <time> <text>:  Reminds a certain event at a specified time in yy.mm.dd.hh.mm.ss format.
```
### cwallpaper command 
> Changes the wallpaper to a random picture.

### todo command: Stores a list of tasks
```
todo add <task>: Adds <task> to the list.
todo -l: Prints the list of tasks.
todo done <index>: Removes the task with <index> from the list.
todo clear: Deletes all items from the list.
```
### pstraverse command 
> With the help of a kernel module, traverses child processes of a task with given PID either in depth-first search or breadth-first 
```
pstraverse -d/-b <PID>: Traverses child processes starting from the process with <PID>.
```
