#!/usr/bin/env python3
"""ImageStreamer 接收端 —— 在 Ubuntu 上运行，接收树莓派的实时画面

用法:
    python3 stream_receiver.py           # 默认端口 8080
    python3 stream_receiver.py 9999      # 自定义端口
"""

import socket
import cv2
import numpy as np
import sys


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", port))
    s.listen(1)
    print(f"[Receiver] 监听端口 {port}，等待树莓派连接...")

    print("[Receiver] 按 Ctrl+C 退出")

    while True:                         # 外层循环：允许断连后重连
        print("[Receiver] 等待连接...")
        conn, addr = s.accept()
        print(f"[Receiver] 已连接: {addr[0]}:{addr[1]}")
        print("[Receiver] 按 ESC 退出")

        try:
            while True:
                # 读 4 字节头
                header = recv_all(conn, 4)
                if not header:
                    print("[Receiver] 连接断开，等待重连...")
                    break
                size = int.from_bytes(header, "big")

                # 读 JPEG 数据
                data = recv_all(conn, size)
                if not data:
                    print("[Receiver] 连接断开，等待重连...")
                    break

                # 解码显示
                frame = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
                if frame is not None:
                    cv2.imshow("LightDetect - Remote View", frame)

                if cv2.waitKey(1) & 0xFF == 27:
                    print("[Receiver] 用户退出")
                    cv2.destroyAllWindows()
                    return                 # ESC 真正退出
        except Exception as e:
            print(f"[Receiver] 连接异常: {e}，等待重连...")
        finally:
            conn.close()


def recv_all(sock, n):
    """确保收满 n 字节（TCP 流式协议需要循环读取）"""
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


if __name__ == "__main__":
    main()
