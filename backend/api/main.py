from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import json
import vrutti_core

app = FastAPI()

# 1. Initialize the C++ Engine (Starts completely empty)
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