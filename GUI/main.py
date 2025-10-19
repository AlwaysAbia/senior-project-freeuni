import customtkinter as ctk
import requests
from robot_hostnames import robots

# ---------- Appearance ----------
ctk.set_appearance_mode("System")
ctk.set_default_color_theme("blue")

# ---------- App Window ----------
app = ctk.CTk()
app.geometry("1100x600")
app.title("Rover Swarm Control Panel")

# ---------- Layout Columns ----------
app.grid_columnconfigure((0, 1, 2), weight=1)
app.grid_columnconfigure(3, weight=0)  # Request log column
app.grid_rowconfigure((1, 2), weight=1)

# ---------- Header ----------
header_frame = ctk.CTkFrame(app)
header_frame.grid(row=0, column=0, columnspan=3, padx=20, pady=20, sticky="ew")
header_frame.grid_columnconfigure(1, weight=1)

ctk.CTkLabel(header_frame, text="Select Robot:", font=ctk.CTkFont(size=16, weight="bold")).grid(row=0, column=0, padx=20, pady=15, sticky="w")
robot_dropdown = ctk.CTkOptionMenu(header_frame, values=["1", "2", "3"], width=120)
robot_dropdown.grid(row=0, column=1, padx=20, pady=15, sticky="w")
robot_dropdown.set("1")

# ========== NEW: BROADCAST CHECKBOX ==========
broadcast_checkbox = ctk.CTkCheckBox(header_frame, text="Broadcast to All Robots", font=ctk.CTkFont(size=14, weight="bold"))
broadcast_checkbox.grid(row=0, column=2, padx=20, pady=15, sticky="w")
# =============================================

# ---------- REQUEST LOG ----------
log_frame = ctk.CTkFrame(app)
log_frame.grid(row=0, column=3, rowspan=3, padx=10, pady=20, sticky="nsew")
log_frame.grid_rowconfigure(1, weight=1)

ctk.CTkLabel(log_frame, text="HTTP Request Log", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0, padx=15, pady=(15, 10), sticky="w")

request_log = ctk.CTkTextbox(log_frame, width=300, height=400, font=ctk.CTkFont(family="Courier", size=11))
request_log.grid(row=1, column=0, padx=15, pady=(0, 10), sticky="nsew")

def clear_log():
    request_log.delete("1.0", "end")

ctk.CTkButton(log_frame, text="Clear Log", command=clear_log, width=100).grid(row=2, column=0, padx=15, pady=(0, 15), sticky="w")

def log_request(method, url, response_text="", error=None):
    import datetime
    timestamp = datetime.datetime.now().strftime("%H:%M:%S")
    log_entry = f"[{timestamp}] {method} Request:\nURL: {url}\n"
    if error:
        log_entry += f"❌ ERROR: {error}\n"
    else:
        log_entry += f"✅ Response: {response_text[:100]}{'...' if len(response_text) > 100 else ''}\n"
    log_entry += "-"*40 + "\n\n"
    request_log.insert("end", log_entry)
    request_log.see("end")

# ========== NEW: HELPER FUNCTION TO GET TARGET ROBOTS ==========
def get_target_robots():
    """Returns list of robot indices to send commands to"""
    if broadcast_checkbox.get():
        return list(range(len(robots)))  # All robots
    else:
        return [int(robot_dropdown.get()) - 1]  # Single selected robot
# ================================================================

# ---------- MOVE COMMAND ----------
move_frame = ctk.CTkFrame(app)
move_frame.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(move_frame, text="Manual Move", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10,5))

left_entry = ctk.CTkEntry(move_frame, placeholder_text="Left (l)", width=180)
left_entry.pack(anchor="w", padx=20, pady=2)
right_entry = ctk.CTkEntry(move_frame, placeholder_text="Right (r)", width=180)
right_entry.pack(anchor="w", padx=20, pady=2)
back_entry = ctk.CTkEntry(move_frame, placeholder_text="Back (b)", width=180)
back_entry.pack(anchor="w", padx=20, pady=2)

