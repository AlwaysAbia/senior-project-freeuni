import customtkinter as ctk
import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime
from robot_hostnames import robots

# ---------- Configuration ----------
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
ROBOT_HOSTNAMES = robots  # Import from robot_hostnames.py

# ---------- Appearance ----------
ctk.set_appearance_mode("System")
ctk.set_default_color_theme("blue")

# ---------- App Window ----------
app = ctk.CTk()
app.geometry("1200x700")
app.title("Rover Swarm Control Panel - MQTT")

# ---------- Layout ----------
app.grid_columnconfigure((0, 1, 2), weight=1)
app.grid_rowconfigure(1, weight=1)
app.grid_rowconfigure(2, weight=0)

# ---------- Header ----------
header_frame = ctk.CTkFrame(app)
header_frame.grid(row=0, column=0, columnspan=3, padx=20, pady=20, sticky="ew")
header_frame.grid_columnconfigure(1, weight=1)

ctk.CTkLabel(header_frame, text="Select Robot:", font=ctk.CTkFont(size=16, weight="bold")).grid(row=0, column=0, padx=20, pady=15, sticky="w")

# Create dropdown values dynamically
robot_numbers = [str(i+1) for i in range(len(ROBOT_HOSTNAMES))]
robot_dropdown = ctk.CTkOptionMenu(header_frame, values=robot_numbers, width=120)
robot_dropdown.grid(row=0, column=1, padx=20, pady=15, sticky="w")
robot_dropdown.set("1")

# Broadcast checkbox
broadcast_checkbox = ctk.CTkCheckBox(header_frame, text="Broadcast to All Robots", font=ctk.CTkFont(size=14, weight="bold"))
broadcast_checkbox.grid(row=0, column=2, padx=20, pady=15, sticky="w")

# MQTT Connection Status Indicator
mqtt_status_label = ctk.CTkLabel(header_frame, text="● MQTT Disconnected", font=ctk.CTkFont(size=12), text_color="red")
mqtt_status_label.grid(row=0, column=3, padx=20, pady=15, sticky="e")

def update_mqtt_status_indicator():
    if gui_mqtt_connected:
        mqtt_status_label.configure(text="● MQTT Connected", text_color="green")
    else:
        mqtt_status_label.configure(text="● MQTT Disconnected", text_color="red")

# ---------- Robot Status Windows ----------
status_frames = {}
status_labels = {}
status_textboxes = {}

for idx, hostname in enumerate(ROBOT_HOSTNAMES):
    frame = ctk.CTkFrame(app)
    frame.grid(row=1, column=idx, padx=10, pady=10, sticky="nsew")
    
    # Header with connection status
    header = ctk.CTkLabel(frame, text=f"Robot {idx+1}", font=ctk.CTkFont(size=14, weight="bold"))
    header.pack(padx=10, pady=(10, 5))
    
    status_label = ctk.CTkLabel(frame, text="● Disconnected", font=ctk.CTkFont(size=11), text_color="red")
    status_label.pack(padx=10, pady=(0, 5))
    
    # Status textbox
    textbox = ctk.CTkTextbox(frame, width=350, height=400, font=ctk.CTkFont(family="Courier", size=10))
    textbox.pack(padx=10, pady=10, fill="both", expand=True)
    textbox.insert("1.0", "Waiting for data...")
    textbox.configure(state="disabled")
    
    status_frames[hostname] = frame
    status_labels[hostname] = status_label
    status_textboxes[hostname] = textbox

def update_status_display(hostname):
    if hostname not in status_textboxes:
        return
    
    data = robot_status[hostname]["data"]
    last_update = robot_status[hostname]["last_update"]
    
    # Calculate time since last update
    time_diff = time.time() - last_update
    
    # Update connection indicator
    if time_diff < 2:
        status_labels[hostname].configure(text="● Connected", text_color="green")
    elif time_diff < 5:
        status_labels[hostname].configure(text="● Stale", text_color="yellow")
    else:
        status_labels[hostname].configure(text="● Disconnected", text_color="red")
    
    # Format JSON nicely
    formatted_json = json.dumps(data, indent=2)
    
    # Update textbox
    textbox = status_textboxes[hostname]
    textbox.configure(state="normal")
    textbox.delete("1.0", "end")
    textbox.insert("1.0", formatted_json)
    textbox.configure(state="disabled")

# Periodic update to check stale connections
def check_connection_status():
    for hostname in ROBOT_HOSTNAMES:
        if hostname in robot_status and robot_status[hostname]["last_update"] > 0:
            update_status_display(hostname)
    app.after(1000, check_connection_status)

# ---------- MQTT Client Setup ----------
mqtt_client = mqtt.Client()
robot_status = {}

# Initialize robot_status dynamically based on imported robots list
for hostname in ROBOT_HOSTNAMES:
    robot_status[hostname] = {"data": {}, "last_update": 0, "connected": False}

gui_mqtt_connected = False

