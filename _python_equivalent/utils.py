
## Python util script that provide functions for a script that Synchronize folders between two devices for remote development
# Importing required modules
import os
import sys
import time
import shutil
import socket
import zipfile
import threading
from watchdog.events import FileSystemEventHandler

# Constants
SOCKET_PORT = 5151
IGNORED_FOLDERS = ["__pycache__", ".git", ".code-workspace"]
BUFFER_SIZE = 32768
LAST_MODIFIED_INTERVAL = 1.0
COLOR_RED = "\033[91m"
COLOR_GREEN = "\033[92m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_RESET = "\033[0m"

def custom_print(message: str, color: str = COLOR_RESET) -> None:
	""" Function for printing a message with a color
	Args:
		message (str):	Message to print
		color (str):	Color to use
	"""
	datetime = time.strftime("%Y/%m/%d %H:%M:%S")
	print(f"{color}[{datetime}] {message}{COLOR_RESET}")

def send_message(socket: socket.socket, message: str|bytes) -> None:
	""" Function for sending a message to a socket
	Args:
		socket (socket):	Socket to send the message to
		message (str):		Message to send
	"""
	# Send message length and then the message
	if type(message) == str:
		message = message.encode()
	msg_length = len(message)
	socket.send(msg_length.to_bytes(4, byteorder="big"))
	total_sent = 0
	while total_sent < msg_length:
		to_send = min(BUFFER_SIZE, msg_length - total_sent)
		sent = socket.send(message[total_sent:total_sent + to_send])
		if sent == 0:
			raise RuntimeError("Socket connection broken")
		total_sent += sent

def receive_message(socket: socket.socket) -> bytes:
	""" Function for receiving a message from a socket
	Args:
		socket (socket):	Socket to receive the message from
	Returns:
		str:	Received message
	"""
	# Receive message length and then the message
	msg_length = int.from_bytes(socket.recv(4), byteorder="big")
	total_received = 0
	message = b""
	while total_received < msg_length:
		to_receive = min(BUFFER_SIZE, msg_length - total_received)
		received = socket.recv(to_receive)
		if received == b"":
			raise RuntimeError("Socket connection broken")
		message += received
		total_received += len(received)
	return message

class ServerHandler(FileSystemEventHandler):
	""" Class for handling the whole server script """

	def __init__(self, path: str, password: str, clients: list[socket.socket] = []):
		""" Constructor for the ServerHandler class.
		Args:
			path (str):		Path to the folder to be synchronized
			password (str):	Password to accept the connection
		"""
		super().__init__()
		self.folder_path = path
		self.password = password
		self.clients = clients
		self.last_modified_interval = LAST_MODIFIED_INTERVAL
		self.last_modified = ("", 0)

		# Special fix to allow for color on Windows 10
		if os.name == "nt":
			os.system("color")

	def on_moved(self, event):

		# If path is ignored, return
		if any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get old and new paths
		old_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")
		new_path = event.dest_path.replace("\\", "/").replace(self.folder_path, "")
				
		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return
		else:
			self.last_modified = (new_path, time.time())

		message = f"MOVED|{old_path}|{new_path}"
		custom_print(f"File moved from {old_path} to {new_path}", COLOR_CYAN)
		
		# Send to the clients old path and new path
		for client in self.clients:
			try:
				send_message(client, message)
			except Exception as e:
				custom_print(f"Error while sending MOVED event to client: {e}, removing client...", COLOR_RED)
				self.clients.remove(client)

	def on_deleted(self, event):

		# If path is ignored, return
		if any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get new path and send to the clients that the path has been deleted
		new_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")

		# Check if the file really exists
		time.sleep(1)
		if os.path.exists(event.src_path):
			return

		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return
		else:
			self.last_modified = (new_path, time.time())

		message = f"DELETED|{new_path}"
		custom_print(f"File deleted: {new_path}", COLOR_CYAN)
		for client in self.clients:
			try:
				send_message(client, message)
			except Exception as e:
				custom_print(f"Error while sending DELETED event to client: {e}, removing client...", COLOR_RED)
				self.clients.remove(client)

	def on_modified(self, event):

		# If path is ignored, return
		if event.is_directory or any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get new path
		new_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")

		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return

		custom_print(f"File modified: {new_path}", COLOR_CYAN)
		self.last_modified = (new_path, time.time())
		# Send to the clients that a file has been modified and the file content
		message = f"FILE_MODIFIED|{new_path}"
		content = ""
		count = 0
		while content == "":
			count += 1
			if count > 10:
				custom_print("Warning #Last: Can't read file, giving up...", COLOR_YELLOW)
				return
			try:
				with open(event.src_path, "rb") as file:
					content = file.read()
					if len(content) == 0:
						custom_print(f"Warning #{count}: File is empty, waiting for more data...", COLOR_YELLOW)
						content = ""
						time.sleep(0.1)
						continue
					for client in self.clients:
						try:
							send_message(client, message)
							send_message(client, content)
						except Exception as e:
							custom_print(f"Error while sending FILE_MODIFIED event to client: {e}, removing client...", COLOR_RED)
							self.clients.remove(client)
					self.last_modified = (new_path, time.time())
					return
			except Exception as e:
				custom_print(f"Warning while opening file: {e}", COLOR_YELLOW)
			time.sleep(self.last_modified_interval / 2)
			self.last_modified = (new_path, time.time())
	
	def new_client(self, client_socket: socket) -> None:
		""" Function for handling a new client connection
		Args:
			client_socket (socket):	Socket of the new client
		"""
		# Receive the password
		password = receive_message(client_socket).decode()
		if password != self.password:
			client_socket.send("KO".encode())
			return
		custom_print(f"Client connected (good password): {client_socket.getpeername()}", COLOR_GREEN)
		
		## Send the whole zip file
		# Zip the folder
		file_name = f"temporary_zipped_file_51_server.zip"
		zip_file = zipfile.ZipFile(file_name, "w", zipfile.ZIP_DEFLATED, compresslevel = 9)
		for root, _, files in os.walk(self.folder_path):
			for file in files:
				if any(ignored_folder in root for ignored_folder in IGNORED_FOLDERS):
					continue
				zip_file.write(os.path.join(root, file), os.path.join(root.replace(self.folder_path, ""), file))
		zip_file.close()

		# Send the zip file
		custom_print(f"Sending zip file to client: {client_socket.getpeername()}", COLOR_GREEN)
		with open(file_name, "rb") as file:
			data = file.read()
			custom_print(f"Sending {len(data)} bytes to client: {client_socket.getpeername()}\n", COLOR_GREEN)
			send_message(client_socket, data)
		os.remove(file_name)

		# Add the client to the list of clients
		self.clients.append(client_socket)

		# Start listening to the client on a new thread
		threading.Thread(target=self.listen_to_client, args=(client_socket,)).start()
	
	def on_client_message(self, client_socket: socket, message: str) -> None:
		""" Function for handling a message from a client
		Args:
			client_socket (socket):	Socket of the client
			message (str):			Message from the client
		"""
		# Split the message
		split_message = message.split("|")
		action = split_message[0]

		# Handle the message
		if action == "MOVED":
			old_path = self.folder_path + split_message[1]
			new_path = self.folder_path + split_message[2]

			# Create directories if they don't exist
			if not os.path.exists(os.path.dirname(new_path)):
				os.makedirs(os.path.dirname(new_path))

			try:
				self.last_modified = (split_message[2], time.time())
				os.rename(old_path, new_path)

			except Exception as e:
				custom_print(f"Error while moving a file: {e}", COLOR_RED)
			custom_print(f"Recv: File moved from {old_path} to {new_path}", COLOR_GREEN)

			for client in self.clients:
				if client != client_socket:
					try:
						send_message(client, message)
					except Exception as e:
						custom_print(f"Error while rebroadcasting a message to client: {e}, removing client...", COLOR_RED)
						self.clients.remove(client)

		elif action == "DELETED":
			new_path = self.folder_path + split_message[1]
			try:
				self.last_modified = (split_message[1], time.time())
				os.remove(new_path)
			except Exception as e:
				custom_print(f"Error while deleting a file: {e}", COLOR_RED)
			custom_print(f"Recv: File deleted: {new_path}", COLOR_GREEN)

			for client in self.clients:
				if client != client_socket:
					try:
						send_message(client, message)
					except Exception as e:
						custom_print(f"Error while rebroadcasting a message to client: {e}, removing client...", COLOR_RED)
						self.clients.remove(client)

		elif action == "FILE_MODIFIED":
			new_path = self.folder_path + split_message[1]
			content = receive_message(client_socket)

			# Create directories if they don't exist
			if not os.path.exists(os.path.dirname(new_path)):
				os.makedirs(os.path.dirname(new_path))

			with open(new_path, "wb") as file:
				self.last_modified = (split_message[1], time.time())
				file.write(content)
			custom_print(f"Recv: File modified: {new_path}", COLOR_GREEN)

			for client in self.clients:
				if client != client_socket:
					try:
						send_message(client, message)
						send_message(client, content)
					except Exception as e:
						custom_print(f"Error while rebroadcasting a message to client: {e}, removing client...", COLOR_RED)
						self.clients.remove(client)
		
		else:
			custom_print(f"Recv: Unknown message: {message}", COLOR_YELLOW)
			raise Exception("Unknown message")


	def listen_to_client(self, client_socket: socket) -> None:
		""" Function for listening to a client
		Args:
			client_socket (socket):	Socket of the client
		"""
		while True:
			try:
				# Receive the message
				message = receive_message(client_socket).decode()
				self.on_client_message(client_socket, message)

			except Exception as e:
				custom_print(f"Error while handling a message: {e}, removing client...", COLOR_RED)
				client_socket.close()
				self.clients.remove(client_socket)
				break

class ClientHandler(FileSystemEventHandler):
	""" Class for handling the whole client script """

	def __init__(self, path: str, password: str) -> None:
		""" Constructor for the ClientHandler class.
		Args:
			path (str):			Path to the folder to be synchronized
			password (str):		Password to accept the connection
			socket (socket):	Socket of the client to the server
		"""
		super().__init__()
		self.folder_path = path
		self.password = password
		self.last_modified_interval = LAST_MODIFIED_INTERVAL
		self.last_modified = ("", 0)

		# Special fix to allow for color on Windows 10
		if os.name == "nt":
			os.system("color")
	
	def negociate_folder(self, socket: socket) -> None:
		""" Function for negociating the folder with the server
		Args:
			socket (socket):	Socket of the server
		"""
		# Delete all the files and directories in the folder
		custom_print(f"Deleting all files and directories in {self.folder_path}", COLOR_GREEN)
		for root, _, _ in os.walk(self.folder_path):
			root = root.replace("\\", "/")
			if root == self.folder_path:
				continue
			if any(ignored_folder in root for ignored_folder in IGNORED_FOLDERS):
				continue
			shutil.rmtree(root)

		# Send the password
		self.socket = socket
		send_message(socket, self.password)

		# Receive the zip file
		file_name = f"temporary_zipped_file_51_client.zip"
		custom_print(f"Waiting for the zip file...", COLOR_GREEN)
		with open(file_name, "wb") as file:
			data = receive_message(socket)
			file.write(data)
		file.close()

		# Unzip the file
		custom_print(f"Unzipping the file...", COLOR_GREEN)
		zip_file = zipfile.ZipFile(file_name, "r")
		os.makedirs(self.folder_path, exist_ok=True)
		zip_file.extractall(self.folder_path)
		zip_file.close()
		os.remove(file_name)
		custom_print(f"File unzipped", COLOR_GREEN)
	
	def on_moved(self, event):

		# If path is ignored, return
		if any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get old and new paths
		old_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")
		new_path = event.dest_path.replace("\\", "/").replace(self.folder_path, "")

		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return
		else:
			self.last_modified = (new_path, time.time())

		message = f"MOVED|{old_path}|{new_path}"
		custom_print(f"Moved from {old_path} to {new_path}", COLOR_CYAN)

		# Send the message to the server
		send_message(self.socket, message)

	def on_deleted(self, event):

		# If path is ignored, return
		if any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get new path
		new_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")

		# Check if the file really exists
		time.sleep(1)
		if os.path.exists(event.src_path):
			return
		
		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return
		else:
			self.last_modified = (new_path, time.time())

		message = f"DELETED|{new_path}"
		custom_print(f"Deleted: {new_path}", COLOR_CYAN)
		send_message(self.socket, message)
	
	def on_modified(self, event):

		# If path is ignored, return
		if event.is_directory or any(ignored_folder in event.src_path for ignored_folder in IGNORED_FOLDERS):
			return

		# Get new path
		new_path = event.src_path.replace("\\", "/").replace(self.folder_path, "")

		# Check if the file has been modified recently
		if new_path == self.last_modified[0] and (time.time() - self.last_modified[1]) < self.last_modified_interval:
			return

		# Send the message to the server
		self.last_modified = (new_path, time.time())
		message = f"FILE_MODIFIED|{new_path}"
		content = ""
		count = 0
		while content == "":
			count += 1
			if count > 10:
				custom_print("Warning #Last: Can't read file, giving up...", COLOR_YELLOW)
				return
			try:
				with open(event.src_path, "rb") as file:
					content = file.read()
					if len(content) == 0:
						custom_print(f"Warning #{count}: File {new_path} is empty", COLOR_YELLOW)
						content = ""
						time.sleep(0.1)
						continue
					custom_print(f"File modified: {new_path}", COLOR_CYAN)
					send_message(self.socket, message)
					send_message(self.socket, content)
					self.last_modified = (new_path, time.time())
					return
			except Exception as e:
				custom_print(f"Warning while sending a file: {e}", COLOR_YELLOW)
			time.sleep(self.last_modified_interval / 2)
			self.last_modified = (new_path, time.time())

	def client_loop(self, socket: socket) -> None:
		""" Function for listening to the server
		Args:
			socket (socket):	Socket of the server
		"""
		error_timeout = 0
		while True:
			try:
				# Receive the message
				message = receive_message(self.socket).decode()
				splitted_message = message.split("|")
				message_type = splitted_message[0]

				# Handle the message
				if message_type == "MOVED":
					# Get the old and new path
					old_path = splitted_message[1]
					new_path = splitted_message[2]
					custom_print(f"Recv: File moved from {old_path} to {new_path}", COLOR_GREEN)

					# Create directories if they don't exist
					if not os.path.exists(self.folder_path + new_path):
						os.makedirs(self.folder_path + new_path)

					# Move the file
					try:
						self.last_modified = (new_path, time.time())
						os.rename(self.folder_path + old_path, self.folder_path + new_path)
					except Exception as e:
						custom_print(f"Error while moving a file: {e}", COLOR_RED)

				elif message_type == "DELETED":
					# Get the new path
					new_path = splitted_message[1]
					custom_print(f"Recv: File deleted: {new_path}", COLOR_GREEN)

					# Delete the file
					try:
						self.last_modified = (new_path, time.time())
						os.remove(self.folder_path + new_path)
					except Exception as e:
						custom_print(f"Error while deleting a file: {e}", COLOR_RED)

				elif message_type == "FILE_MODIFIED":
					# Get the new path and the content
					new_path = splitted_message[1]
					content = receive_message(self.socket)
					custom_print(f"Recv: File modified: {new_path}", COLOR_GREEN)

					# Create directories if they don't exist
					if not os.path.exists(self.folder_path + new_path):
						os.makedirs(self.folder_path + new_path)

					# Modify the file
					with open(self.folder_path + new_path, "wb") as file:
						self.last_modified = (new_path, time.time())
						file.write(content)
				
				else:
					custom_print(f"Unknown message: {message}", COLOR_RED)
					raise KeyboardInterrupt()
				error_timeout = 0
			
			except KeyboardInterrupt as e:
				print("Stopping client...")
				socket.close()
				print("Client stopped")
				sys.exit(0)

			except Exception as e:
				error_timeout += 1
				print(f"Error while handling a message: {e}")
				if error_timeout > 10:
					print("Too many errors, stopping client...")
					socket.close()
					print("Client stopped")
					sys.exit(0)

