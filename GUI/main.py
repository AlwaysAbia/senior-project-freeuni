import customtkinter as ctk
import requests
from robot_hostnames import robots

# Set appearance and theme
ctk.set_appearance_mode("System")
ctk.set_default_color_theme("blue")

# Create app window
app = ctk.CTk()
app.geometry("1100x600")
app.title("Rover Swarm Control Panel")

# Configure grid weights for better layout
app.grid_columnconfigure((0, 1, 2), weight=1)
app.grid_columnconfigure(3, weight=0)  # Request log column

# ========== ROBOT SELECTION ==========
header_frame = ctk.CTkFrame(app)
header_frame.grid(row=0, column=0, columnspan=3, padx=20, pady=20, sticky="ew")
header_frame.grid_columnconfigure(1, weight=1)

ctk.CTkLabel(header_frame, text="Select Robot:", font=ctk.CTkFont(size=16, weight="bold")).grid(row=0, column=0,
                                                                                                padx=20, pady=15,
                                                                                                sticky="w")
robot_dropdown = ctk.CTkOptionMenu(header_frame, values=["1", "2", "3"], width=120)
robot_dropdown.grid(row=0, column=1, padx=20, pady=15, sticky="w")
robot_dropdown.set("1")

# ========== UPDATE STATE SECTION ==========
state_frame = ctk.CTkFrame(app)
state_frame.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(state_frame, text="Update State", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0, padx=15,
                                                                                              pady=(15, 10), sticky="w")

def send_update_state():
    selected_index = int(robot_dropdown.get()) - 1
    base_url = f"{robots[selected_index]}/updateState"

    state = state_dropdown.get()
    params = {"mode": state}

    if state == "IDLE" and idle_thresh_entry.get():
        params["idle_thresh"] = idle_thresh_entry.get()
    elif state == "LINE" and line_dist_entry.get():
        params["line_nodeDist"] = line_dist_entry.get()
    elif state == "POLYGON" and polygon_radius_entry.get():
        params["polygon_radius"] = polygon_radius_entry.get()

    # Build full URL with parameters
    if len(params) > 0:
        param_string = "&".join([f"{key}={value}" for key, value in params.items()])
        full_url = f"{base_url}?{param_string}"
    else:
        full_url = base_url

    try:
        response = requests.get(base_url, params=params, timeout=2)
        print("Update State Response:", response.text)
        update_status_label.configure(text="✓ Update sent successfully", text_color="green")
        log_request("GET", full_url, response.text)
    except Exception as e:
        print("Error sending update state:", e)
        update_status_label.configure(text="✗ Update failed", text_color="red")
        log_request("GET", full_url, error=str(e))

update_btn = ctk.CTkButton(state_frame, text="Send Update", command=send_update_state, width=200)
update_btn.grid(row=1, column=0, padx=15, pady=15, sticky="w")

update_status_label = ctk.CTkLabel(state_frame, text="", font=ctk.CTkFont(size=12))
update_status_label.grid(row=2, column=0, padx=15, pady=5, sticky="w")

state_dropdown = ctk.CTkOptionMenu(state_frame, values=["OFF", "IDLE", "LINE", "POLYGON", "MANUAL"], width=200)
state_dropdown.grid(row=3, column=0, padx=15, pady=5, sticky="w")
state_dropdown.set("OFF")

idle_thresh_entry = ctk.CTkEntry(state_frame, placeholder_text="Idle Threshold", width=200)
idle_thresh_entry.grid(row=4, column=0, padx=15, pady=5, sticky="w")

line_dist_entry = ctk.CTkEntry(state_frame, placeholder_text="Line Distance", width=200)
line_dist_entry.grid(row=5, column=0, padx=15, pady=5, sticky="w")

polygon_radius_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Radius", width=200)
polygon_radius_entry.grid(row=6, column=0, padx=15, pady=5, sticky="w")

# ========== MOVE COMMAND ==========
move_frame = ctk.CTkFrame(app)
move_frame.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(move_frame, text="Move Command", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0, padx=15,
                                                                                             pady=(15, 10), sticky="w")

