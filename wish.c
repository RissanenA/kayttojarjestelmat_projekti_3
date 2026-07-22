#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define PRINT_ERROR( msg ) write( STDERR_FILENO, "error: " msg "\n", sizeof( "error: " msg "\n" ) )
#define WHITESPACE "\t\n\v\f\r "

// read line from 'input' stream, remove trailing newline if it exists
ssize_t fetch_line( FILE *input, char **line, size_t *bytes )
{
    ssize_t read = getline( line, bytes, input );
    if ( read != -1 )
    {
        char *line_data = *line;

        // remove newline if it exists
        if ( line_data[read - 1] == '\n' )
        {
            line_data[read - 1] = '\0';
            read -= 1;
        }
    }

    return read;
}

// split whitespace-delimited tokens by the special operators '&' and '>'
size_t split_by_special( char *line, char *commands[32][128], size_t part_count, size_t *cmd_count )
{
    char *start = line;
    char *delim = NULL;

    while (( delim = strpbrk( start, "&>" ) ))
    {
        char type = *delim;
        *delim = '\0';

        if ( start != delim )
        {
            commands[*cmd_count][part_count++] = start;
        }

        if ( type == '>' )
        {
            commands[*cmd_count][part_count++] = ">";
        }
        else
        {
            commands[*cmd_count][part_count] = NULL;
            *cmd_count += 1;
            part_count = 0;
        }

        start = delim + 1;
    }

    if ( *start != '\0' )
    {
        commands[*cmd_count][part_count++] = start;
    }

    return part_count;
}

// parses line to separate commands
void parse_line( char *line, char *commands[32][128], size_t *cmd_count )
{
    size_t part_count = 0;

    char *first_part = strtok( line, WHITESPACE );
    part_count = split_by_special( first_part, commands, part_count, cmd_count );

    for ( char *part; ( part = strtok( NULL, WHITESPACE ) ) != NULL; )
    {
        part_count = split_by_special( part, commands, part_count, cmd_count );
    }

    commands[*cmd_count][part_count] = NULL;
    *cmd_count += 1;
}

//
int get_redirect( char **args, char **filename )
{
    for ( size_t i = 0; args[i] != NULL; ++i )
    {
        if ( strcmp( args[i], ">" ) != 0 )
        {
            continue;
        }

        // redirection found, remove everything after '>' from arguments
        args[i] = NULL;

        char *file = args[i + 1];
        if ( file == NULL ) // no file specified
        {
            PRINT_ERROR( "Expected a file name after `>`" );
            return -1;
        }

        if ( args[i + 2] != NULL ) // arguments after redirect
        {
            PRINT_ERROR( "Arguments not allowed after stream redirection `> [FILE]`" );
            return -1;
        }

        // return redirect file name
        *filename = file;
        return 0;
    }

    return 0;
}

// try to find command from path
int find_cmd( char *search_path, char *cmd, char *buf )
{
    char *start = search_path;
    char *sep = NULL;

    while (( sep = strchr( start, ',' ) ))
    {
        buf[0] = '\0';
        strncat( buf, start, sep - start );
        strcat( buf, "/" );
        strcat( buf, cmd );

        if ( access( buf, X_OK ) == 0 )
        {
            return 0;
        }

        start = sep + 1;
    }

    buf[0] = '\0';
    strncat( buf, start, sep - start );
    strcat( buf, "/" );
    strcat( buf, cmd );

    if ( access( buf, X_OK ) == 0 )
    {
        return 0;
    }

    return -1;
}

