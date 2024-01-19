
import utils
import sys
import socket
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

PASSWORD = "SOMETHING YOU WANT THAT IS THE SAME AS CLIENT AND SERVER"
PATH = "./_Client/"
HOST = "play.paralya.fr"

# Create an event handler
handler = utils.ClientHandler(PATH, PASSWORD)

# Create an observer
observer = Observer()
observer.schedule(handler, PATH, recursive=True)

## Socket part
# Create a TCP socket
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.connect((HOST, utils.SOCKET_PORT))
print(f"Client connected to {HOST}:{utils.SOCKET_PORT}")

# Request the folder
handler.negociate_folder(server_socket)

# Start the observer
observer.start()
print("Client observer started...")

# Listen server
handler.client_loop(server_socket)

# Stop
print("Stopping client...")
server_socket.close()
observer.stop()

# Make a popup window to inform the user
from tkinter import messagebox
messagebox.showinfo("Client", "Client stopped")

