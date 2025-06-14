             +--------------------------+
             |          CS 130          |
             | PROJECT 2: USER PROGRAMS |
             |     DESIGN DOCUMENT      |
             +--------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Zhechuan Yu <arahato@shanghaitech.edu.cn>
Ziheng Yang <yangzh2023@shanghaitech.edu.cn>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

               ARGUMENT PASSING
               ================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

No new 'struct'.

---- ALGORITHMS ----

>> A2: Briefly describe how you implemented argument parsing.  How do
>> you arrange for the elements of argv[] to be in the right order?
>> How do you avoid overflowing the stack page?

In 'process.c':

    argument parsing:
        we use the strtok_r() function to split the string from the command
        line, and the get words can now be used as the filenames and
        arguments, then call the start_process function with the file name
        and pass the arguments to the load() and setup_stack() function, in
        which the arguments and commands are pushed into the stack when the
        page is initialised

    arrange argv[]:
        In the load() function, we parse the strings in the order of last to
        first, in which the last string will be the command and the first one
        will be the last argument

    avoid overflowing the stack page:
        When overflowing occurs, exit(-1) will be executed.Then we can easily
        detect the overflowing.

---- RATIONALE ----

>> A3: Why does Pintos implement strtok_r() but not strtok()?
    The strtok_r() function is the restartable version of strtok(). We can
    use it in several threads at once.If we use strtok(), the program will
    crash with errors because it can't be used in several threads at once.

>> A4: In Pintos, the kernel separates commands into a executable name
>> and arguments.  In Unix-like systems, the shell does this
>> separation.  Identify at least two advantages of the Unix approach.

Advantage 1:In Unix, the argument parsing is done in user program memory,
even if the user program memory runs out, system will not crash.

Advantage 2:Shell can perform checks on the arguments before passing them to
the kernel.



                 SYSTEM CALLS
                 ============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

In threads/threads.h

    struct thread *parent;              the thread which creates this one.
    struct list children;               Threads that this one creates.
    struct list_elem child_elem;        List element for children list.
    int child_load_status;              Load status of its child.
    int child_exit_status;              Exit status of its child.
    struct list open_fd;                Fds the thread opens
    struct file *file;                  Executable file of this thread 
    struct semaphore process_wait;      Determine whether thread should wait


>> B2: Describe how file descriptors are associated with open files.
>> Are file descriptors unique within the entire OS or just within a
>> single process?

Every file is assigned to a unique descriptor marked with a number.The 'fd' is a non-negative integer with 0 and 1 as standard input and output respectively, rest all values can be used to allocate the file.

Just within a single process.

---- ALGORITHMS ----

>> B3: Describe your code for reading and writing user data from the
>> kernel.

First, check the buffer pointer, if it is illegal, exit.

Second, check which one the buffer pointer is doing, reading or writing.

Third,the descriptor will check if standard input or output operation is performed. Then it will acquire a lock and perform the reading or writing operation holding the lock and release the lock after the reading or writing operation is complete.

>> B4: Suppose a system call causes a full page (4,096 bytes) of data
>> to be copied from user space into the kernel.  What is the least
>> and the greatest possible number of inspections of the page table
>> (e.g. calls to pagedir_get_page()) that might result?  What about
>> for a system call that only copies 2 bytes of data?  Is there room
>> for improvement in these numbers, and how much?

We need to do at least one inspection. One call to pagedir_get_page() is needed. The greatest number of calls needed for the required inspection is 2.

For a call that copies only 2 bytes of data the number is also 1 and 2.

Maybe no room for improvement.

>> B5: Briefly describe your implementation of the "wait" system call
>> and how it interacts with process termination.

We use the process_wait() function inside the wait function. The child thread will be allowed to terminate and any resources it held would be released.

>> B6: Any access to user program memory at a user-specified address
>> can fail due to a bad pointer value.  Such accesses must cause the
>> process to be terminated.  System calls are fraught with such
>> accesses, e.g. a "write" system call requires reading the system
>> call number from the user stack, then each of the call's three
>> arguments, then an arbitrary amount of user memory, and any of
>> these can fail at any point.  This poses a design and
>> error-handling problem: how do you best avoid obscuring the primary
>> function of code in a morass of error-handling?  Furthermore, when
>> an error is detected, how do you ensure that all temporarily
>> allocated resources (locks, buffers, etc.) are freed?  In a few
>> paragraphs, describe the strategy or strategies you adopted for
>> managing these issues.  Give an example.

We prevent this issue by checking the validity of the buffer pointer first.  If a pointer is not valid, e.g, it points to either kernel memory or is a null pointer,a page fault wiil occur and page_fault() in exception.c functions. Then the process is terminated with sys_exit()

---- SYNCHRONIZATION ----

>> B7: The "exec" system call returns -1 if loading the new executable
>> fails, so it cannot return before the new executable has completed
>> loading.  How does your code ensure this?  How is the load
>> success/failure status passed back to the thread that calls "exec"?

When process_execute() is called, it returns the thread id of the thread if the execution was successful. If the value of the status of the thread is “FAILED” then the return value is -1.

Through the value of the child_status in the struct. It will change if the child thread is in success/failure status.

>> B8: Consider parent process P with child process C.  How do you
>> ensure proper synchronization and avoid race conditions when P
>> calls wait(C) before C exits?  After C exits?  How do you ensure
>> that all resources are freed in each case?  How about when P
>> terminates without waiting, before C exits?  After C exits?  Are
>> there any special cases?

When P waits for C, P has to stop and wait for C to execute and exit. When C exits, the locks acquired by the C are released.

If P calls wait after C exits, after checking, P has no children to wait for and P won’t wait.

When P terminates before C exits, all the children will be terminated and release their locks.

When P terminates after C exits, C’s locks will be released.

No special cases.

---- RATIONALE ----

>> B9: Why did you choose to implement access to user memory from the
>> kernel in the way that you did?

To find errors more easily with page fault interupt handler.

>> B10: What advantages or disadvantages can you see to your design
>> for file descriptors?

Advantages:

    File descriptors are unique for each process to avoid the race conditions.
Disadvantages:

    With more file descriptors, the cost is higher the system may be slower.

>> B11: The default tid_t to pid_t mapping is the identity mapping.
>> If you changed it, what advantages are there to your approach?

We didn’t change it because we can't find a better way.