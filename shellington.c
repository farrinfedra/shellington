#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
const char * sysname = "shellington";
char namesFilePath[255];
char pathsFilePath[255];
char todoFilePath[255];

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
	int i=0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background?"yes":"no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
	printf("\tRedirects:\n");
	for (i=0;i<3;i++)
		printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i=0;i<command->arg_count;++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}


}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i=0; i<command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i=0;i<3;++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next=NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters=" \t"; // split at whitespace
	int index, len;
	len=strlen(buf);
	while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len>0 && strchr(splitters, buf[len-1])!=NULL)
		buf[--len]=0; // trim right whitespace

	if (len>0 && buf[len-1]=='?') // auto-complete
		command->auto_complete=true;
	if (len>0 && buf[len-1]=='&') // background
		command->background=true;

	char *pch = strtok(buf, splitters);
	command->name=(char *)malloc(strlen(pch)+1);
	if (pch==NULL)
		command->name[0]=0;
	else
		strcpy(command->name, pch);

	command->args=(char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index=0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch) break;
		arg=temp_buf;
		strcpy(arg, pch);
		len=strlen(arg);

		if (len==0) continue; // empty arg, go for next
		while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
		if (len==0) continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|")==0)
		{
			struct command_t *c=malloc(sizeof(struct command_t));
			int l=strlen(pch);
			pch[l]=splitters[0]; // restore strtok termination
			index=1;
			while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

			parse_command(pch+index, c);
			pch[l]=0; // put back strtok termination
			command->next=c;
			continue;
		}

		// background process
		if (strcmp(arg, "&")==0)
			continue; // handled before

		// handle input redirection
		redirect_index=-1;
		if (arg[0]=='<')
			redirect_index=0;
		if (arg[0]=='>')
		{
			if (len>1 && arg[1]=='>')
			{
				redirect_index=2;
				arg++;
				len--;
			}
			else redirect_index=1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index]=malloc(len);
			strcpy(command->redirects[redirect_index], arg+1);
			continue;
		}

		// normal arguments
		if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
			|| (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
		{
			arg[--len]=0;
			arg++;
		}
		command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
		command->args[arg_index]=(char *)malloc(len+1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count=arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index=0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);


    //FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state=0;
	buf[0]=0;
  	while (1)
  	{
		c=getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c==9) // handle tab
		{
			buf[index++]='?'; // autocomplete
			break;
		}

		if (c==127) // handle backspace
		{
			if (index>0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c==27 && multicode_state==0) // handle multi-code keys
		{
			multicode_state=1;
			continue;
		}
		if (c==91 && multicode_state==1)
		{
			multicode_state=2;
			continue;
		}
		if (c==65 && multicode_state==2) // up arrow
		{
			int i;
			while (index>0)
			{
				prompt_backspace();
				index--;
			}
			for (i=0;oldbuf[i];++i)
			{
				putchar(oldbuf[i]);
				buf[i]=oldbuf[i];
			}
			index=i;
			continue;
		}
		else
			multicode_state=0;

		putchar(c); // echo the character
		buf[index++]=c;
		if (index>=sizeof(buf)-1) break;
		if (c=='\n') // enter key
			break;
		if (c==4) // Ctrl+D
			return EXIT;
  	}
  	if (index>0 && buf[index-1]=='\n') // trim newline from the end
  		index--;
  	buf[index++]=0; // null terminate string

  	strcpy(oldbuf, buf);

  	parse_command(buf, command);

  	// print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  	return SUCCESS;
}
int process_command(struct command_t *command);


int main()
{
	getcwd(namesFilePath, sizeof(namesFilePath));
	strcat(namesFilePath, "/names.txt");
	getcwd(pathsFilePath, sizeof(pathsFilePath));
	strcat(pathsFilePath, "/paths.txt");
	getcwd(todoFilePath, sizeof(todoFilePath));
	strcat(todoFilePath, "/todos.txt");

	while (1)
	{
		struct command_t *command=malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code==EXIT) break;

		code = process_command(command);
		if (code==EXIT) break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

void short_set_command(char *name){
	FILE *namesFile = fopen(namesFilePath, "a+");
	FILE *pathsFile = fopen(pathsFilePath, "a+");
	char buffer[100];
	//Write name to the file.
	strcpy(buffer, name);
	strcat(buffer, "\n");
	fputs(buffer, namesFile);
	//Write current working path to the file.
	memset(buffer,0,sizeof(buffer));
	getcwd(buffer, sizeof(buffer));
	strcat(buffer, "\n");
	fputs(buffer, pathsFile);
	fclose(namesFile);
	fclose(pathsFile);
}

void short_jump_command(char *name){
	FILE *namesFile = fopen(namesFilePath, "r");
	FILE *pathsFile = fopen(pathsFilePath, "r");
	char targetPath[1024];
	char tempPath[1024];
	int flag = 0;
	if (namesFile == NULL || pathsFile == NULL){
		printf("Could not find any set path history");
		return;
	}
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), namesFile) != NULL){
		fgets(tempPath, sizeof(tempPath), pathsFile);
		buffer[strcspn(buffer, "\n")] = 0;
		if (strcmp(buffer, name) == 0){
			tempPath[strcspn(tempPath, "\n")] = 0;
			strcpy(targetPath, tempPath);
			flag = 1;
		}
	}
	
	if (flag == 1) {
		int r = chdir(targetPath);
		if (r == -1){
			printf("Could not jump to path.\n");
		}
	}		
}

