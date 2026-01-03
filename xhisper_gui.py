#!/usr/bin/env python3
"""
xhisper GUI - Single button to start/stop recording and transcribe
"""

import subprocess
import sys
import tkinter as tk
from tkinter import font as tkFont

IS_RECORDING = False

def toggle_recording():
    global IS_RECORDING
    
    if IS_RECORDING:
        button.config(bg='#4CAF50', text='STOPPED\n\nClick to START', fg='white')
        button.config(font=('Arial', 20, 'bold'))
        IS_RECORDING = False
        subprocess.Popen(['xhisper'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        button.config(bg='#f44336', text='RECORDING...\n\nSPEAK NOW', fg='white')
        button.config(font=('Arial', 20, 'bold'))
        IS_RECORDING = True
        subprocess.Popen(['xhisper'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# Create window
root = tk.Tk()
root.title('xhisper - Greek Dictation')
root.geometry('400x300')
root.config(bg='#222222')

# Title
title = tk.Label(root, text='xhisper', font=('Arial', 32, 'bold'), fg='#4CAF50', bg='#222222')
title.pack(pady=20)

# Button
button = tk.Button(
    root, 
    text='STOPPED\n\nClick to START',
    font=('Arial', 20, 'bold'),
    bg='#4CAF50',
    fg='white',
    width=20,
    height=5,
    command=toggle_recording,
    activebackground='#45a049',
    activeforeground='white',
    relief='raised',
    bd=2
)
button.pack(pady=20, padx=20, fill=tk.BOTH, expand=True)

# Status label
status = tk.Label(root, text='Ready', font=('Arial', 12), fg='#4CAF50', bg='#222222')
status.pack(pady=10)

root.mainloop()
