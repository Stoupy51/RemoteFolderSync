
import utils
import sys
import socket
import time
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

PASSWORD = "SOMETHING YOU WANT THAT IS THE SAME AS CLIENT AND SERVER"
PATH = "/var/opt/minecraft/crafty/crafty-4/servers/ae5e9828-b7de-49ba-b0b0-3ba9584969db/Switch/datapacks/"
HOST = "0.0.0.0"

# Create an event handler
handler = utils.ServerHandler(PATH, PASSWORD)

# Create an observer
observer = Observer()
observer.schedule(handler, PATH, recursive=True)

# Start the observer
observer.start()
print("Server observer started...")

## Socket part
# Create a TCP socket
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((HOST, utils.SOCKET_PORT))
server_socket.listen(10)
print(f"Server started on {HOST}:{utils.SOCKET_PORT}")

# Infinite loop for clients
while True:
	try:
		# Wait for a new client
		client_socket, client_address = server_socket.accept()
		print(f"New client connected: {client_address}")

		# Handle the new client
		handler.new_client(client_socket)

	except KeyboardInterrupt:
		print("Stopping server...")
		for client in handler.clients:
			client.close()
		server_socket.close()
		observer.stop()
		print("Server stopped")
		sys.exit(0)

	except Exception as e:
		print(f"Error while handling a client: {e}")
		continue


