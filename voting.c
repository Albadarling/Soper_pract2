#include "voting.h"

#define CHILDS "system"

int exit_signaled = 0;

int* read_pids(int n_procs){
    int * buffer = NULL;
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int i = 0;

    buffer = (int*)malloc(sizeof(int) * n_procs);

    fp = fopen(CHILDS, "r");
    if(fp == NULL) exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        buffer[i] = atoi(line);
        i++;
    }

    fclose(fp);
    if(line) free(line);

    return buffer;
}

void padre_int_handler(int sig) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(CHILDS, "r");
    if(fp == NULL) exit(EXIT_FAILURE);

    //enviar senal interrupt a hijoss
    while ((read = getline(&line, &len, fp)) != -1) {
        //enviar senal
        kill(atoi(line), SIGINT);
    }

    fclose(fp);
    if(line) free(line);

    exit_signaled = 1;
    return;
}

int main(int argc, char *argv[]){
    int i, n_procs, n_secs, childpid, ret;
    int *buffer = NULL;
    FILE *pids = NULL, *elecciones = NULL;
    sem_t * sem1 = NULL, *sem2 = NULL;

    struct sigaction act;

    if (argc != 3){
        printf("Please input 2 arguments: number of process, maximum number of seconds.\n");
        exit(EXIT_FAILURE);
    }

    n_procs = atoi(argv[1]);
    n_secs = atoi(argv[2]);

    buffer = (int*)malloc(sizeof(int) * n_procs);

    
    /* --------------------------- Initialize system -------------------------- */
    
    //Alarma
    //--------

    // Create system file
    pids = fopen(CHILDS, "w");
    if (!pids){
        perror("fopen");
        exit (EXIT_FAILURE);
    }

    // Create elections file
    elecciones = fopen("elecciones", "w");
    if (!elecciones){
        perror("fopen");
        exit (EXIT_FAILURE);
    }


    // Preparar handler interrupt
    sigemptyset(&(act.sa_mask));    
    act.sa_flags = 0;

    act.sa_handler = padre_int_handler;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Initialize semaphors
    sem_unlink("/sem1");
    sem_unlink("/sem2");
    if((sem1 = sem_open("/sem1", O_CREAT | O_EXCL , S_IRUSR | S_IWUSR, 0)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if((sem2 = sem_open("/sem2", O_CREAT | O_EXCL , S_IRUSR | S_IWUSR, 0)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Crear procesos
    for (i = 0; i < n_procs; i++){
        childpid = fork();
        if (childpid < 0){
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if(childpid == 0){
            // hijo ----
            votante(sem1, sem2, n_procs, elecciones);
        }else{
            // padre ---
            fprintf(pids, "%d\n", childpid);
            buffer[i] = childpid;
        }
    }

    fflush(pids); // output pids to system file
    fflush(elecciones);

    /* ---------------------------- Start childs --------------------------- */

    // Esperar a q los hijos estan readys
    for (i = 0; i < n_procs; i++){
        ret = 4;
        while(ret == 4){
            if(sem_wait(sem1) < 0){
                ret = errno;
            }else{
                ret = 0;
            }
        }
    }

    // Sistema listo -> enviar senales SIGUSR1
    for (i = 0; i < n_procs; i++){
        if(kill(buffer[i], SIGUSR1) == -1){
            perror("kill");
            exit(EXIT_FAILURE);
        }
    }

    sem_post(sem1); // Deja pasar al candidato (proceso que sea mas rapido)
    
    /* --------------------------- Finalize system -------------------------- */

    // Esperar a finalizacion
    int exit1 = 1;
    for (i = 0; i < n_procs; i++){
        if(waitpid(buffer[i], &exit1, 0) == 0)
            exit(EXIT_FAILURE);
    }

    // Terminar
    free(buffer);
    fclose(elecciones);
    if(exit_signaled == 1){
        printf("Finishing by signal\n");
    }

    exit(EXIT_SUCCESS);
}





int listo = 0, candidate = 1, votar = 0;

void int_handler(int sig) {
    // Liberar espacio
    printf("(interrupt) liberandooooo....\n");
    exit(EXIT_SUCCESS);
}

void usr1_handler(int sig) {
    if(listo){
        votar = 1;
    }
    listo = 1;
}


void usr2_handler(int sig) {
    candidate = 0;
}

char generarVoto(){
    srand(time(NULL) + getpid());
    int rand_num = rand() % 2;

    printf("random: %i\n", rand_num);

    if(rand_num == 0){
        return 'N';
    }else{
        return 'Y';
    }
}

// Codigo que van a ejecutar los hijos
int votante(sem_t *sem1, sem_t *sem2, int n_procs, FILE* elecciones){
    struct sigaction act_int, act_usr1, act_usr2;
    sigset_t setUsr1, oset;
    int* buffer;
    int i, ret;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

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

    // Set user 2 handler
    act_usr2.sa_handler = usr2_handler;
    if (sigaction(SIGUSR2, &act_usr2, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Block everything but ready signal
    sigemptyset(&setUsr1);
    sigaddset(&setUsr1, SIGUSR1);
    if(sigprocmask(SIG_BLOCK, &setUsr1, &oset) < 0){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    sem_post(sem1); // permitirle continuar al padre, el hijo esta listo a recibir el ready

    // Wait for user signal from system (ready)
    while(!listo){
        sigsuspend(&act_int.sa_mask);
    }

    // Return to normal
    if(sigprocmask(SIG_UNBLOCK, &setUsr1, &oset) < 0){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* --------------------------- Solicitar el candidato -------------------------- */

    // Block everything but sigusr2 signal
    ret = 4;
    while(ret == 4){
        if(sem_wait(sem1) < 0){
            ret = errno;
        }else{
            ret = 0;
        }
    }

    if(candidate){
        buffer = read_pids(n_procs);

        // Decirle a sus hermanos que no son candidatos
        for(i = 0; i < n_procs; i++){
            if(buffer[i] != getpid()){
                if(kill(buffer[i], SIGUSR2) == -1){ // Enviar sigusr2 al resto de hijos (no candidatos)
                    perror("kill");
                    exit(EXIT_FAILURE);
                }
            }
        }

        for(i = 0; i < n_procs-1; i++){
            sem_post(sem1);
        }
    }

    //printf("Soy candidato? %d\n", candidate);

    /* ----------------------------- Votar ---------------------------- */

    if(candidate){
        // Esperar a q los hermanos votantes estan readys (semaforo 2)
        for (i = 0; i < n_procs-1; i++){
            ret = 4;
            while(ret == 4){
                if(sem_wait(sem2) < 0){
                    ret = errno;
                }else{
                    ret = 0;
                }
            }
        }

        // Enviar sigusr1 a los votantes para que voten
        for(i = 0; i < n_procs; i++){
            if(buffer[i] != getpid()){
                if(kill(buffer[i], SIGUSR1) == -1){ 
                    perror("kill");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // El candidato tambien vota
        fprintf(elecciones, "%c ", generarVoto());
        fflush(elecciones);

        // Comprobar votacion
        /*
        int finished = 1;

        while(!finished){
            //usleep(1000);

            fseek(elecciones, 0, SEEK_SET);
            read = getline(&line, &len, elecciones);
            printf("length: %ld\n", len);

            usleep(500000);

        }*/
    
    }else{

        // Crear handler de SIGUSR1
        act_usr1.sa_handler = usr1_handler;
        if (sigaction(SIGUSR1, &act_usr1, NULL) < 0) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        // Block everything but voting signal
        sigemptyset(&setUsr1);
        sigaddset(&setUsr1, SIGUSR1);
        if(sigprocmask(SIG_BLOCK, &setUsr1, NULL) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }
    
        sem_post(sem2);

        // Wait for voting signal from candidate (sigusr1)
        while(!votar){
            sigsuspend(&act_usr1.sa_mask);
        }

        // Return to normal
        if(sigprocmask(SIG_UNBLOCK, &setUsr1, &oset) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }

        // Votar
        if(fprintf(elecciones, "%c ", generarVoto()) < 0){
            perror("fprintf");
            exit(EXIT_FAILURE);
        }
        printf("[VOTANTE] acabo de votar a Trump 👽\n");
        fflush(elecciones);

    }

    //espera no activa hasta la siguiente ronda

    sleep(99);

    exit(EXIT_SUCCESS);
}





