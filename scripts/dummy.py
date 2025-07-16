import os
import time
import threading


def thread_worker():
    thread_id = threading.get_native_id()
    while True:
        print(f"[THREAD] Processo thread | TID: {thread_id}")
        time.sleep(1)


def child_process_main():
    while True:
        print(f"[FILHO] Processo filho | PID: {os.getpid()}")
        time.sleep(1)


def parent_process_main():
    thread = threading.Thread(target=thread_worker, daemon=True)
    thread.start()
    while True:
        print(f"[PAI] Processo pai | PID: [{os.getpid()}]")
        time.sleep(1)


if __name__ == "__main__":
    newpid = os.fork()
    if newpid == 0:
        child_process_main()
    else:
        parent_process_main()