move_status_label = ctk.CTkLabel(move_frame, text="")
move_status_label.pack(anchor="w", padx=15, pady=(0,5))

def send_move_command():
    params = {}
    if left_entry.get(): params["l"] = left_entry.get()
    if right_entry.get(): params["r"] = right_entry.get()
    if back_entry.get(): params["b"] = back_entry.get()
    
    # ========== MODIFIED: BROADCAST SUPPORT ==========
    target_robots = get_target_robots()
    success_count = 0
    fail_count = 0
    
    for robot_idx in target_robots:
        base_url = f"{robots[robot_idx]}/move"
        full_url = f"{base_url}?{'&'.join([f'{k}={v}' for k,v in params.items()])}" if params else base_url
        try:
            response = requests.get(base_url, params=params, timeout=2)
            success_count += 1
            log_request("GET", full_url, response.text)
        except Exception as e:
            fail_count += 1
            log_request("GET", full_url, error=str(e))
    
    if fail_count == 0:
        move_status_label.configure(text=f"✓ Move sent to {success_count} robot(s)", text_color="green")
    elif success_count == 0:
        move_status_label.configure(text=f"✗ All {fail_count} moves failed", text_color="red")
    else:
        move_status_label.configure(text=f"⚠ {success_count} success, {fail_count} failed", text_color="orange")
    # ================================================

ctk.CTkButton(move_frame, text="Send Move", command=send_move_command, width=180).pack(anchor="w", padx=15, pady=10)

# ---------- STATUS COMMAND ----------
status_frame = ctk.CTkFrame(app)
status_frame.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(status_frame, text="Status", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10,5))

status_status_label = ctk.CTkLabel(status_frame, text="")
status_status_label.pack(anchor="w", padx=15, pady=(0,5))

def send_status_command():
    # ========== MODIFIED: BROADCAST SUPPORT ==========
    target_robots = get_target_robots()
    status_text.delete("1.0","end")
    success_count = 0
    fail_count = 0
    
    for robot_idx in target_robots:
        url = f"{robots[robot_idx]}/status"
        try:
            response = requests.get(url, timeout=2)
            success_count += 1
            status_text.insert("end", f"=== Robot {robot_idx + 1} ===\n{response.text}\n\n")
            log_request("GET", url, response.text)
        except Exception as e:
            fail_count += 1
            status_text.insert("end", f"=== Robot {robot_idx + 1} ===\nError: {str(e)}\n\n")
            log_request("GET", url, error=str(e))
    
    if fail_count == 0:
        status_status_label.configure(text=f"✓ Status from {success_count} robot(s)", text_color="green")
    elif success_count == 0:
        status_status_label.configure(text=f"✗ All {fail_count} requests failed", text_color="red")
    else:
        status_status_label.configure(text=f"⚠ {success_count} success, {fail_count} failed", text_color="orange")
    # ================================================
        
ctk.CTkButton(status_frame, text="Send Status", command=send_status_command, width=180).pack(anchor="w", padx=15, pady=5)

status_text = ctk.CTkTextbox(status_frame, width=350, height=150)
status_text.pack(anchor="w", padx=15, pady=5)

