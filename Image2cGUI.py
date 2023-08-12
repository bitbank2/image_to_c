import subprocess
import sys
import os
import ctypes
import platform  # <-- added

# Function to check if a module is installed
def is_module_installed(module_name):
    try:
        __import__(module_name)
        return True
    except ImportError:
        return False

# Function to check if pip is available
def is_pip_available():
    try:
        subprocess.run([sys.executable, "-m", "pip", "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except:
        return False

# Function to install a module using pip
def install_module(module_name):
    subprocess.check_call([sys.executable, "-m", "pip", "install", module_name])

# Check if tkinter is installed
if not is_module_installed("tkinter"):
    if not is_pip_available():
        print("pip is not installed on your system.")
        print("Please download and install Python from https://www.python.org/downloads/windows/")
        sys.exit()

    user_input = input("tkinter is not installed. Would you like to install it now? (yes/no): ")
    if user_input.lower() == "yes":
        install_module("tkinter")
    else:
        print("Exiting as tkinter is required for the GUI.")
        sys.exit()

import tkinter as tk
from tkinter import filedialog, messagebox

# Function to hide the console window
def hide_console_window():
    ctypes.windll.user32.ShowWindow(ctypes.windll.kernel32.GetConsoleWindow(), 0)

def select_input_file():
    filepath = filedialog.askopenfilename(
        title="Select Input Image",
        filetypes=[
            ("Supported files", "*.png;*.jpeg;*.jpg;*.bmp;*.tiff;*.tif;*.gif;*.ppm;*.tga;*.cgm;*.cal;*.pcx"),
            ("All files", "*.*")
        ]
    )
    if filepath:
        input_file_var.set(filepath)

def convert_image():
    input_file = input_file_var.get()
    if not input_file:
        return

    output_file = os.path.splitext(input_file)[0] + ".h"
    
    # Check OS and set command accordingly
    if platform.system() == "Windows":
        command = ["./image_to_c64"] if os.environ["PROCESSOR_ARCHITECTURE"] == "AMD64" else ["./image_to_c32"]
    elif platform.system() == "Darwin":  # Darwin indicates macOS
        command = ["./image_to_c"]
    else:
        messagebox.showerror("Error", "Unsupported Operating System!")
        return
    
    if strip_var.get():
        command.append("--strip")

    command.append(input_file)

    result = subprocess.run(command, stdout=open(output_file, 'w'))

    if result.returncode == 0:
        messagebox.showinfo("Success", f"Image conversion successful! Output saved to: {output_file}")
    else:
        messagebox.showerror("Error", "Image conversion failed!")

app = tk.Tk()
app.title("Image to C")
app.iconbitmap('app_icon.ico')

input_file_var = tk.StringVar()

input_frame = tk.Frame(app)
input_frame.pack(pady=20, padx=20, fill=tk.X)

input_label = tk.Label(input_frame, text="Input File:")
input_label.pack(side=tk.LEFT)

input_entry = tk.Entry(input_frame, textvariable=input_file_var, width=40)
input_entry.pack(side=tk.LEFT, padx=10, expand=True, fill=tk.X)

input_button = tk.Button(input_frame, text="Browse", command=select_input_file)
input_button.pack(side=tk.LEFT)

strip_var = tk.IntVar()
strip_checkbox = tk.Checkbutton(app, text="Strip", variable=strip_var)
strip_checkbox.pack(pady=10)

convert_button = tk.Button(app, text="Convert", command=convert_image)
convert_button.pack(pady=20)

# Hide the console window (only for Windows)
if platform.system() == "Windows":
    hide_console_window()

# Center the window on the screen
def center_window():
    app.update_idletasks()
    window_width = app.winfo_width()
    window_height = app.winfo_height()
    screen_width = app.winfo_screenwidth()
    screen_height = app.winfo_screenheight()
    x = (screen_width / 2) - (window_width / 2)
    y = (screen_height / 2) - (window_height / 2)
    app.geometry(f'+{int(x)}+{int(y)}')

center_window()
# Start the tkinter main loop
app.mainloop()