def send_move_command():
    selected_index = int(robot_dropdown.get()) - 1
    base_url = f"{robots[selected_index]}/move"

    params = {}
    if left_entry.get():
        params["l"] = left_entry.get()
    if right_entry.get():
        params["r"] = right_entry.get()
    if back_entry.get():
        params["b"] = back_entry.get()

    # Build full URL with parameters
    if len(params) > 0:
        param_string = "&".join([f"{key}={value}" for key, value in params.items()])
        full_url = f"{base_url}?{param_string}"
    else:
        full_url = base_url

    try:
        response = requests.get(base_url, params=params, timeout=2)
        print("Move Response:", response.text)
        move_status_label.configure(text="✓ Move sent successfully", text_color="green")
        log_request("GET", full_url, response.text)
    except Exception as e:
        print("Error sending move command:", e)
        move_status_label.configure(text="✗ Move failed", text_color="red")
        log_request("GET", full_url, error=str(e))

move_btn = ctk.CTkButton(move_frame, text="Send Move", command=send_move_command, width=200)
move_btn.grid(row=1, column=0, padx=15, pady=15, sticky="w")

move_status_label = ctk.CTkLabel(move_frame, text="", font=ctk.CTkFont(size=12))
move_status_label.grid(row=2, column=0, padx=15, pady=5, sticky="w")

left_entry = ctk.CTkEntry(move_frame, placeholder_text="Left (l)", width=200)
left_entry.grid(row=3, column=0, padx=15, pady=5, sticky="w")

right_entry = ctk.CTkEntry(move_frame, placeholder_text="Right (r)", width=200)
right_entry.grid(row=4, column=0, padx=15, pady=5, sticky="w")

back_entry = ctk.CTkEntry(move_frame, placeholder_text="Back (b)", width=200)
back_entry.grid(row=5, column=0, padx=15, pady=5, sticky="w")

# ========== STATUS COMMAND ==========
status_frame = ctk.CTkFrame(app)
status_frame.grid(row=1, column=2, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(status_frame, text="Status Command", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0,
                                                                                                 padx=15, pady=(15, 10),
                                                                                                 sticky="w")

def send_status_command():
    selected_index = int(robot_dropdown.get()) - 1
    url = f"{robots[selected_index]}/status"

    try:
        response = requests.get(url, timeout=2)
        print("Status Response:", response.text)
        status_text.delete("1.0", "end")
        status_text.insert("1.0", response.text)
        status_status_label.configure(text="✓ Status retrieved", text_color="green")
        log_request("GET", url, response.text)
    except Exception as e:
        print("Error sending status command:", e)
        status_text.delete("1.0", "end")
        status_text.insert("1.0", f"Error: {str(e)}")
        status_status_label.configure(text="✗ Status failed", text_color="red")
        log_request("GET", url, error=str(e))

status_btn = ctk.CTkButton(status_frame, text="Send Status", command=send_status_command, width=200)
status_btn.grid(row=1, column=0, padx=15, pady=15, sticky="w")

status_status_label = ctk.CTkLabel(status_frame, text="", font=ctk.CTkFont(size=12))
status_status_label.grid(row=2, column=0, padx=15, pady=5, sticky="w")

status_text = ctk.CTkTextbox(status_frame, width=200, height=120)
status_text.grid(row=3, column=0, padx=15, pady=5, sticky="w")

# ========== REQUEST LOG SECTION ==========
log_frame = ctk.CTkFrame(app)
log_frame.grid(row=0, column=3, rowspan=2, padx=10, pady=20, sticky="nsew")
log_frame.grid_rowconfigure(1, weight=1)

ctk.CTkLabel(log_frame, text="HTTP Request Log", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0,
                                                                                                padx=15, pady=(15, 10),
                                                                                                sticky="w")

request_log = ctk.CTkTextbox(log_frame, width=300, height=400, font=ctk.CTkFont(family="Courier", size=11))
request_log.grid(row=1, column=0, padx=15, pady=(0, 10), sticky="nsew")

def clear_log():
    request_log.delete("1.0", "end")

clear_btn = ctk.CTkButton(log_frame, text="Clear Log", command=clear_log, width=100)
clear_btn.grid(row=2, column=0, padx=15, pady=(0, 15), sticky="w")

def log_request(method, url, response_text="", error=None):
    """Log HTTP request details to the text widget"""
    import datetime
    timestamp = datetime.datetime.now().strftime("%H:%M:%S")

    log_entry = f"[{timestamp}] {method} Request:\n"
    log_entry += f"URL: {url}\n"

    if error:
        log_entry += f"❌ ERROR: {error}\n"
    else:
        log_entry += f"✅ Response: {response_text[:100]}{'...' if len(response_text) > 100 else ''}\n"

    log_entry += "-" * 40 + "\n\n"

    request_log.insert("end", log_entry)
    request_log.see("end")  # Auto-scroll to bottom

# Start the GUI loop
app.mainloop()