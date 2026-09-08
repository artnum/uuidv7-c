/**
 * Example for uuidv7
 *
 * Create a bunch of process that generate in loop ID and store into a file
 * for each process.
 * Not perfect testing, but still kind of testing.
 */
#define UUIDV7_IMPLEMENTATION
#include "uuidv7.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PROCESS 50
#define MAX_ID 10000

int main(void) {
    int process = 0;
    pid_t pids[MAX_PROCESS];
new_fork:    
    pid_t pid = fork();
    if (pid == 0) {
        uuidv7_ctx_t ctx;
        char filename[255];
        FILE *fp = NULL;
        char struuid[50] = "##################################################";
        struuid[49] = '\0';

        snprintf(filename, 255, "./process.%d.txt", getpid());
        fp = fopen(filename, "w");
        if (!fp) {
            fprintf(stderr, "Cannot open file %s for writing\n", filename);
            return EXIT_FAILURE;
        }
        
        if (!uuidv7_open(&ctx, 10)) {
            fprintf(stderr, "Cannot open uuidv7 context\n");
            return EXIT_FAILURE;
        }

        for(int i = 0; i < MAX_ID; i++) {
            uuidv7_t id = uuidv7_get(&ctx);
            if (!uuidv7_is_valid(id)) {
                fprintf(stderr, "Error generating an id\n");
                continue;
            }
            uuidv7_str(struuid, id);
            fprintf(fp, "%s\n", struuid);
            fprintf(fp, "%016lx %016lx\n", id.uuid_high, id.uuid_low);
        }
        fclose(fp);

        uuidv7_close(&ctx);
        return EXIT_SUCCESS;
    } else if (pid > 0) {
        pids[process] = pid;
        process++;
        if (process < MAX_PROCESS) {
            goto new_fork;
        }
        for (int i = 0; i < process; i++) {
            waitpid(pids[i], NULL, 0);
        }
        printf("Should generate %d ID\n", MAX_PROCESS * MAX_ID);
    } else {
        perror("fork");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
