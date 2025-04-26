/*
* Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
*/

/* Sierra release
* Source Code License Grant: The authors hereby grant to Licensee
* personal permission to use and modify the Licensed Source Code
* for the sole purpose of studying while attending the course CO2018->
*/

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include "sched.h"
#include <queue.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// Helper function to get the basename of a path
const char* get_proc_name(const char *path) {
    const char *proc_name = path;
    for(const char *p = path; *p != '\0'; p++) {
        if (*p == '/') {
            proc_name = p + 1;
        }
    }

    return proc_name;
}


int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    printf("-------sys_killall executed-------\n");

    char proc_name[100];
    uint32_t data;
    uint32_t memrg = regs->a1;
    
    // Get name of the target proc
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }
    proc_name[i] = '\0';
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    pthread_mutex_lock(&lock);
    int terminated_count = 0; // Count of terminated processes
    int processed_pids[1000] = {0}; // Track already processed PIDs

    
#ifdef MLQ_SCHED
    if(caller->mlq_ready_queue) {
        for(int prio = 0; prio < MAX_PRIO; prio++) {
            struct queue_t *mlq_queue = &(caller->mlq_ready_queue[prio]);
            
            for(int m = mlq_queue->size - 1; m >= 0; m--) {
                struct pcb_t *proc = mlq_queue->proc[m];

                if (!proc || processed_pids[proc->pid]) continue;
                
                printf("[CHECKING] Process with PID: %d - Name: \"%s\"\n", proc->pid, proc_name);
                
                if(strcmp(get_proc_name(proc->path), proc_name) == 0) {                    
                    // Mark as terminated with value expected by get_mlq_proc
                    proc->is_terminated = 1;
                    processed_pids[proc->pid] = 1;
                    terminated_count++;
                    
                    // Don't remove from queue - let the scheduler handle that
                    // The scheduler will discard terminated processes
                    printf("[TERMINATED] Process with PID: %d - Name: \"%s\"\n", proc->pid, proc_name);
                }
            }
        }
    }
#else
    // For non-MLQ scheduler
    if(caller->ready_queue) {
        for(int m = caller->ready_queue->size - 1; m >= 0; m--) {
            struct pcb_t *proc = caller->ready_queue->proc[m];

            if (!proc || processed_pids[proc->pid]) continue;
            printf("[CHECKING] Process with PID: %d - Name: \"%s\"\n", proc->pid, proc_name);
            
            if(strcmp(get_proc_name(proc->path), proc_name) == 0) {
                proc->is_terminated = 1;
                processed_pids[proc->pid] = 1;
                terminated_count++;

                printf("[TERMINATED] process with PID: %d - Name: \"%s\"\n", proc->pid, proc_name);
            }
        }
    }
#endif
    
    pthread_mutex_unlock(&lock);
    printf("Terminated %d processes with name: \"%s\"\n", terminated_count, proc_name);
    
    return terminated_count; 
}
