# dummy_safe.py
import os
import time
import threading


def thread_worker():
    """Function for the thread to run."""
    thread_id = threading.get_native_id()
    while True:
        print(f"Processo teste thread: {thread_id} (em processo {os.getpid()})")
        time.sleep(1)


def child_process_main():
    """Main function for the forked child process."""
    print(f"Inicializando processo teste filho, PID: {os.getpid()}")
    # The child can safely create its own threads if it wants.
    while True:
        print(f"Processo teste filho pid: {os.getpid()} (pai: {os.getppid()})")
        time.sleep(1)


def parent_process_main():
    """Main function for the parent process after forking."""
    print(f"Inicializando processo teste pai, PID: {os.getpid()}")
    # The parent creates its thread *after* forking.
    thread = threading.Thread(target=thread_worker, daemon=True)
    thread.start()
    while True:
        print(f"Processo teste pai pid: {os.getpid()}")
        time.sleep(1)


if __name__ == "__main__":
    newpid = os.fork()
    if newpid == 0:
        # Code for the Child Process
        child_process_main()
    else:
        # Code for the Parent Process
        parent_process_main()