void remindMe(struct command_t *command){
	char *timeString = command->args[0];
	char *hour = strtok(timeString, "."); //extract hour
	char *min = strtok(NULL, "."); //extract minute
	char pathToNotify[30] = "/usr/bin/notify-send";

	char message2[500] = {0};
	for(int i = 1; i < command->arg_count ; i++){
		strcat(message2, command->args[i]);
		strcat(message2, " ");
	}

	char buffer[1000] = {0};
	sprintf(buffer, "crontab -l | { cat; echo '%s %s * * * XDG_RUNTIME_DIR=/run/user/$(id -u) /usr/bin/notify-send %s'; } | crontab -",
	min, hour, message2);
	char *arr[] = {"sh","-c", buffer, NULL};
	execv("/usr/bin/sh", arr);
}

void cWallPaper(struct command_t *command){

	char savePath[] = "/tmp/random_background.jpg";
	char url[] = "https://unsplash.it/1920/1080/?random";
	char *args3[] = {"wget", "-O", savePath, url ,NULL};
	char *args4[] = {"gsettings", "set", "org.gnome.desktop.background", "picture-uri", savePath};

	execv("/usr/bin/wget", args3);
	printf("\n\nafter wget\n");
	execv("/usr/bin/gsettings", args4);
}

void todo_add_command(char *task){
	FILE *todoFile = fopen(todoFilePath, "a+");

	if (todoFile == NULL) {
		printf("Unable to open todos.txt file. Exiting...\n");
		exit(EXIT);
	}

	char buffer[255];
	strcpy(buffer, task);
	strcat(buffer, "\n");
	fputs(buffer, todoFile);
	fclose(todoFile);
}

void todo_done_command(int targetIndex){
	FILE *todoFile = fopen(todoFilePath, "a+");
	FILE *tempFile = fopen("temp.txt", "w");
	int lineCounter = 0;

	if (todoFile == NULL || tempFile == NULL){
		printf("Unable to open file. Exiting...\n");
		exit(EXIT);
	}
	
	char buffer[1024];
  	memset(buffer, 0, sizeof buffer);
	while (fgets(buffer, sizeof(buffer), todoFile) != NULL){
		lineCounter++;	
		if (targetIndex == lineCounter + 1){
			strcat(buffer, "***DONE*** -> ");
			fputs(buffer, tempFile);
		} else {
			fputs(buffer, tempFile);
		}
	}

	fclose(todoFile);
	fclose(tempFile);
	remove(todoFilePath);
	rename("temp.txt", todoFilePath);
}

void todo_list_command(){
	FILE *todoFile = fopen(todoFilePath, "a+");

	if (todoFile == NULL){
		printf("Unable to open todos.txt file. Exiting...\n");
		exit(EXIT);
	}
	
	int lineCount = 1;
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), todoFile) != NULL){	
		printf("%d) %s", lineCount, buffer);
		lineCount++;			
	}

	fclose(todoFile);
}

void todo_clear_command(){
	FILE *todoFile = fopen(todoFilePath, "w");
	fclose(todoFile);
}


int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "")==0) return SUCCESS;

	if (strcmp(command->name, "exit")==0)
		return EXIT;

	if (strcmp(command->name, "cd")==0)
	{
		if (command->arg_count > 0)
		{
			r=chdir(command->args[0]);
			if (r==-1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	if (strcmp(command->name, "short") == 0){
		if (command->arg_count == 2) {
			if (strcmp(command->args[0], "set") == 0){
				short_set_command(command->args[1]);
				return SUCCESS;
			} else if (strcmp(command->args[0], "jump") == 0){
				short_jump_command(command->args[1]);
				return SUCCESS;
			}
		}
	}

	if (strcmp(command->name, "todo") == 0){
		if (command->arg_count == 2){
			if (strcmp(command->args[0], "add") == 0){
				todo_add_command(command->args[1]);
				return SUCCESS;
			} else if (strcmp(command->args[0], "done") == 0){
				todo_done_command(atoi(command->args[1]));
				return SUCCESS;
			}
		} else if (command->arg_count == 1) {
			if (strcmp(command->args[0], "clear") == 0){
				todo_clear_command();
				return SUCCESS;	
			} else if (strcmp(command->args[0], "-l") == 0){
				todo_list_command();
				return SUCCESS;
			}
		}
	}
	

	pid_t pid=fork();
	if (pid==0) // child
	{
		/// This shows how to do exec with environ (but is not available on MacOs)
	    // extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		if(strcmp(command->name, "remindme") == 0){
			remindMe(command);
			exit(0);
		}else if(strcmp(command->name, "cwallpaper") == 0){
			cWallPaper(command);
			exit(0);
		}

		// increase args size by 2
		command->args=(char **)realloc(
			command->args, sizeof(char *)*(command->arg_count+=2));

		// shift everything forward by 1
		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];

		// set args[0] as a copy of name
		command->args[0]=strdup(command->name);
		// set args[arg_count-1] (last) to NULL

		command->args[command->arg_count-1]=NULL;	

		// execvp(command->name, command->args); // exec+args+path
		char path[sizeof("/usr/bin/")+ sizeof(command->name)] = "/usr/bin/";
		strcat(path, command->name);
		execv(path, command->args);


		exit(0);
		/// TODO: do your own exec with path resolving using execv()

	}
	else
	{
		if (!command->background)
			wait(0); // wait for child process to finish
		return SUCCESS;
	}

	// TODO: your implementation here
	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}
