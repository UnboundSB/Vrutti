import os
import json
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

# --- THE WINDOWS BYPASS ---
msys2_bin_path = r"C:\msys64\ucrt64\bin"
if hasattr(os, 'add_dll_directory') and os.path.exists(msys2_bin_path):
    os.add_dll_directory(msys2_bin_path)

import vrutti_core

# --- APP INITIALIZATION ---
app = FastAPI(title="Vrutti Backend Engine")
document = vrutti_core.PieceTable("")

# --- THE WEBSOCKET ENDPOINT ---
@app.websocket("/ws/editor")
async def editor_websocket(websocket: WebSocket):
    
    # 1. OPEN THE DOOR (The handshake)
    await websocket.accept()
    
    # 2. Send the initial text to the client
    await websocket.send_text(document.get_text())
    
    try:
        # 3. KEEP THE PIPE OPEN
        while True:
            # Wait for the user to type
            data = await websocket.receive_text()
            
            # Parse the JSON string from JavaScript into a Python dictionary
            payload = json.loads(data)
            
            # Feed the C++ Brain and respond
            if "index" in payload and "text" in payload:
                
                # Insert the text into C++ memory
                document.insert_text(payload["index"], payload["text"])
                
                # Instantly send the updated text back to the browser
                await websocket.send_text(document.get_text())
                
    except WebSocketDisconnect:
        print("Frontend disconnected.")
    except Exception as e:
        print(f"Engine Error: {e}")

# --- SERVER LAUNCHER ---
if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)