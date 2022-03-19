/**
 * @file voting.h
 * @author Fernando Ónega and Alba Delgado
 * @brief Computation of the POW.
 * @version 1.0
 * @date 2022-02-28
 *
 * @copyright Copyright (c) 2022
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>

#ifndef _VOTING_H
#define _VOTING_H

int votante(sem_t *sem1, sem_t *sem2, int n_procs, FILE* elecciones);
void int_handler(int sig);
void padre_int_handler(int sig);
void candidato_comprobarVotacion(FILE* elecciones, int n_procs);
void votante_votar(FILE* elecciones);
#endif