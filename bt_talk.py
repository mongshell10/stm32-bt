# 블루투스 대화 — 보드와 값 주고받기 (인천 전자 마이스터고 펌웨어 동아리)
# 더블클릭으로 실행.  안 열리면 IDLE 로 열고 F5.   serial 폴더가 같은 폴더에 있어야 한다.
import tkinter as tk
from tkinter import ttk
import serial                               # ① 시리얼(블루투스 · USB) 도구
from serial.tools import list_ports

s = None                                    # 열린 포트 (처음에는 없음)


def ports():                                # 노트북에 있는 COM 포트 목록
    return [f"{p.device}  {p.description}" for p in list_ports.comports()]


def connect():                              # [연결] 버튼
    global s
    if s:                                   # 이미 열려 있으면 → 끊기
        s.close(); s = None
        btn.config(text="연결"); show("--- 끊음")
        return
    name = port_box.get().split()[0]        # "COM7  표준 직렬 ..." → "COM7"
    speed = 115200 if "STLink" in port_box.get() else 9600
    try:
        s = serial.Serial(name, speed, timeout=0)    # ② 포트 열기
    except Exception as e:
        show("--- 못 열었음: " + str(e)); return
    btn.config(text="끊기"); show(f"--- {name} 연결 ({speed})")


def send(event=None):                       # [보내기] 버튼 · Enter
    msg = entry.get()
    if s and msg:
        s.write((msg + "\n").encode())      # ③ 보내기 — 끝에 \n (= Enter)
        show("나 > " + msg)
        entry.delete(0, "end")


buf = b""
def receive():                              # 0.1초마다 받은 글자 확인
    global buf
    if s and s.in_waiting:
        buf += s.read(s.in_waiting)         # ④ 받기
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            show("보드 > " + line.decode(errors="ignore").strip())
    win.after(100, receive)


def show(text):                             # 받은 글자 칸에 한 줄 더하기
    log.insert("end", text + "\n"); log.see("end")


# ---------------- 화면 ----------------
win = tk.Tk(); win.title("블루투스 대화 — 보드와 값 주고받기")
top = tk.Frame(win); top.pack(fill="x", padx=8, pady=6)
tk.Label(top, text="포트").pack(side="left")
port_box = ttk.Combobox(top, values=ports(), width=48); port_box.pack(side="left", padx=4)
tk.Button(top, text="새로고침", command=lambda: port_box.config(values=ports())).pack(side="left")
btn = tk.Button(top, text="연결", width=8, command=connect); btn.pack(side="left", padx=4)

log = tk.Text(win, width=70, height=18, font=("Consolas", 11)); log.pack(padx=8)

bottom = tk.Frame(win); bottom.pack(fill="x", padx=8, pady=6)
entry = tk.Entry(bottom, width=40, font=("Consolas", 12)); entry.pack(side="left")
entry.bind("<Return>", send)
tk.Button(bottom, text="보내기", command=send).pack(side="left", padx=4)
tk.Label(bottom, text="영어 · 숫자 12글자까지 → LCD 아랫줄").pack(side="left")

receive()
win.mainloop()
