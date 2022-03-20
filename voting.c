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
    if(line)
        free(line);

    return buffer;
}

void padre_int_handler(int sig) {
    printf("    [INT PADRE]...\n");
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
    if(line)
        free(line);

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

    //Alarma
    //--------

    /* --------------------------- Initialize system -------------------------- */

    buffer = (int*)malloc(sizeof(int) * n_procs);
    
    // Create system file
    pids = fopen(CHILDS, "w+");
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
    fclose(elecciones);
    elecciones = fopen("elecciones", "a+");

    // Preparar handler interrupt
    sigemptyset(&(act.sa_mask));    
    act.sa_flags = 0;

    act.sa_handler = padre_int_handler;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    sem_unlink("/sem1");
    sem_unlink("/sem2");
    // Initialize semaphors
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
            votante(sem1, sem2, n_procs, elecciones, 1, 0);
        }else{
            fprintf(pids, "%d\n", childpid);
            buffer[i] = childpid;
        }
    }
    fflush(pids); // output pids to system file

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
    fclose(pids);
    sem_close(sem1);
    sem_close(sem2);
    sem_unlink("/sem1");
    sem_unlink("/sem2");

    if(exit_signaled == 1){
        printf("Finishing by signal\n");
    }

    exit(EXIT_SUCCESS);
}




// codigo que van a ejecutar los hijos //




int listo, candidate, votar, interrupt = 0;

void int_handler(int sig) {
    printf("    [INT]...\n");
    interrupt = 1; //no hace na
    exit(EXIT_SUCCESS);
}

void usr1_handler(int sig) {
    printf("    [USR1]...\n");
    if(listo){ // Ya hay candidato
        votar = 1;
    }
    listo = 1;
}

void usr2_handler(int sig) {
    printf("    [USR2]...\n");
    candidate = 0;
}

char generarVoto(){
    srand(time(NULL) + getpid());
    int rand_num = rand() % 2;

    if(rand_num == 0){
        return 'N';
    }else{
        return 'Y';
    }
}

// Codigo que van a ejecutar los hijos
int votante(sem_t *sem1, sem_t *sem2, int n_procs, FILE* elecciones, int init, int ronda){
    struct sigaction act_int, act_usr1, act_usr2;
    sigset_t set, oset;
    int* buffer;
    int i, ret;

    sigemptyset(&(act_int.sa_mask));
    act_int.sa_flags = 0;

    listo = 0; candidate = 1; votar = 0; // variables globales

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

    if(init == 1){
        /* --------------------------- Wait till ready -------------------------- */

        // Block everything but ready signal
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        if(sigprocmask(SIG_BLOCK, &set, &oset) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }

        sem_post(sem1);

        // Wait for user signal from system (ready)
        while(!listo){
            sigsuspend(&act_int.sa_mask);
        }

        // Return to normal
        if(sigprocmask(SIG_UNBLOCK, &set, &oset) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }
    }else{
        listo = 1;
    }

    //printf("Una hijita (%i)\n", getpid());

    /* --------------------------- Solicitar el candidato -------------------------- */

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

    /* ---------------------------------- Votar ---------------------------------- */

    if(candidate){

        // Esperar a los votantes
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

        // Enviar SIGUSR1 a los votantes
        for(i = 0; i < n_procs; i++){
            if(buffer[i] != getpid()){
                if(kill(buffer[i], SIGUSR1) == -1){ 
                    perror("kill");
                    exit(EXIT_FAILURE);
                }
            }
        }

        votante_votar(elecciones); // El candidato tambien vota
        candidato_comprobarVotacion(elecciones, n_procs, ronda);

        // Nueva ronda
        if(!interrupt){
            usleep(250000);
            sem_post(sem1);

            fprintf(elecciones, "\n");
            fflush(elecciones);
            
            for (i = 0; i < n_procs-1; i++){
                sem_post(sem2);
            }
            printf("NUEVO NUEVO NUEVO (candidato)\n");
            votante(sem1, sem2, n_procs, elecciones, 0, ronda+1);
        }
    
    }else{
        // Crear handler de SIGUSR1
        act_usr1.sa_handler = usr1_handler;
        if (sigaction(SIGUSR1, &act_usr1, NULL) < 0) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        // Block everything but voting signal
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        if(sigprocmask(SIG_BLOCK, &set, NULL) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }
    
        sem_post(sem2); // Listo para votar

        // Wait for voting signal from candidate (sigusr1)
        while(!votar){
            sigsuspend(&act_usr1.sa_mask);
        }

        // Return to normal
        if(sigprocmask(SIG_UNBLOCK, &set, &oset) < 0){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }

        votante_votar(elecciones);
    }

    /* ---------------------------------- Nueva ronda ---------------------------------- */

    // Votantes esperan
    ret = 4;
    printf("votante wait\n");
    while(ret == 4){
        if(sem_wait(sem2) < 0){
            ret = errno;
        }else{
            ret = 0;
        }
    }

    if(interrupt){
        if(buffer)
            free(buffer);

        printf("Adios\n");
        exit(EXIT_SUCCESS);
    }else{
        printf("NUEVO NUEVO NUEVO (votante)\n");
        votante(sem1, sem2, n_procs, elecciones, 0, ronda+1);
    }
}





void votante_votar(FILE* elecciones){
    if(fprintf(elecciones, "%c ", generarVoto()) < 0){
        perror("fprintf");
        exit(EXIT_FAILURE);
    }
    fflush(elecciones);
}

void candidato_comprobarVotacion(FILE* elecciones, int n_procs, int ronda){
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int i;

    int finished = 0, res = 0;
    int finalSize = (2*n_procs);

    while(!finished){
        fseek(elecciones, 0, SEEK_SET);

        for(i = 0; i < ronda; i++){
            read = getline(&line, &len, elecciones);
        }
        
        usleep(1000); // Espera no activa de 1ms

        read = -1;
        /*while((read = getline(&line, &len, elecciones)) == -1){
            printf("intento. %i\n", read);
            if(read == finalSize){
                printf("Candidate %d => [ %s] => ", getpid(), line);
                res = getResults(line, finalSize);
                if(res == 1){
                    printf("Accepted\n");
                }else{
                    printf("Rejected\n");
                }
                finished = 1;
            }
        }*/
        printf("Prueba!!!\n");
        finished = 1;
    }

    if(line)
        free(line);
}

int getResults(char* s, int size){
    int i, r = 0;

    if(s == NULL) return 0;

    for(i = 0; i < size; i += 2){
        if(s[i] == 'Y'){
            r++;
        }else{
            r--;
        }
    }

    if(r > 0){
        return 1;
    }

    return 0;
}


