import os
import time


def main():
    """
    Executa um processo de teste que exibe seu PID e cria apenas uma thread.
    """
    while True:
        print(f"Processo teste pai pid: {os.getpid()}", flush=True)
        time.sleep(1)


if __name__ == "__main__":
    main()
