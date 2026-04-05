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
document = vrutti_core.PieceTable("Welcome to Vrutti. Type anywhere...\n")


# --- THE WEBSOCKET ENDPOINT ---
@app.websocket("/ws/editor")
async def editor_websocket(websocket: WebSocket):
    
    # STEP 1: Accept the incoming connection from the frontend
    # Python Hint: await websocket.accept()
    
    
    # STEP 2: Send the initial text to the client so they can see the document
    # Python Hint: await websocket.send_text(document.get_text())
    
    
    try:
        # We use an infinite loop to keep the pipe open forever
        while True:
            
            # STEP 3: Wait for a message from the frontend
            # Python Hint: data = await websocket.receive_text()
            data = await websocket.receive_text()
            
            # STEP 4: Parse the incoming JSON string into a Python dictionary
            # Python Hint: payload = json.loads(data)
            payload = json.loads(data)
            
            # STEP 5: Feed the C++ Brain and respond!
            if "index" in payload and "text" in payload:
                
                # A: Call document.insert_text() using the payload's "index" and "text"
                document.insert_text(payload.get_index(), payload.get_text)
                
                # B: Send the updated document.get_text() back down the websocket
                await websocket.send_text(document.get_text())
                
    except WebSocketDisconnect:
        print("Frontend disconnected.")

# --- SERVER LAUNCHER ---
if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)