// execute command if found
void execute_cmd( char *search_path, char *cmd, char **args )
{
    char path[1024];
    if ( find_cmd( search_path, cmd, path ) == -1 )
    {
        PRINT_ERROR( "Could not find command in path" );
        return;
    }

    char *redirect_name = NULL;
    if ( get_redirect( args, &redirect_name ) == -1 )
    {
        return;
    }

    // enter new process
    if ( fork() == 0 )
    {
        if ( redirect_name )
        {
            // octal 0644 mode sets created (possibly) files permissions as rw for owner,
            // read-only for others (https://www.redhat.com/en/blog/linux-file-permissions-explained)
            int redirect_fd = open( redirect_name, O_TRUNC | O_WRONLY | O_CREAT, 0644 );

            // closes stdout and stderr, and points them point to the redirect file
            dup2( redirect_fd, STDOUT_FILENO );
            dup2( redirect_fd, STDERR_FILENO );

            // close the redirect descriptor to avoid leaking it
            close( redirect_fd );
        }

        // execute command
        execv( path, args );

        PRINT_ERROR( "Command failed" );
        exit( 1 );
    }
}

char *init_search_path( void )
{
    char *default_path = "/bin";
    size_t len = strlen( default_path );

    char *search_path = malloc( len + 1 );
    memcpy( search_path, default_path, len );
    search_path[len] = '\0';

    return search_path;
}

size_t get_arg_count( char **command )
{
    size_t count = 0;
    while ( command[count] != NULL )
    {
        count++;
    }

    return count;
}

int main( int argc, char *argv[] )
{
    char *search_path = init_search_path();
    FILE *input = stdin;

    if ( argc == 2 )
    {
        char *batch_file = argv[1];
        input = fopen( batch_file, "r" );
        if ( !input )
        {
            PRINT_ERROR( "Could not open batchfile, exiting...\n" );
            exit( 1 );
        }
    }
    else if ( argc > 2 )
    {
        PRINT_ERROR( "Too many arguments, exiting...\n" );
        exit( 1 );
    }

    char *commands[32][128];
    size_t cmd_count = 0;

    char *line = NULL;
    size_t line_bytes;
    ssize_t read;

    for ( ;; )
    {
        // zero out old commands
        cmd_count = 0;

        if ( input == stdin ) // print prompt only in interactive mode
        {
            printf( "wish> " );
        }

        read = fetch_line( input, &line, &line_bytes );
        if ( read == -1 )
        {
            if ( input != stdin )
            {
                fclose( input );
            }
            free( search_path );
            free( line );
            exit( 0 );
        }
        else if ( read == 0 ) // empty command
        {
            continue;
        }

        parse_line( line, commands, &cmd_count );

        for ( size_t i = 0; i < cmd_count; ++i )
        {
            char **command = commands[i];
            char *op = command[0];
            char **args = command + 1;
            size_t arg_count = get_arg_count( args );

            if ( strcmp( op, "exit" ) == 0 ) // builtin exit
            {
                if ( arg_count == 0 )
                {
                    if ( input != stdin )
                    {
                        fclose( input );
                    }
                    free( search_path );
                    free( line );
                    exit( 0 );
                }

                PRINT_ERROR( "`exit` expects no arguments" );
            }
            else if ( strcmp( op, "cd" ) == 0 ) // builtin cd
            {
                if ( arg_count != 1 )
                {
                    PRINT_ERROR( "`cd` expects 1 argument" );
                }
                if ( chdir( args[0] ) == -1 )
                {
                    PRINT_ERROR( "Could not change directory" );
                }
            }
            else if ( strcmp( op, "path" ) == 0 ) // builtin path
            {
                size_t new_size = 0;
                search_path[0] = '\0'; // clear out the old search path

                for ( size_t i = 0; i < arg_count; ++i )
                {
                    new_size += strlen( args[i] ) + 1;                  // room for ','
                    search_path = realloc( search_path, new_size + 1 ); // room for null-terminator
                    strcat( search_path, args[i] );
                    strcat( search_path, "," );
                }
            }
            else
            {
                if ( *search_path == '\0' )
                {
                    PRINT_ERROR( "Path is empty, cannot search for commands" );
                    continue;
                }

                execute_cmd( search_path, op, command );
            }
        }

        // wait for all commands
        while ( wait( NULL ) > 0 )
        {
            continue;
        }
    }

    return 0;
}