# ---------- UPDATE STATE ----------
state_frame = ctk.CTkFrame(app)
state_frame.grid(row=2, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")
state_frame.grid_columnconfigure((0,1,2,3), weight=1)

ctk.CTkLabel(state_frame, text="Update State", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0, columnspan=4, pady=(10,15))

# Grouped entries: Idle, Line, Polygon, General
# IDLE
ctk.CTkLabel(state_frame, text="IDLE Mode", font=ctk.CTkFont(size=13, weight="bold")).grid(row=1, column=0, pady=(0,5))
idle_thresh_entry = ctk.CTkEntry(state_frame, placeholder_text="Idle Threshold")
idle_thresh_entry.grid(row=2, column=0, pady=2, padx=5)

# LINE
ctk.CTkLabel(state_frame, text="LINE Mode", font=ctk.CTkFont(size=13, weight="bold")).grid(row=1, column=1, pady=(0,5))
line_dist_entry = ctk.CTkEntry(state_frame, placeholder_text="Line Distance")
line_dist_entry.grid(row=2, column=1, pady=2, padx=5)
line_alignTol_entry = ctk.CTkEntry(state_frame, placeholder_text="Line Align Tolerance")
line_alignTol_entry.grid(row=3, column=1, pady=2, padx=5)

# POLYGON
ctk.CTkLabel(state_frame, text="POLYGON Mode", font=ctk.CTkFont(size=13, weight="bold")).grid(row=1, column=2, pady=(0,5))
polygon_radius_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Radius")
polygon_radius_entry.grid(row=2, column=2, pady=2, padx=5)
polygon_sides_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Sides")
polygon_sides_entry.grid(row=3, column=2, pady=2, padx=5)
polygon_alignTol_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Align Tolerance")
polygon_alignTol_entry.grid(row=4, column=2, pady=2, padx=5)

# GENERAL
ctk.CTkLabel(state_frame, text="General", font=ctk.CTkFont(size=13, weight="bold")).grid(row=1, column=3, pady=(0,5))
neighbor_maxDist_entry = ctk.CTkEntry(state_frame, placeholder_text="Neighbor Max Distance")
neighbor_maxDist_entry.grid(row=2, column=3, pady=2, padx=5)

# Mode selection dropdown and Send Update button on same row
state_dropdown = ctk.CTkOptionMenu(state_frame, values=["OFF","IDLE","LINE","POLYGON","MANUAL"])
state_dropdown.grid(row=5, column=0, columnspan=2, pady=(10,5), padx=5, sticky="ew")
state_dropdown.set("OFF")

ctk.CTkButton(state_frame, text="Send Update", command=lambda: send_update_state(), width=200).grid(row=5, column=2, columnspan=2, pady=(10,5), padx=5, sticky="ew")

# Update status label below
update_status_label = ctk.CTkLabel(state_frame, text="")
update_status_label.grid(row=6, column=0, columnspan=4, pady=(0,10))

# ---------- SEND UPDATE FUNCTION ----------
def send_update_state():
    state = state_dropdown.get()
    params = {"mode": state}

    # General
    if neighbor_maxDist_entry.get(): params["neighbor_maxDist"] = neighbor_maxDist_entry.get()

    # IDLE
    if state=="IDLE" and idle_thresh_entry.get(): params["idle_thresh"] = idle_thresh_entry.get()
    # LINE
    if state=="LINE":
        if line_dist_entry.get(): params["line_nodeDist"] = line_dist_entry.get()
        if line_alignTol_entry.get(): params["line_alignTol"] = line_alignTol_entry.get()
    # POLYGON
    if state=="POLYGON":
        if polygon_radius_entry.get(): params["polygon_radius"] = polygon_radius_entry.get()
        if polygon_sides_entry.get(): params["polygon_sides"] = polygon_sides_entry.get()
        if polygon_alignTol_entry.get(): params["polygon_alignTol"] = polygon_alignTol_entry.get()

    # ========== MODIFIED: BROADCAST SUPPORT ==========
    target_robots = get_target_robots()
    success_count = 0
    fail_count = 0
    
    for robot_idx in target_robots:
        base_url = f"{robots[robot_idx]}/updateState"
        full_url = f"{base_url}?{'&'.join([f'{k}={v}' for k,v in params.items()])}" if params else base_url
        try:
            response = requests.get(base_url, params=params, timeout=2)
            success_count += 1
            log_request("GET", full_url, response.text)
        except Exception as e:
            fail_count += 1
            log_request("GET", full_url, error=str(e))
    
    if fail_count == 0:
        update_status_label.configure(text=f"✓ Update sent to {success_count} robot(s)", text_color="green")
    elif success_count == 0:
        update_status_label.configure(text=f"✗ All {fail_count} updates failed", text_color="red")
    else:
        update_status_label.configure(text=f"⚠ {success_count} success, {fail_count} failed", text_color="orange")
    # ================================================

# ---------- Run GUI ----------
app.mainloop()