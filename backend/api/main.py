from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import json
import os
import uvicorn

# --- THE WINDOWS DLL FIX ---
# Force Python 3.8+ to trust the MSYS2 compiler folder so it can load C++ dependencies
if os.name == 'nt':
    try:
        os.add_dll_directory(r"C:\msys64\ucrt64\bin")
    except Exception as e:
        print(f"Warning: Could not add MSYS2 path: {e}")

# NOW we can safely import your C++ engine!
import vrutti_core

app = FastAPI()

# 1. Initialize the C++ Engine
document = vrutti_core.PieceTable("")

@app.websocket("/ws/editor")
async def editor_endpoint(websocket: WebSocket):
    await websocket.accept()
    print("Browser connected to Vrutti Engine.")

    # 2. THE REFRESH FIX: 
    # Immediately send the current C++ state to the browser so they match perfectly!
    await websocket.send_text(document.get_text())

    try:
        while True:
            # Wait for the JSON payload from the ChatGPT frontend
            data = await websocket.receive_text()
            payload = json.loads(data)
            
            action = payload.get("action")
            
            # 3. ROUTER: Bind the actions directly to your C++ methods
            if action == "insert":
                document.insert_text(payload["index"], payload["text"])
                
            elif action == "delete":
                document.delete_text(payload["index"], payload["length"])
            
            # 4. Blast the finalized text back to the browser to ensure absolute sync
            await websocket.send_text(document.get_text())

    except WebSocketDisconnect:
        print("Browser disconnected.")
# --- SERVER LAUNCHER ---
if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)