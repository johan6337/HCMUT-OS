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


int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    // printf("sys_killall excuted\n");
    // return 0;

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

    struct queue_t *running_list = caller->running_list;
    struct queue_t *mlq_ready_queue = caller->mlq_ready_queue;
    
    // Check processes in the running_list
    for (int i = 0; i < running_list->size; i++) {
        struct pcb_t *proc = running_list->proc[i];
        
        // Compare process name
        if (strcmp(proc->path, proc_name) == 0) {
            printf("Terminating running process with PID: %d - name: \"%s\"\n", proc->pid, proc_name);
            
            // Remove from running_list by shifting elements
            for (int j = i; j < running_list->size - 1; j++) {
                running_list->proc[j] = running_list->proc[j + 1];
            }

            running_list->size--;
            i--;
            terminated_count++;
        }
    }
    
#ifdef MLQ_SCHED
    // Check processes in MLQ ready queues
    for (int prio = 0; prio < MAX_PRIO; prio++) {
        struct queue_t *queue = &(mlq_ready_queue[prio]);
        for (int i = 0; i < queue->size; i++) {
            struct pcb_t *proc = queue->proc[i];
            
            // Compare process name
            if (strcmp(proc->path, proc_name) == 0) {
                printf("Terminating running process with PID: %d - name: \"%s\"\n", proc->pid, proc_name);
                
                // Remove from ready queue by shifting elements
                for (int j = i; j < queue->size - 1; j++) {
                    queue->proc[j] = queue->proc[j + 1];
                }

                queue->size--;
                i--;
                terminated_count++;
            }
        }
    }
#else
    // For non-MLQ scheduler, check the ready queue
    struct queue_t *ready_queue = caller->ready_queue;
    for (int i = 0; i < ready_queue->size; i++) {
        struct pcb_t *proc = ready_queue->proc[i];
        
        // Compare process name
        if (strcmp(proc->path, proc_name) == 0) {
            printf("Terminating running process with PID: %d - name: \"%s\"\n", proc->pid, proc_name);
            
            // Remove from ready queue by shifting elements
            for (int j = i; j < ready_queue->size - 1; j++) {
                ready_queue->proc[j] = ready_queue->proc[j + 1];
            }
            
            ready_queue->size--;
            i--;
            terminated_count++;
        }
    }
#endif
    
    printf("Terminated %d processes with name: \"%s\"\n", terminated_count, proc_name);
    
    return terminated_count; 
}
