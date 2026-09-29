#ifndef THREAD_H
#define THREAD_H
int  thread_create(void (*fn)(void));
void yield(void);
int  thread_self(void);
int  threads_running(void);   // จำนวน thread ที่ยังไม่จบ (นับรวม main)
#endif
