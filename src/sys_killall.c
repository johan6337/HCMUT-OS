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
#include <queue.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

// Declare the external variables
#ifdef MLQ_SCHED
// extern struct queue_t mlq_ready_queue[MAX_PRIO];
static struct queue_t mlq_ready_queue[MAX_PRIO];
#else
extern struct queue_t ready_queue;
#endif

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
    printf("-------sys_killall excuted-------\n");

    char proc_name[100];
    uint32_t data;

    //hardcode for demo only
    uint32_t memrg = regs->a1;
    
    /* TODO: Get name of the target proc */
    //proc_name = libread->->
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    /* TODO: Traverse proclist to terminate the proc
    *       stcmp to check the process match proc_name
    */
    //caller->running_list
    //caller->mlq_ready_queue

    /* TODO Maching and terminating 
    *       all processes with given
    *        name in var proc_name
    */

    int terminated_count = 0;

#ifdef MLQ_SCHED
    // Check processes in MLQ ready queues
    for(int prio = 0; prio < MAX_PRIO; prio++) {
        struct queue_t *queue = &(mlq_ready_queue[prio]);
        for(int m = 0; m < queue->size;) {
            struct pcb_t *proc = queue->proc[m];
            printf("Checking process PID: %d with path: \"%s\"\n", proc->pid, proc->path);
            
            // Compare process name
            if(strcmp(get_proc_name(proc->path), proc_name) == 0) {
                printf("Terminating running process with PID: %d - name: \"%s\"\n", proc->pid, proc_name);
                free(proc); // Free the process memory

                // Remove from ready queue by shifting elements
                for(int n = m; n < queue->size - 1; n++) {
                    queue->proc[n] = queue->proc[n + 1];
                }

                queue->size--;
                terminated_count++;
            } else {
                m++;
            }
        }
    }

#else
    // For non-MLQ scheduler, check the ready queue
    extern struct queue_t ready_queue;
    for(int m = 0; m < ready_queue->size;) {
        struct pcb_t *proc = ready_queue->proc[m];
        
        // Compare process name
        if(strcmp(get_proc_name(proc->path), proc_name) == 0) {
            printf("Terminating running process with PID: %d - name: \"%s\"\n", proc->pid, proc_name);
            free(proc); // Free the process memory

            // Remove from ready queue by shifting elements
            for(int n = m; n < ready_queue->size - 1; n++) {
                ready_queue->proc[n] = ready_queue->proc[n + 1];
            }
            
            ready_queue->size--;
            terminated_count++;
        } else {
            m++;
        }
    }
#endif
    
    printf("Terminated %d processes with name: \"%s\"\n", terminated_count, proc_name);
    return terminated_count; 
}