# ---------- MQTT Callbacks ----------
def on_connect(client, userdata, flags, rc):
    global gui_mqtt_connected
    if rc == 0:
        print("Connected to MQTT Broker!")
        gui_mqtt_connected = True
        
        # Subscribe to all robot status topics
        # Remove .local suffix if present for MQTT topics
        for hostname in ROBOT_HOSTNAMES:
            # Extract base hostname (remove .local if present)
            base_hostname = hostname.replace('.local', '')
            topic = f"telemetry/{base_hostname}/status"
            client.subscribe(topic)
            print(f"Subscribed to {topic}")
        
        update_mqtt_status_indicator()
    else:
        print(f"Failed to connect, return code {rc}")
        gui_mqtt_connected = False

def on_disconnect(client, userdata, rc):
    global gui_mqtt_connected
    gui_mqtt_connected = False
    print("Disconnected from MQTT Broker")
    update_mqtt_status_indicator()

def on_message(client, userdata, msg):
    try:
        # Parse topic to get robot hostname
        topic_parts = msg.topic.split('/')
        if len(topic_parts) == 3 and topic_parts[0] == "telemetry":
            base_hostname = topic_parts[1]
            
            # Find matching hostname in ROBOT_HOSTNAMES (with or without .local)
            matched_hostname = None
            for hostname in ROBOT_HOSTNAMES:
                if hostname == base_hostname or hostname == f"{base_hostname}.local":
                    matched_hostname = hostname
                    break
            
            if not matched_hostname:
                print(f"Unknown hostname: {base_hostname}")
                return
            
            # Parse JSON payload
            data = json.loads(msg.payload.decode())
            
            # Update robot status
            robot_status[matched_hostname]["data"] = data
            robot_status[matched_hostname]["last_update"] = time.time()
            robot_status[matched_hostname]["connected"] = True
            
            # Update GUI
            update_status_display(matched_hostname)
    except Exception as e:
        print(f"Error processing message: {e}")

mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message

# Connect to broker
try:
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()
except Exception as e:
    print(f"Failed to connect to MQTT broker: {e}")

# Start connection check after everything is initialized
check_connection_status()

# ---------- Control Panel Row ----------
control_frame = ctk.CTkFrame(app)
control_frame.grid(row=2, column=0, columnspan=3, padx=10, pady=10, sticky="ew")
control_frame.grid_columnconfigure((0, 1), weight=1)

# ---------- MANUAL MOVE ----------
move_frame = ctk.CTkFrame(control_frame)
move_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

ctk.CTkLabel(move_frame, text="Manual Move", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10, 5))

left_entry = ctk.CTkEntry(move_frame, placeholder_text="Left (l)", width=180)
left_entry.pack(anchor="w", padx=20, pady=2)
right_entry = ctk.CTkEntry(move_frame, placeholder_text="Right (r)", width=180)
right_entry.pack(anchor="w", padx=20, pady=2)
back_entry = ctk.CTkEntry(move_frame, placeholder_text="Back (b)", width=180)
back_entry.pack(anchor="w", padx=20, pady=2)

move_status_label = ctk.CTkLabel(move_frame, text="")
move_status_label.pack(anchor="w", padx=15, pady=(0, 5))

def send_move_command():
    payload = {"mode": "MANUAL"}
    
    if left_entry.get():
        payload["l"] = int(left_entry.get())
    if right_entry.get():
        payload["r"] = int(right_entry.get())
    if back_entry.get():
        payload["b"] = int(back_entry.get())
    
    # Publish command
    target_robots = get_target_robots()
    success_count = 0
    
    for robot_idx in target_robots:
        hostname = ROBOT_HOSTNAMES[robot_idx]
        # Remove .local for MQTT topic
        base_hostname = hostname.replace('.local', '')
        topic = "command/broadcast" if broadcast_checkbox.get() else f"command/individual/{base_hostname}"
        
        try:
            result = mqtt_client.publish(topic, json.dumps(payload), qos=1)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                success_count += 1
                print(f"Sent move command to {topic}")
            else:
                print(f"Failed to send to {topic}")
        except Exception as e:
            print(f"Error sending move: {e}")
        
        if broadcast_checkbox.get():
            break  # Only send once for broadcast
    
    if success_count > 0:
        move_status_label.configure(text=f"✓ Move sent to {success_count} robot(s)", text_color="green")
    else:
        move_status_label.configure(text="✗ Move failed", text_color="red")

ctk.CTkButton(move_frame, text="Send Move", command=send_move_command, width=180).pack(anchor="w", padx=15, pady=10)

# ---------- UPDATE STATE ----------
state_frame = ctk.CTkFrame(control_frame)
state_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
state_frame.grid_columnconfigure((0, 1, 2, 3), weight=1)

ctk.CTkLabel(state_frame, text="Update State", font=ctk.CTkFont(size=14, weight="bold")).grid(row=0, column=0, columnspan=4, pady=(10, 15))

