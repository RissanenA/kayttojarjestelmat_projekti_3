# Project 3: Unix Shell
## Aaro Rissanen

# Introduction
<b>wish</b> shell is a simple Unix shell with interactive and batch execution modes, configurable path variables, built-in `exit`, `cd`, and `path` commands, output redirection, and parallel command execution.

# Usage

<b>wish</b> can be run either in interactive mode where the shell prompts you to provide commands via stdin, or in batch-mode where you can provide a text file that <b>wish</b> executes as commands.

Interactive mode
```bash
./wish
```

Batch mode
```bash
./wish [batch-file]
```

When inside <b>wish</b>, you can execute commands from the default path `/bin`
```bash
wish> ls -la # executes /bin/ls
```

or you can change the search path with the built-in command `path`, specifying directories to search in
```bash
wish> path [search-dir-1] [search-dir-2] ...
```

You can exit with the built-in command `exit`, and you can change directory with the built-in command `cd`.

Command outputs can be redirected with the arrow-operator `>`. This redirects the `stdout` and `stderr` of the command process, for example
```bash
wish> ls -l > output.txt
```

would redirect the text and error output of `ls` to `output.txt`.

Multiple commands can be run simultaneously with the ampersand-operator `&`. The command processes are spawned in order from left to right, and after they have all been dispatched, they are all waited for and reaped.
```bash
wish> pwd > out1.txt & ls & echo 'Hey'
```

# Compilation

Compile with
```bash
gcc -o wish wish.c -Wall -Werror
```

and run
```bash
./wish ...
```