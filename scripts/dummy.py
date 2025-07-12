import os
import time


if __name__ == "__main__":
    print(f"Starting dummy process with PID: {os.getpid()}")
    try:
        while True:
            print(f"Process still running with PID: {os.getpid()}")
            time.sleep(1)
    except KeyboardInterrupt:
        print("Process terminated by user")