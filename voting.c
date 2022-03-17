#include "voting.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>

#define CHILDS "pids"

int exit_signaled = 0;

void padre_int_handler(int sig) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(CHILDS, "r");
    if(fp == NULL) exit(EXIT_FAILURE);

    //enviar señal interrupt a hijoss
    while ((read = getline(&line, &len, fp)) != -1) {
        //enviar señal
        kill(atoi(line), SIGINT);
    }

    fclose(fp);
    if(line) free(line);

    exit_signaled = 1;
    return;
}

int main(int argc, char *argv[]){
    int i, n_procs, n_secs, childpid;
    int *buffer = NULL;
    FILE *pids = NULL;
    sem_t * sem1 = NULL;

    struct sigaction act;

    if (argc != 3){
        printf("Please input 2 arguments: number of process, maximum number of seconds.\n");
        exit(EXIT_FAILURE);
    }

    n_procs = atoi(argv[1]);
    n_secs = atoi(argv[2]);

    //Alarma

    pids = fopen(CHILDS, "w");
    if (!pids){
        perror("fopen");
        exit (EXIT_FAILURE);
    }
    buffer = (int*)malloc(sizeof(int) * n_procs);

    //Preparar handler interrupt
    sigemptyset(&(act.sa_mask));    
    act.sa_flags = 0;

    act.sa_handler = padre_int_handler;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Initialize semaphor
    if((sem1 = sem_open("/sem1", O_CREAT | O_EXCL , S_IRUSR | S_IWUSR, 0)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    //Crear procesos
    for (i = 0; i < n_procs; i++){
        childpid = fork();
        if (childpid < 0){
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if(childpid == 0){
            // hijo ----
            votante(sem1);
        }else{
            // padre ---
            fprintf(pids, "%d\n", childpid);
            buffer[i] = childpid;
        }
    }



    // Esperar a q los hijos estan readys
    for (i = 0; i < n_procs; i++){
        sem_wait(sem1);
    }

    //Sistema listo -> enviar senales SIGUSR1
    for (i = 0; i < n_procs; i++){
        if(kill(buffer[i], SIGUSR1) == -1){
            perror("kill");
            exit(EXIT_FAILURE);
        }
    }

    //Esperar a finalizacion
    int exit1 = 1;
    for (i = 0; i < n_procs; i++){
        if(waitpid(buffer[i], &exit1, 0) == 0)
            exit(EXIT_FAILURE);
    }

    //Terminar
    free(buffer);
    if(exit_signaled == 1){
        printf("Finishing by signal\n");
    }

    exit(EXIT_SUCCESS);
}









int usr_interrupt = 0;
int candidate = 1;

void int_handler(int sig) {
    //Liberar espacio
    printf("(interrupt) liberandooooo....\n");
    exit(EXIT_SUCCESS);
}

void usr1_handler(int sig) {
    usr_interrupt = 1;
}

void usr2_handler(int sig) {
    candidate = 0;
}

//Codigo que van a ejecutar los hijos
int votante(sem_t *sem1){
    struct sigaction act_int, act_usr1, act_usr2;
    sigset_t set, oset;

    sigemptyset(&(act_int.sa_mask));
    act_int.sa_flags = 0;

    printf("Una hijita\n");

    /* --------------------------- Wait till ready -------------------------- */

    // Set signal interrupt handler
    act_int.sa_handler = int_handler;
    if (sigaction(SIGINT, &act_int, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Set user 1 handler
    act_usr1.sa_handler = usr1_handler;
    if (sigaction(SIGUSR1, &act_usr1, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Block ready signal
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if(sigprocmask(SIG_BLOCK, &set, &oset) < 0){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    sem_post(sem1); // permitirle continuar al padre, el hijo esta listo a recibir el ready

    // Wait for user signal from system (ready)
    while(!usr_interrupt){
        sigsuspend(&act_int.sa_mask);
    }

    // Return to normal
    if(sigprocmask(SIG_UNBLOCK, &set, &oset) < 0){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* --------------------------- Solicitar el candidato -------------------------- */
    printf("Vamo a solicitar\n");

    // semaphore = 0

    sem_wait(sem1);


    // enviar señales




    // Set user 2 handler
    /*
    act_usr2.sa_handler = usr1_handler;
    if (sigaction(SIGUSR2, &act_usr2, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    */




    // escuchando la señal sigusr2
    //      sigusr2 -> (handler) candidate = 0

    // (semaphore = 1)

    // sem_wait (semphore = 0)

    // if(candidate = 1)
    //      for ->  kill(1, sigusr2)
    //              kill(2, sigusr2)
    //              kill(3, sigusr2)













    exit(EXIT_SUCCESS);
}





/*











*/