from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    # 接受客户端的WebSocket连接
    await websocket.accept()
    try:
        while True:
            # 等待接收客户端发送的消息
            data = await websocket.receive_text()
            # 立即将接收到的消息发送回去
            await websocket.send_text(f"服务器收到: {data}")
    except WebSocketDisconnect:
        # 处理客户端断开连接的情况
        print("客户端已断开连接")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
    