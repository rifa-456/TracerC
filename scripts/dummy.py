import os
import time
import threading


def worker(identifier_type):
    """
    Função que será executada pela thread e pelo processo filho.
    """
    current_id = threading.get_native_id() if identifier_type == "thread" else os.getpid()
    parent_pid = os.getppid() if identifier_type == "filho" else os.getpid()

    while True:
        if identifier_type == "thread":
            print(f"Processo teste thread: {current_id}")
        elif identifier_type == "filho":
            print(f"Processo teste filho pid: {current_id} (pai: {parent_pid})")
        else:
            print(f"Processo teste pai pid: {current_id}")
        time.sleep(1)


def main():
    """
    Executa um processo de teste que exibe seu PID, cria uma thread e um processo filho.
    """
    print(f"Inicializando processo teste pai, PID: {os.getpid()}")
    thread = threading.Thread(target=worker, args=("thread",), daemon=True) # cria uma nova thread que vai executar a função worker passando como argumento "thread"
    thread.start()
    newpid = os.fork()
    if newpid == 0:
        worker("filho")
    else:
        worker("pai")


if __name__ == "__main__":
    main()