# IDLE
ctk.CTkLabel(state_frame, text="IDLE Mode", font=ctk.CTkFont(size=11, weight="bold")).grid(row=1, column=0, pady=(0, 5))
idle_thresh_entry = ctk.CTkEntry(state_frame, placeholder_text="Idle Threshold", width=150)
idle_thresh_entry.grid(row=2, column=0, pady=2, padx=5)

# LINE
ctk.CTkLabel(state_frame, text="LINE Mode", font=ctk.CTkFont(size=11, weight="bold")).grid(row=1, column=1, pady=(0, 5))
line_dist_entry = ctk.CTkEntry(state_frame, placeholder_text="Line Distance", width=150)
line_dist_entry.grid(row=2, column=1, pady=2, padx=5)
line_alignTol_entry = ctk.CTkEntry(state_frame, placeholder_text="Line Align Tol", width=150)
line_alignTol_entry.grid(row=3, column=1, pady=2, padx=5)

# POLYGON
ctk.CTkLabel(state_frame, text="POLYGON Mode", font=ctk.CTkFont(size=11, weight="bold")).grid(row=1, column=2, pady=(0, 5))
polygon_radius_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Radius", width=150)
polygon_radius_entry.grid(row=2, column=2, pady=2, padx=5)
polygon_sides_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Sides", width=150)
polygon_sides_entry.grid(row=3, column=2, pady=2, padx=5)
polygon_alignTol_entry = ctk.CTkEntry(state_frame, placeholder_text="Polygon Align Tol", width=150)
polygon_alignTol_entry.grid(row=4, column=2, pady=2, padx=5)

# GENERAL
ctk.CTkLabel(state_frame, text="General", font=ctk.CTkFont(size=11, weight="bold")).grid(row=1, column=3, pady=(0, 5))
neighbor_maxDist_entry = ctk.CTkEntry(state_frame, placeholder_text="Neighbor Max Dist", width=150)
neighbor_maxDist_entry.grid(row=2, column=3, pady=2, padx=5)

# Mode dropdown and button
state_dropdown = ctk.CTkOptionMenu(state_frame, values=["OFF", "IDLE", "LINE", "POLYGON", "MANUAL"], width=200)
state_dropdown.grid(row=5, column=0, columnspan=2, pady=(10, 5), padx=5, sticky="ew")
state_dropdown.set("OFF")

ctk.CTkButton(state_frame, text="Send Update", command=lambda: send_update_state(), width=200).grid(row=5, column=2, columnspan=2, pady=(10, 5), padx=5, sticky="ew")

update_status_label = ctk.CTkLabel(state_frame, text="")
update_status_label.grid(row=6, column=0, columnspan=4, pady=(0, 10))

# ---------- Helper Functions ----------
def get_target_robots():
    if broadcast_checkbox.get():
        return list(range(len(ROBOT_HOSTNAMES)))
    else:
        return [int(robot_dropdown.get()) - 1]

def send_update_state():
    state = state_dropdown.get()
    payload = {"mode": state}
    
    # General
    if neighbor_maxDist_entry.get():
        payload["neighbor_maxDist"] = int(neighbor_maxDist_entry.get())
    
    # IDLE
    if state == "IDLE" and idle_thresh_entry.get():
        payload["idle_thresh"] = int(idle_thresh_entry.get())
    
    # LINE
    if state == "LINE":
        if line_dist_entry.get():
            payload["line_nodeDist"] = int(line_dist_entry.get())
        if line_alignTol_entry.get():
            payload["line_alignTol"] = int(line_alignTol_entry.get())
    
    # POLYGON
    if state == "POLYGON":
        if polygon_radius_entry.get():
            payload["polygon_radius"] = int(polygon_radius_entry.get())
        if polygon_sides_entry.get():
            payload["polygon_sides"] = int(polygon_sides_entry.get())
        if polygon_alignTol_entry.get():
            payload["polygon_alignTol"] = int(polygon_alignTol_entry.get())
    
    # Publish command
    target_robots = get_target_robots()
    success_count = 0
    
    for robot_idx in target_robots:
        hostname = ROBOT_HOSTNAMES[robot_idx]
        # Remove .local for MQTT topic
        base_hostname = hostname.replace('.local', '')
        topic = "command/broadcast" if broadcast_checkbox.get() else f"command/individual/{base_hostname}"
        
        try:
            result = mqtt_client.publish(topic, json.dumps(payload), qos=1)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                success_count += 1
                print(f"Sent update to {topic}: {payload}")
            else:
                print(f"Failed to send to {topic}")
        except Exception as e:
            print(f"Error sending update: {e}")
        
        if broadcast_checkbox.get():
            break  # Only send once for broadcast
    
    if success_count > 0:
        update_status_label.configure(text=f"✓ Update sent to {success_count} robot(s)", text_color="green")
    else:
        update_status_label.configure(text="✗ Update failed", text_color="red")

# ---------- Cleanup on Close ----------
def on_closing():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    app.destroy()

app.protocol("WM_DELETE_WINDOW", on_closing)

# ---------- Run GUI ----------
app.mainloop()